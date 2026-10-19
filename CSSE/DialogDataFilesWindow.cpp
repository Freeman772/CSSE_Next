#include "DialogDataFilesWindow.h"

#include "CSSE.h"
#include "CSDataHandler.h"
#include "CSRecordHandler.h"
#include "CSGameFile.h"
#include "DarkMode.h"
#include "LogUtil.h"
#include "PathUtil.h"
#include "PluginLoadOrder.h"

namespace se::cs::dialog::data_files_window {
	constexpr UINT_PTR SUBCLASS_ID = 0x43534446;
	constexpr UINT_PTR DRAG_TIMER = 0x43534447;
	constexpr int GROUP_VISIBLE = 0x435301;
	constexpr int GROUP_HIDDEN = 0x435302;
	constexpr int ID_FILTER = 62001;
	constexpr int ID_CLEAR = 62002;
	constexpr int ID_CHECK = 62003;
	constexpr int ID_UNCHECK = 62004;
	constexpr int ID_UP = 62005;
	constexpr int ID_DOWN = 62006;
	constexpr char CONTROLLER_PROPERTY[] = "CSSE:DataFilesController";
	static HHOOK initializationHook = nullptr;

	class DataFilesController {
		struct NativeEntry {
			LPARAM payload = 0;
			GameFile* file = nullptr;
			std::filesystem::path path;
			std::wstring search;
			bool visible = true;
		};

		struct OpenFile {
			HANDLE handle;
			size_t entry;
			FILETIME before;
			OpenFile(HANDLE value, size_t id, FILETIME time) : handle(value), entry(id), before(time) {}
			OpenFile(const OpenFile&) = delete;
			OpenFile& operator=(const OpenFile&) = delete;
			OpenFile(OpenFile&& other) noexcept : handle(other.handle), entry(other.entry), before(other.before) { other.handle = INVALID_HANDLE_VALUE; }
			~OpenFile() { if (handle != INVALID_HANDLE_VALUE) { CloseHandle(handle); } }
		};

	public:
		explicit DataFilesController(HWND window) : m_Window(window), m_List(GetDlgItem(window, CONTROL_ID_DATA_FILES_LIST)) {}

		static void attach(HWND window) {
			if (GetPropA(window, CONTROLLER_PROPERTY)) { return; }
			auto controller = std::make_unique<DataFilesController>(window);
			if (!controller->captureFiles()) { return; }
			if (!SetPropA(window, CONTROLLER_PROPERTY, controller.get())) { return; }
			if (!SetWindowSubclass(window, dialogProc, SUBCLASS_ID, reinterpret_cast<DWORD_PTR>(controller.get()))) {
				RemovePropA(window, CONTROLLER_PROPERTY); return;
			}
			// The dialog owns the controller after subclass installation.
			const auto instance = controller.release();
			instance->createInterface();
			log::stream << "Data Files: filter, multiple selection and load-order controls attached (" << instance->m_Entries.size() << " files)." << std::endl;
		}

	private:
		static std::uint64_t ticks(const FILETIME& value) {
			return (static_cast<std::uint64_t>(value.dwHighDateTime) << 32) | value.dwLowDateTime;
		}
		static FILETIME fileTime(std::uint64_t value) {
			return { static_cast<DWORD>(value), static_cast<DWORD>(value >> 32) };
		}
		static std::wstring wide(const std::string& value, UINT page = CP_ACP) {
			if (value.empty()) { return {}; }
			const auto count = MultiByteToWideChar(page, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
			std::wstring result(count, L'\0');
			MultiByteToWideChar(page, 0, value.data(), static_cast<int>(value.size()), result.data(), count);
			return result;
		}
		static std::string utf8(const std::wstring& value) {
			if (value.empty()) { return {}; }
			const auto count = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
			std::string result(count, '\0');
			WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), count, nullptr, nullptr);
			return result;
		}
		static std::wstring lower(std::wstring value) {
			if (!value.empty()) { CharLowerBuffW(value.data(), static_cast<DWORD>(value.size())); }
			return value;
		}
		static std::string json(const std::string& value) {
			std::string result = "\"";
			for (const unsigned char c : value) {
				if (c == '\\' || c == '"') { result += '\\'; result += static_cast<char>(c); }
				else if (c < 32) { result += fmt::format("\\u{:04x}", c); }
				else { result += static_cast<char>(c); }
			}
			return result + '"';
		}
		std::wstring itemText(int row, int column) const {
			wchar_t buffer[2048] = {};
			LVITEMW item = {};
			item.iSubItem = column; item.pszText = buffer; item.cchTextMax = 2048;
			SendMessageW(m_List, LVM_GETITEMTEXTW, row, reinterpret_cast<LPARAM>(&item));
			return buffer;
		}
		std::optional<size_t> entryForRow(int row) const {
			if (row < 0) { return {}; }
			LVITEMA item = {}; item.mask = LVIF_PARAM; item.iItem = row;
			if (!ListView_GetItem(m_List, &item)) { return {}; }
			const auto found = m_ByPayload.find(item.lParam);
			return found != m_ByPayload.end() ? std::optional<size_t>{found->second} : std::nullopt;
		}
		int rowForEntry(size_t entry) const {
			LVFINDINFOA find = {}; find.flags = LVFI_PARAM; find.lParam = m_Entries[entry].payload;
			return ListView_FindItem(m_List, -1, &find);
		}
		std::vector<size_t> selection() const {
			std::vector<size_t> result;
			for (int row = ListView_GetNextItem(m_List, -1, LVNI_SELECTED); row >= 0; row = ListView_GetNextItem(m_List, row, LVNI_SELECTED)) {
				if (const auto entry = entryForRow(row); entry && m_Entries[*entry].visible) { result.push_back(*entry); }
			}
			return result;
		}
		bool captureFiles() {
			if (!m_List || (GetWindowLongA(m_List, GWL_STYLE) & LVS_OWNERDATA)) { return false; }
			const auto dataHandler = DataHandler::get();
			m_Handler = dataHandler ? dataHandler->recordHandler : nullptr;
			if (!m_Handler || !m_Handler->availableDataFiles) { return false; }
			m_Available = m_Handler->availableDataFiles;
			std::vector<GameFile*> available;
			std::unordered_map<std::wstring, GameFile*> byName;
			for (auto node = m_Available->head; node && node != m_Available->sentinel() && available.size() < m_Available->size(); node = node->next) {
				available.push_back(node->data);
				if (node->data) { byName[lower(wide(node->data->fileName))] = node->data; }
			}
			const auto count = ListView_GetItemCount(m_List);
			std::vector<plugins::PluginEntry> model;
			m_PointerPayloads = m_IndexPayloads = true;
			const auto columns = std::min(16, Header_GetItemCount(ListView_GetHeader(m_List)));
			for (int row = 0; row < count; ++row) {
				LVITEMA item = {}; item.mask = LVIF_PARAM | LVIF_IMAGE; item.iItem = row;
				if (!ListView_GetItem(m_List, &item) || m_ByPayload.contains(item.lParam)) { return false; }
				auto filename = itemText(row, 0);
				GameFile* file = nullptr;
				for (const auto candidate : available) {
					if (reinterpret_cast<LPARAM>(candidate) == item.lParam) { file = candidate; break; }
				}
				if (!file) {
					const auto match = byName.find(lower(filename));
					if (match != byName.end()) { file = match->second; }
				}
				if (file) { filename = wide(file->fileName); }
				if (file && item.iImage >= 0) {
					const bool enabled = file->getToLoadFlag();
					const auto [known, inserted] = m_CheckedImages.emplace(item.iImage, enabled);
					if (!inserted && known->second && *known->second != enabled) { known->second.reset(); }
				}
				NativeEntry entry;
				entry.payload = item.lParam; entry.file = file;
				entry.path = path::getDataFilesPath() / filename;
				entry.search = filename;
				for (int column = 1; column < columns; ++column) { entry.search += L" " + itemText(row, column); }
				entry.search = lower(entry.search);
				m_PointerPayloads = m_PointerPayloads && file && item.lParam == reinterpret_cast<LPARAM>(file);
				m_IndexPayloads = m_IndexPayloads && file && item.lParam >= 0 && static_cast<size_t>(item.lParam) < available.size()
					&& available[static_cast<size_t>(item.lParam)] == file;
				FILETIME modified = {};
				if (file) { modified = file->findFileData.ftLastWriteTime; }
				else {
					WIN32_FILE_ATTRIBUTE_DATA info = {};
					if (GetFileAttributesExW(entry.path.c_str(), GetFileExInfoStandard, &info)) { modified = info.ftLastWriteTime; }
				}
				const auto name = lower(filename);
				const bool pinned = name == L"morrowind.esm" || name == L"tribunal.esm" || name == L"bloodmoon.esm";
				const bool master = file ? file->getIsMasterFile() : lower(entry.path.extension().wstring()) == L".esm";
				model.push_back({utf8(name), master, pinned, ticks(modified), {}});
				m_ByPayload[item.lParam] = m_Entries.size();
				m_Entries.push_back(std::move(entry));
			}
			m_CanReorder = (m_PointerPayloads || m_IndexPayloads) && available.size() == m_Entries.size();
			m_Order = std::make_unique<plugins::LoadOrder>(std::move(model));
			return true;
		}
		HWND control(DWORD style, const wchar_t* type, const wchar_t* title, int id, int x, int y, int width, int height, DWORD extended = 0) {
			const auto result = CreateWindowExW(extended, type, title, WS_CHILD | WS_VISIBLE | style, x, y, width, height,
				m_Window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), application.m_hInstance, nullptr);
			if (!result) { throw std::runtime_error("Could not create the Data Files controls."); }
			SendMessageW(result, WM_SETFONT, SendMessageW(m_List, WM_GETFONT, 0, 0), FALSE);
			return result;
		}
		// Keep the stock checkbox glyphs: the dark-mode subclass already paints
		// them correctly, with no dependency on a Unicode symbol font.
		void addCheckTooltips(HWND check, HWND uncheck) {
			const auto tooltip = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr,
				WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP, CW_USEDEFAULT, CW_USEDEFAULT,
				CW_USEDEFAULT, CW_USEDEFAULT, m_Window, nullptr, application.m_hInstance, nullptr);
			if (!tooltip) { return; } // Cosmetic only, activation must still work.
			for (const auto& item : { std::pair<HWND, const wchar_t*>{check, L"Enable selected plugins"},
				std::pair<HWND, const wchar_t*>{uncheck, L"Disable selected plugins"} }) {
				TOOLINFOW info = {};
				info.cbSize = sizeof(info);
				info.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
				info.hwnd = m_Window;
				info.uId = reinterpret_cast<UINT_PTR>(item.first);
				info.lpszText = const_cast<LPWSTR>(item.second);
				SendMessageW(tooltip, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&info));
			}
		}
		static bool isNativePluginCaption(HWND window) {
			wchar_t type[32] = {}, caption[64] = {};
			if (!GetClassNameW(window, type, 32) || _wcsicmp(type, L"Static") != 0) { return false; }
			GetWindowTextW(window, caption, 64);
			return _wcsnicmp(caption, L"TES3 Files", 10) == 0;
		}
		void createInterface() {
			RECT listRect = {}; GetWindowRect(m_List, &listRect); MapWindowPoints(nullptr, m_Window, reinterpret_cast<POINT*>(&listRect), 2);
			const auto dc = GetDC(m_List);
			const int dpi = GetDeviceCaps(dc, LOGPIXELSX); ReleaseDC(m_List, dc);
			const int gap = MulDiv(5, dpi, 96);
			const int height = MulDiv(23, dpi, 96);
			const int extra = 2 * height + 3 * gap;
			struct Child { HWND window; RECT rect; };
			std::vector<Child> children;
			for (auto child = GetWindow(m_Window, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT)) {
				RECT rect = {}; GetWindowRect(child, &rect); MapWindowPoints(nullptr, m_Window, reinterpret_cast<POINT*>(&rect), 2);
				children.push_back({child, rect});
			}
			RECT windowRect = {}; GetWindowRect(m_Window, &windowRect);
			MONITORINFO monitor = {sizeof(monitor)}; GetMonitorInfoW(MonitorFromWindow(m_Window, MONITOR_DEFAULTTONEAREST), &monitor);
			const int y = std::max(monitor.rcWork.top, std::min(windowRect.top, monitor.rcWork.bottom - (windowRect.bottom - windowRect.top + extra)));
			SetWindowPos(m_Window, nullptr, windowRect.left, y, windowRect.right-windowRect.left, windowRect.bottom-windowRect.top+extra, SWP_NOZORDER | SWP_NOACTIVATE);
			for (const auto& child : children) {
				// The original "TES3 Files" caption would overlap the added
				// selection controls. The list's column header already labels it.
				if (isNativePluginCaption(child.window)) { ShowWindow(child.window, SW_HIDE); continue; }
				SetWindowPos(child.window, nullptr, child.rect.left, child.rect.top+extra, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
			}
			const int left = listRect.left, top = listRect.top;
			const int width = listRect.right-listRect.left;
			const int label = MulDiv(42, dpi, 96), clear = MulDiv(55, dpi, 96);
			control(SS_CENTERIMAGE, L"STATIC", L"Filter:", 0, left, top, label-gap, height);
			m_Filter = control(WS_TABSTOP | ES_AUTOHSCROLL, L"EDIT", L"", ID_FILTER, left+label, top, width-label-clear-gap, height, WS_EX_CLIENTEDGE);
			SendMessageW(m_Filter, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"Plugin name or type (Ctrl+F)"));
			SendMessageW(m_Filter, EM_SETLIMITTEXT, 512, 0);
			control(WS_TABSTOP | BS_PUSHBUTTON, L"BUTTON", L"Clear", ID_CLEAR, listRect.right-clear, top, clear, height);
			const int row = top+height+gap;
			const int check = MulDiv(26, dpi, 96), arrow = MulDiv(27, dpi, 96);
			// Manual-state native checkboxes behave as momentary actions; clicking
			// them never flips the glyph. Existing selection logic stays intact.
			const auto checkedBox = control(WS_TABSTOP | BS_CHECKBOX, L"BUTTON", L"", ID_CHECK, left, row, check, height);
			const auto emptyBox = control(WS_TABSTOP | BS_CHECKBOX, L"BUTTON", L"", ID_UNCHECK, left+check+gap, row, check, height);
			SendMessageW(checkedBox, BM_SETCHECK, BST_CHECKED, 0);
			SendMessageW(emptyBox, BM_SETCHECK, BST_UNCHECKED, 0);
			addCheckTooltips(checkedBox, emptyBox);
			int x = left+2*(check+gap);
			control(WS_TABSTOP | BS_PUSHBUTTON, L"BUTTON", L"\u2191", ID_UP, x, row, arrow, height); x += arrow+gap;
			control(WS_TABSTOP | BS_PUSHBUTTON, L"BUTTON", L"\u2193", ID_DOWN, x, row, arrow, height); x += arrow+gap;
			m_Status = control(SS_CENTERIMAGE | SS_ENDELLIPSIS, L"STATIC", L"", 0, x, row, std::max(1, static_cast<int>(listRect.right)-x), height);
			SetWindowLongA(m_List, GWL_STYLE, GetWindowLongA(m_List, GWL_STYLE) & ~(LVS_SINGLESEL | LVS_SORTASCENDING | LVS_SORTDESCENDING));
			ListView_SetExtendedListViewStyleEx(m_List, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
			SetWindowSubclass(m_List, listProc, SUBCLASS_ID, reinterpret_cast<DWORD_PTR>(this));
			SetWindowSubclass(m_Filter, filterProc, SUBCLASS_ID, reinterpret_cast<DWORD_PTR>(this));
			for (const int id : {GROUP_VISIBLE, GROUP_HIDDEN}) {
				LVGROUP group = {}; group.cbSize = sizeof(group); group.mask = LVGF_GROUPID | LVGF_STATE;
				group.iGroupId = id; group.stateMask = LVGS_HIDDEN | LVGS_NOHEADER;
				group.state = LVGS_NOHEADER | (id == GROUP_HIDDEN ? LVGS_HIDDEN : 0);
				ListView_InsertGroup(m_List, -1, &group);
			}
			ListView_EnableGroupView(m_List, TRUE);
			m_GroupFiltering = ListView_IsGroupViewEnabled(m_List) != FALSE;
			sortToOrder(); applyFilter();
		}
		void updateStatus() const {
			if (!m_Status) { return; }
			const auto visible = std::count_if(m_Entries.begin(), m_Entries.end(), [](const NativeEntry& e) { return e.visible; });
			const auto text = std::to_wstring(visible) + L"/" + std::to_wstring(m_Entries.size()) + L"  |  "
				+ std::to_wstring(ListView_GetSelectedCount(m_List)) + L" selected" + (m_Order->changed() ? L"  |  order pending" : L"");
			SetWindowTextW(m_Status, text.c_str());
			// Retain native restrictions (for example, an ESM cannot be made active).
			if (ListView_GetSelectedCount(m_List) != 1) {
				EnableWindow(GetDlgItem(m_Window, CONTROL_ID_SET_AS_ACTIVE_BUTTON), FALSE);
				EnableWindow(GetDlgItem(m_Window, CONTROL_ID_MERGE_TO_MASTERS_BUTTON), FALSE);
			}
		}
		void applyFilter(bool showAll = false) {
			if (!m_Filter) { return; }
			wchar_t buffer[513] = {}; GetWindowTextW(m_Filter, buffer, 513);
			std::wistringstream terms(lower(buffer));
			std::vector<std::wstring> words;
			for (std::wstring term; terms >> term;) { words.push_back(std::move(term)); }
			SendMessageA(m_List, WM_SETREDRAW, FALSE, 0);
			m_Internal = true;
			for (int row = 0; row < ListView_GetItemCount(m_List); ++row) {
				const auto entry = entryForRow(row);
				if (!entry) { continue; }
				auto& file = m_Entries[*entry];
				file.visible = showAll || !m_GroupFiltering || std::all_of(words.begin(), words.end(), [&file](const auto& word) { return file.search.find(word) != std::wstring::npos; });
				if (!file.visible) { ListView_SetItemState(m_List, row, 0, LVIS_SELECTED | LVIS_FOCUSED); }
				if (m_GroupFiltering) {
					LVITEMA item = {}; item.mask = LVIF_GROUPID; item.iItem = row; item.iGroupId = file.visible ? GROUP_VISIBLE : GROUP_HIDDEN;
					ListView_SetItem(m_List, &item);
				}
			}
			m_Internal = false;
			SendMessageA(m_List, WM_SETREDRAW, TRUE, 0); InvalidateRect(m_List, nullptr, TRUE);
			updateStatus();
		}
		static int CALLBACK compareItems(LPARAM left, LPARAM right, LPARAM context) {
			const auto self = reinterpret_cast<DataFilesController*>(context);
			const auto a = self->m_ByPayload.find(left), b = self->m_ByPayload.find(right);
			if (a == self->m_ByPayload.end() || b == self->m_ByPayload.end()) { return 0; }
			return self->m_Rank[a->second] < self->m_Rank[b->second] ? -1 : self->m_Rank[a->second] != self->m_Rank[b->second];
		}
		void sortToOrder() {
			m_Rank.resize(m_Entries.size());
			for (size_t position = 0; position < m_Order->order().size(); ++position) { m_Rank[m_Order->order()[position]] = position; }
			ListView_SortItems(m_List, compareItems, reinterpret_cast<LPARAM>(this));
		}
		bool checked(size_t entry) const {
			const int row = rowForEntry(entry);
			LVITEMA item = {}; item.mask = LVIF_IMAGE | LVIF_STATE; item.stateMask = LVIS_STATEIMAGEMASK; item.iItem = row;
			if (!ListView_GetItem(m_List, &item)) { return false; }
			const auto state = (item.state & LVIS_STATEIMAGEMASK) >> 12;
			if (state) { return state != 1; }
			// Learn native image indices from the initial flags instead of assuming
			// the editor inserted its checkbox icons in a particular order.
			const auto known = m_CheckedImages.find(item.iImage);
			if (known != m_CheckedImages.end() && known->second) { return *known->second; }
			if (item.iImage == 0 || item.iImage == 1) { return item.iImage == 0; }
			return m_Entries[entry].file && m_Entries[entry].file->getToLoadFlag();
		}
		void learnToggle(size_t entry, int previousImage, bool wasChecked) {
			LVITEMA item = {}; item.mask = LVIF_IMAGE | LVIF_STATE; item.stateMask = LVIS_STATEIMAGEMASK; item.iItem = rowForEntry(entry);
			if (item.iItem >= 0 && ListView_GetItem(m_List, &item) && !(item.state & LVIS_STATEIMAGEMASK)
				&& previousImage >= 0 && item.iImage >= 0 && previousImage != item.iImage) {
				m_CheckedImages[previousImage] = wasChecked;
				m_CheckedImages[item.iImage] = !wasChecked;
			}
		}
		void changeChecks(std::optional<bool> desired) {
			const auto selected = selection();
			if (selected.empty()) { return; }
			m_Internal = true;
			for (const auto entry : selected) {
				if (desired && checked(entry) == *desired) { continue; }
				const int row = rowForEntry(entry);
				if (row < 0) { continue; }
				const bool wasChecked = checked(entry);
				LVITEMA previous = {}; previous.mask = LVIF_IMAGE; previous.iItem = row; previous.iImage = -1;
				ListView_GetItem(m_List, &previous);
				// Invoke the original toggle with one selected row at a time, so
				// neither its single-row nor multi-row path can toggle a row twice.
				ListView_SetItemState(m_List, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
				ListView_SetItemState(m_List, row, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
				NMITEMACTIVATE action = {};
				action.hdr.hwndFrom = m_List; action.hdr.idFrom = CONTROL_ID_DATA_FILES_LIST; action.hdr.code = NM_DBLCLK;
				action.iItem = row; action.iSubItem = 0; action.lParam = m_Entries[entry].payload;
				RECT rect = {}; ListView_GetItemRect(m_List, row, &rect, LVIR_BOUNDS);
				action.ptAction = {rect.left+3, (rect.top+rect.bottom)/2};
				SendMessageA(m_Window, WM_NOTIFY, CONTROL_ID_DATA_FILES_LIST, reinterpret_cast<LPARAM>(&action));
				learnToggle(entry, previous.iImage, wasChecked);
			}
			ListView_SetItemState(m_List, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
			for (const auto entry : selected) { const int row = rowForEntry(entry); if (row >= 0) { ListView_SetItemState(m_List, row, LVIS_SELECTED, LVIS_SELECTED); } }
			if (const int row = rowForEntry(selected.front()); row >= 0) { ListView_SetItemState(m_List, row, LVIS_FOCUSED, LVIS_FOCUSED); }
			m_Internal = false; updateStatus();
		}
		void ensureDependencies() {
			if (m_DependenciesLoaded) { return; }
			for (size_t i = 0; i < m_Entries.size(); ++i) {
				std::ifstream file(m_Entries[i].path, std::ios::binary);
				if (!file) { throw std::runtime_error("Cannot read plugin: " + utf8(m_Entries[i].path.filename().wstring())); }
				auto dependencies = plugins::LoadOrder::readDependencies(file);
				for (auto& name : dependencies) { name = utf8(lower(wide(name))); }
				m_Order->setDependencies(i, std::move(dependencies));
			}
			m_DependenciesLoaded = true;
		}
		void moveSelection(const std::vector<size_t>& selected, std::optional<size_t> before) {
			if (selected.empty()) { return; }
			if (!m_CanReorder) { throw std::runtime_error("This editor uses an unsupported plugin-row format. Reordering is disabled to preserve its native data."); }
			ensureDependencies();
			std::string error;
			if (!m_Order->move(selected, before, error)) { throw std::runtime_error(error); }
			sortToOrder(); applyFilter();
			const auto row = rowForEntry(selected.front());
			if (row >= 0) { ListView_EnsureVisible(m_List, row, FALSE); }
		}
		void nudge(bool down) {
			const auto selected = selection();
			if (selected.empty()) { return; }
			const std::unordered_set<size_t> set(selected.begin(), selected.end());
			const auto& order = m_Order->order();
			size_t first = order.size(), last = 0;
			for (size_t i = 0; i < order.size(); ++i) { if (set.contains(order[i])) { first = std::min(first, i); last = i; } }
			if ((!down && first == 0) || (down && last+1 >= order.size())) { return; }
			moveSelection(selected, down ? (last+2 < order.size() ? std::optional<size_t>{order[last+2]} : std::nullopt) : std::optional<size_t>{order[first-1]});
		}
		void beginDrag() {
			m_DragEntries = selection();
			if (m_DragEntries.empty()) { return; }
			m_Dragging = true; SetCapture(m_List); SetTimer(m_List, DRAG_TIMER, 80, nullptr);
			updateDrop();
		}
		void stopDrag(bool apply) {
			if (!m_Dragging) { return; }
			const auto entries = m_DragEntries;
			const auto before = m_DropBefore;
			const bool valid = m_DropValid;
			m_Dragging = false; m_DragLine = -1; m_DragEntries.clear();
			KillTimer(m_List, DRAG_TIMER); if (GetCapture() == m_List) { ReleaseCapture(); }
			InvalidateRect(m_List, nullptr, FALSE);
			if (apply && valid) { moveSelection(entries, before); }
		}
		void updateDrop() {
			POINT point = {}; GetCursorPos(&point); ScreenToClient(m_List, &point);
			RECT client = {}; GetClientRect(m_List, &client);
			m_DropValid = PtInRect(&client, point) != FALSE;
			m_DropBefore.reset(); m_DragLine = -1;
			if (!m_DropValid) { return; }
			LVHITTESTINFO hit = {}; hit.pt = point;
			const int row = ListView_HitTest(m_List, &hit);
			if (const auto entry = entryForRow(row)) {
				RECT rect = {}; ListView_GetItemRect(m_List, row, &rect, LVIR_BOUNDS);
				const bool after = point.y > (rect.top+rect.bottom)/2;
				const auto position = m_Rank[*entry] + (after ? 1 : 0);
				if (position < m_Order->order().size()) { m_DropBefore = m_Order->order()[position]; }
				m_DragLine = after ? rect.bottom : rect.top;
			}
			else {
				const auto header = ListView_GetHeader(m_List);
				RECT rect = {}; GetWindowRect(header, &rect); MapWindowPoints(nullptr, m_List, reinterpret_cast<POINT*>(&rect), 2);
				if (point.y < rect.bottom) { m_DropValid = false; }
				else { m_DragLine = client.bottom-2; }
			}
			SetCursor(LoadCursorW(nullptr, MAKEINTRESOURCEW(32646))); // IDC_SIZEALL
			InvalidateRect(m_List, nullptr, FALSE);
		}
		void dragTimer() {
			POINT point = {}; GetCursorPos(&point); ScreenToClient(m_List, &point);
			RECT client = {}; GetClientRect(m_List, &client);
			if (point.y < client.top+28) { SendMessageA(m_List, WM_VSCROLL, SB_LINEUP, 0); }
			else if (point.y > client.bottom-24) { SendMessageA(m_List, WM_VSCROLL, SB_LINEDOWN, 0); }
			updateDrop();
		}
		std::vector<StlList<GameFile*>::Node*> sourceNodes() const {
			if (m_Handler->availableDataFiles != m_Available || m_Available->size() != m_Entries.size()) {
				throw std::runtime_error("The plugin list changed. Reopen Data Files before saving the order.");
			}
			std::vector<StlList<GameFile*>::Node*> nodes;
			std::unordered_set<GameFile*> expected;
			for (const auto& entry : m_Entries) { expected.insert(entry.file); }
			for (auto node = m_Available->head; node && node != m_Available->sentinel() && nodes.size() < m_Entries.size(); node = node->next) {
				if (expected.erase(node->data) != 1) { throw std::runtime_error("The editor's plugin list no longer matches this window."); }
				nodes.push_back(node);
			}
			if (!expected.empty()) { throw std::runtime_error("The editor's plugin list is incomplete."); }
			return nodes;
		}
		std::filesystem::path writeBackup(const std::vector<plugins::TimeChange>& changes) const {
			const auto directory = path::getInstallPath() / L"CSSE" / L"load-order-backups";
			std::filesystem::create_directories(directory);
			FILETIME now = {}; GetSystemTimeAsFileTime(&now);
			static std::uint64_t sequence = 0;
			const auto backup = directory / (std::to_wstring(ticks(now)) + L"-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(++sequence) + L".json");
			std::ofstream file(backup, std::ios::binary);
			file << "{\n  \"format\": \"CSSE plugin timestamps v1\",\n  \"files\": [\n";
			for (size_t i = 0; i < changes.size(); ++i) {
				const auto& change = changes[i];
				file << "    {\"path\": " << json(utf8(std::filesystem::absolute(m_Entries[change.entry].path).wstring()))
					<< ", \"before_filetime\": \"" << change.before << "\", \"after_filetime\": \"" << change.after << "\"}"
					<< (i+1 < changes.size() ? ",\n" : "\n");
			}
			file << "  ]\n}\n"; file.close();
			if (!file) { throw std::runtime_error("Cannot save the load-order backup. No plugin dates were changed."); }
			return backup;
		}
		void commitOrder() {
			if (!m_Order->changed()) { return; }
			const auto nodes = sourceNodes();
			const auto changes = m_Order->planTimes();
			std::vector<size_t> rows;
			for (int row = 0; row < ListView_GetItemCount(m_List); ++row) {
				const auto entry = entryForRow(row);
				if (!entry) { throw std::runtime_error("The plugin rows changed. Reopen Data Files."); }
				rows.push_back(*entry);
			}
			std::unordered_map<LPARAM, size_t> replacementMap;
			if (m_IndexPayloads && !m_PointerPayloads) {
				for (size_t i = 0; i < m_Order->order().size(); ++i) { replacementMap.emplace(static_cast<LPARAM>(i), m_Order->order()[i]); }
			}
			std::vector<OpenFile> opened; opened.reserve(changes.size());
			for (const auto& change : changes) {
				const auto handle = CreateFileW(m_Entries[change.entry].path.c_str(), FILE_READ_ATTRIBUTES | FILE_WRITE_ATTRIBUTES,
					FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
				if (handle == INVALID_HANDLE_VALUE) { throw std::runtime_error("Cannot update plugin dates: " + utf8(m_Entries[change.entry].path.filename().wstring()) + " (Windows error " + std::to_string(GetLastError()) + ")."); }
				FILETIME current = {};
				if (!GetFileTime(handle, nullptr, nullptr, &current)) { CloseHandle(handle); throw std::runtime_error("Cannot read the plugin modification date."); }
				opened.emplace_back(handle, change.entry, current);
				if (ticks(current) != change.before) { throw std::runtime_error("A plugin was modified while Data Files was open. Reopen this window before reordering."); }
			}
			const auto backup = writeBackup(changes);
			DWORD error = 0;
			const auto applied = plugins::LoadOrder::applyTimes(changes, [&](size_t i, std::uint64_t value) {
				const auto target = fileTime(value);
				FILETIME actual = {};
				const bool result = SetFileTime(opened[i].handle, nullptr, nullptr, &target)
					&& GetFileTime(opened[i].handle, nullptr, nullptr, &actual) && ticks(actual) == value;
				if (!result && !error) { error = GetLastError(); if (!error) { error = ERROR_WRITE_FAULT; } }
				return result;
			});
			if (applied != plugins::LoadOrder::ApplyResult::Complete) {
				throw std::runtime_error(std::string("Could not save plugin dates (Windows error ") + std::to_string(error) + "). "
					+ (applied == plugins::LoadOrder::ApplyResult::RolledBack ? "The original dates were restored." : "Some dates could not be restored; use the timestamp backup.")
					+ " Backup: " + utf8(backup.wstring()));
			}
			for (const auto& change : changes) { m_Entries[change.entry].file->findFileData.ftLastWriteTime = fileTime(change.after); }
			// The engine reads its available-file list when committing this dialog.
			// Reorder data pointers in existing nodes; retain every engine allocation.
			for (size_t i = 0; i < nodes.size(); ++i) { nodes[i]->data = m_Entries[m_Order->order()[i]].file; }
			if (m_IndexPayloads && !m_PointerPayloads) {
				for (size_t i = 0; i < m_Order->order().size(); ++i) { m_Entries[m_Order->order()[i]].payload = static_cast<LPARAM>(i); }
				for (size_t row = 0; row < rows.size(); ++row) {
					LVITEMA item = {}; item.mask = LVIF_PARAM; item.iItem = static_cast<int>(row); item.lParam = m_Entries[rows[row]].payload;
					ListView_SetItem(m_List, &item);
				}
				m_ByPayload.swap(replacementMap);
			}
			m_Order->acceptTimes(changes); sortToOrder();
			log::stream << "Data Files: load order committed; " << changes.size() << " modification dates updated. Backup: " << backup.string() << std::endl;
		}
		void report(const std::exception& error) const {
			log::stream << "Data Files: " << error.what() << std::endl;
			MessageBoxW(m_Window, wide(error.what(), CP_UTF8).c_str(), L"CSSE - Data Files", MB_OK | MB_ICONINFORMATION);
		}
		static LRESULT CALLBACK filterProc(HWND window, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR data) {
			const auto self = reinterpret_cast<DataFilesController*>(data);
			if (msg == WM_GETDLGCODE && (wp == VK_ESCAPE || wp == VK_RETURN)) { return DefSubclassProc(window, msg, wp, lp) | DLGC_WANTMESSAGE; }
			if (msg == WM_KEYDOWN && wp == VK_ESCAPE) { SetWindowTextW(window, L""); SetFocus(self->m_List); return 0; }
			if (msg == WM_KEYDOWN && wp == VK_RETURN) { SetFocus(self->m_List); return 0; }
			if (msg == WM_NCDESTROY) { RemoveWindowSubclass(window, filterProc, SUBCLASS_ID); }
			return DefSubclassProc(window, msg, wp, lp);
		}
		static LRESULT CALLBACK listProc(HWND window, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR data) {
			const auto self = reinterpret_cast<DataFilesController*>(data);
			try {
				if (msg == WM_GETDLGCODE && (wp == VK_SPACE || (wp == VK_ESCAPE && self->m_Dragging))) { return DefSubclassProc(window, msg, wp, lp) | DLGC_WANTMESSAGE; }
				if (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) {
					if (wp == VK_SPACE && GetKeyState(VK_MENU) >= 0) { self->changeChecks({}); return 0; }
					if (wp == 'F' && GetKeyState(VK_CONTROL) < 0) { SetFocus(self->m_Filter); SendMessageW(self->m_Filter, EM_SETSEL, 0, -1); return 0; }
					if (wp == 'A' && GetKeyState(VK_CONTROL) < 0) {
						for (int row = 0; row < ListView_GetItemCount(window); ++row) { const auto entry = self->entryForRow(row); if (entry && self->m_Entries[*entry].visible) { ListView_SetItemState(window, row, LVIS_SELECTED, LVIS_SELECTED); } }
						self->updateStatus(); return 0;
					}
					if ((wp == VK_UP || wp == VK_DOWN) && GetKeyState(VK_MENU) < 0) { self->nudge(wp == VK_DOWN); return 0; }
					if (wp == VK_ESCAPE && self->m_Dragging) { self->stopDrag(false); return 0; }
				}
				if (msg == WM_CHAR && (wp == ' ' || wp == 1 || wp == 6)) { return 0; }
				if (msg == WM_MOUSEMOVE && self->m_Dragging) { self->updateDrop(); return 0; }
				if (msg == WM_LBUTTONUP && self->m_Dragging) { self->updateDrop(); self->stopDrag(true); return 0; }
				if (msg == WM_TIMER && wp == DRAG_TIMER && self->m_Dragging) { self->dragTimer(); return 0; }
				if ((msg == WM_CANCELMODE || msg == WM_CAPTURECHANGED) && self->m_Dragging) { self->stopDrag(false); }
				if (msg == WM_PAINT) {
					const auto result = DefSubclassProc(window, msg, wp, lp);
					if (self->m_Dragging && self->m_DragLine >= 0) {
						const auto dc = GetDC(window); const auto pen = CreatePen(PS_SOLID, 2, RGB(74, 151, 255));
						const auto old = SelectObject(dc, pen); RECT rect = {}; GetClientRect(window, &rect);
						MoveToEx(dc, 2, self->m_DragLine, nullptr); LineTo(dc, rect.right-2, self->m_DragLine);
						SelectObject(dc, old); DeleteObject(pen); ReleaseDC(window, dc);
					}
					return result;
				}
			} catch (const std::exception& error) { self->stopDrag(false); self->report(error); return 0; }
			if (msg == WM_NCDESTROY) { KillTimer(window, DRAG_TIMER); RemoveWindowSubclass(window, listProc, SUBCLASS_ID); }
			return DefSubclassProc(window, msg, wp, lp);
		}
		static LRESULT CALLBACK dialogProc(HWND window, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR data) {
			const auto self = reinterpret_cast<DataFilesController*>(data);
			if (msg == WM_NCDESTROY) {
				self->stopDrag(false);
				RemovePropA(window, CONTROLLER_PROPERTY); RemoveWindowSubclass(window, dialogProc, SUBCLASS_ID);
				const auto result = DefSubclassProc(window, msg, wp, lp); delete self; return result;
			}
			try {
				if (msg == WM_COMMAND) {
					const int id = LOWORD(wp);
					if (id == ID_FILTER && HIWORD(wp) == EN_CHANGE) { self->applyFilter(); return 0; }
					if (id == ID_CLEAR) { SetWindowTextW(self->m_Filter, L""); SetFocus(self->m_Filter); return 0; }
					if (id == ID_CHECK || id == ID_UNCHECK) { self->changeChecks(id == ID_CHECK); return 0; }
					if (id == ID_UP || id == ID_DOWN) { self->nudge(id == ID_DOWN); return 0; }
					if ((id == CONTROL_ID_SET_AS_ACTIVE_BUTTON || id == CONTROL_ID_MERGE_TO_MASTERS_BUTTON) && ListView_GetSelectedCount(self->m_List) != 1) { return 0; }
					if (id == CONTROL_ID_OK_BUTTON) {
						self->stopDrag(false); self->commitOrder(); self->applyFilter(true);
						ListView_EnableGroupView(self->m_List, FALSE);
						const auto result = DefSubclassProc(window, msg, wp, lp);
						if (IsWindow(window) && GetPropA(window, CONTROLLER_PROPERTY) == self) {
							if (self->m_GroupFiltering) { ListView_EnableGroupView(self->m_List, TRUE); }
							self->applyFilter();
						}
						return result;
					}
				}
				if (msg == WM_NOTIFY) {
					const auto notification = reinterpret_cast<NMHDR*>(lp);
					if (notification && notification->hwndFrom == self->m_List && !self->m_Internal) {
						if (notification->code == NM_DBLCLK) { self->changeChecks({}); return 0; }
						if (notification->code == LVN_BEGINDRAG) { self->beginDrag(); return 0; }
						if (notification->code == LVN_COLUMNCLICK) { return 0; } // Keep the displayed order equal to the load order.
						if (notification->code == LVN_ITEMCHANGED) {
							const auto result = DefSubclassProc(window, msg, wp, lp);
							if (IsWindow(window) && GetPropA(window, CONTROLLER_PROPERTY) == self) { self->updateStatus(); }
							return result;
						}
					}
				}
			} catch (const std::exception& error) { self->m_Internal = false; self->report(error); return 0; }
			return DefSubclassProc(window, msg, wp, lp);
		}

		HWND m_Window = nullptr, m_List = nullptr, m_Filter = nullptr, m_Status = nullptr;
		RecordHandler* m_Handler = nullptr;
		StlList<GameFile*>* m_Available = nullptr;
		std::vector<NativeEntry> m_Entries;
		std::unordered_map<LPARAM, size_t> m_ByPayload;
		std::unordered_map<int, std::optional<bool>> m_CheckedImages;
		std::unique_ptr<plugins::LoadOrder> m_Order;
		std::vector<size_t> m_Rank, m_DragEntries;
		std::optional<size_t> m_DropBefore;
		bool m_CanReorder = false, m_PointerPayloads = false, m_IndexPayloads = false;
		bool m_GroupFiltering = false, m_Internal = false, m_DependenciesLoaded = false;
		bool m_Dragging = false, m_DropValid = false;
		int m_DragLine = -1;
	};

	static LRESULT CALLBACK afterWindowMessage(int code, WPARAM wp, LPARAM lp) {
		if (code >= 0) {
			const auto message = reinterpret_cast<const CWPRETSTRUCT*>(lp);
			if (message && message->message == WM_INITDIALOG && GetDlgItem(message->hwnd, CONTROL_ID_DATA_FILES_LIST)
				&& GetDlgItem(message->hwnd, CONTROL_ID_SET_AS_ACTIVE_BUTTON) && GetDlgItem(message->hwnd, CONTROL_ID_MASTERS_LIST)) {
				try { DataFilesController::attach(message->hwnd); }
				catch (const std::exception& error) { log::stream << "Data Files initialization failed: " << error.what() << std::endl; }
			}
		}
		return CallNextHookEx(initializationHook, code, wp, lp);
	}

	void installPatches() {
		if (!initializationHook) {
			initializationHook = SetWindowsHookExA(WH_CALLWNDPROCRET, afterWindowMessage, nullptr, GetCurrentThreadId());
			if (!initializationHook) { log::stream << "Data Files: cannot install dialog initialization hook (" << GetLastError() << ")." << std::endl; }
		}
	}
}
