#include "IconOverride.h"

#include "DarkMode.h"
#include "LogUtil.h"
#include "MemoryUtil.h"

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace se::cs::iconoverride {

	// Original import targets, saved before the IAT slots are redirected.
	static decltype(&LoadIconA) realLoadIconA = nullptr;
	static decltype(&LoadImageA) realLoadImageA = nullptr;
	static decltype(&ImageList_LoadImageA) realImageList_LoadImageA = nullptr;
	static decltype(&CreateToolbarEx) realCreateToolbarEx = nullptr;

	enum class Kind { bitmap, icon };

	struct NamedResource {
		UINT id;
		Kind kind;
		const char* name;
	};

	// Stable names for the original CS resources, supplied by PNG RCDATA in this DLL.
	static constexpr NamedResource exeResources[] = {
		// Toolbar strips, 16x16 cells, silver RGB(192,192,192) keyed.
		{ 152, Kind::bitmap, "toolbar_main" },		// Unused while PatchReplaceToolbarBitmap supplies CSSE's strip.
		{ 194, Kind::bitmap, "toolbar_render" },	// Render + Preview windows. 112x16.
		{ 219, Kind::bitmap, "toolbar_object" },	// Object Window + Script Editor. 112x16.

		// Record/spell type strip used by the Object Window, Cell View, and the
		// NPC/Creature/Race spell lists. 384x16, 24 cells, teal RGB(0,128,128) keyed.
		{ 178, Kind::bitmap, "object_icons" },

		// Show Scene Graph browser tiles, 16x15 (107/108 are 16x16), drawn opaque.
		{ 103, Kind::bitmap, "scenegraph_103" },
		{ 104, Kind::bitmap, "scenegraph_104" },
		{ 105, Kind::bitmap, "scenegraph_105" },
		{ 106, Kind::bitmap, "scenegraph_106" },
		{ 107, Kind::bitmap, "scenegraph_107" },
		{ 108, Kind::bitmap, "scenegraph_108" },
		{ 109, Kind::bitmap, "scenegraph_109" },
		{ 113, Kind::bitmap, "scenegraph_113" },

		// Window class icons (MDI child captions), 32x32.
		{ 128, Kind::icon, "window_main" },
		{ 129, Kind::icon, "window_cell_view" },	// Also the Preview window class.
		{ 130, Kind::icon, "window_plane" },
		{ 131, Kind::icon, "window_render" },
		{ 132, Kind::icon, "window_face" },
		{ 143, Kind::icon, "window_weapon" },
		{ 144, Kind::icon, "window_npc" },
		{ 145, Kind::icon, "window_armor" },
		{ 146, Kind::icon, "window_creature" },
		{ 149, Kind::icon, "window_lockpick" },
		{ 150, Kind::icon, "window_alchemy" },
		{ 151, Kind::icon, "window_activator" },
		{ 191, Kind::icon, "window_speaker" },		// Also the Land window class.

		// Data Files dialog state marks, 16x16. Overriding any of these
		// disables the black-stroke recolor hack, so supply all five.
		{ 133, Kind::icon, "datafiles_check_checked" },
		{ 134, Kind::icon, "datafiles_check_unchecked" },
	};

	static std::string getOverrideName(HINSTANCE hInstance, UINT resourceId, Kind kind) {
		if (hInstance == GetModuleHandleA(nullptr)) {
			for (const auto& resource : exeResources) {
				if (resource.id == resourceId && resource.kind == kind) {
					return resource.name;
				}
			}
			// Unmapped exe resources stay overridable by raw id.
			return (kind == Kind::bitmap ? "bitmap_" : "icon_") + std::to_string(resourceId);
		}
		if (hInstance == reinterpret_cast<HINSTANCE>(&__ImageBase) && kind == Kind::bitmap) {
			if (resourceId == IDB_MAIN_TOOLBAR) { return "toolbar_main"; }
			if (resourceId == IDB_TOOLBAR_STRIP) { return "toolbar_layers"; }
		}
		return {};
	}

	static std::string findEmbeddedResource(HINSTANCE hInstance, UINT resourceId, Kind kind) {
		const auto name = getOverrideName(hInstance, resourceId, kind);
		if (name.empty()) { return {}; }
		std::string resource = darkmode::isActive() ? "CSSE_ICON_DARK_" : "CSSE_ICON_LIGHT_";
		for (const auto c : name) {
			resource += c >= 'a' && c <= 'z' ? static_cast<char>(c - 'a' + 'A') : c;
		}
		return FindResourceA(reinterpret_cast<HINSTANCE>(&__ImageBase), resource.c_str(), RT_RCDATA) ? resource : std::string{};
	}

	static void logResult(const std::string& resource, bool loaded) {
		static std::set<std::string> logged;
		if (logged.insert(resource).second) {
			log::stream << "Embedded icon: " << (loaded ? "loaded " : "failed ") << resource << "." << std::endl;
		}
	}

	//
	// Image loading.
	//

	static bool ensureGdiplus() {
		// Started on first use; deliberately never shut down.
		static const ULONG_PTR token = [] {
			ULONG_PTR value = 0;
			Gdiplus::GdiplusStartupInput input;
			if (Gdiplus::GdiplusStartup(&value, &input, nullptr) != Gdiplus::Ok) {
				value = 0;
			}
			return value;
		}();
		return token != 0;
	}

	static HBITMAP createDibSection32(int width, int height, void** bits) {
		BITMAPINFO info = {};
		info.bmiHeader.biSize = sizeof(info.bmiHeader);
		info.bmiHeader.biWidth = width;
		info.bmiHeader.biHeight = -height;
		info.bmiHeader.biPlanes = 1;
		info.bmiHeader.biBitCount = 32;
		info.bmiHeader.biCompression = BI_RGB;
		return CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, bits, nullptr, 0);
	}

	// GDI+ requires the stream to outlive the image. The stream owns a copy of
	// the resource bytes, and releases that memory after the image is destroyed.
	class ResourceImage {
	public:
		explicit ResourceImage(const std::string& name) {
			if (!ensureGdiplus()) { return; }
			const auto module = reinterpret_cast<HINSTANCE>(&__ImageBase);
			const auto resource = FindResourceA(module, name.c_str(), RT_RCDATA);
			if (!resource) { return; }
			const auto size = SizeofResource(module, resource);
			const auto source = LockResource(LoadResource(module, resource));
			if (!source || !size) { return; }
			const auto memory = GlobalAlloc(GMEM_MOVEABLE, size);
			if (!memory) { return; }
			const auto bytes = GlobalLock(memory);
			if (!bytes) { GlobalFree(memory); return; }
			memcpy(bytes, source, size);
			GlobalUnlock(memory);
			if (FAILED(CreateStreamOnHGlobal(memory, TRUE, &m_Stream))) {
				GlobalFree(memory); return;
			}
			m_Image.reset(Gdiplus::Bitmap::FromStream(m_Stream, FALSE));
			if (m_Image && m_Image->GetLastStatus() != Gdiplus::Ok) { m_Image.reset(); }
		}
		~ResourceImage() { m_Image.reset(); if (m_Stream) { m_Stream->Release(); } }
		Gdiplus::Bitmap* get() const { return m_Image.get(); }
	private:
		IStream* m_Stream = nullptr;
		std::unique_ptr<Gdiplus::Bitmap> m_Image;
	};

	static HBITMAP loadPngAsDib(const std::string& resource) {
		ResourceImage image(resource);
		const auto source = image.get();
		if (!source) { return nullptr; }
		const int width = source->GetWidth();
		const int height = source->GetHeight();
		void* bits = nullptr;
		const auto dib = createDibSection32(width, height, &bits);
		if (!dib) { return nullptr; }
		Gdiplus::Rect rect(0, 0, width, height);
		Gdiplus::BitmapData locked = {};
		if (source->LockBits(&rect, Gdiplus::ImageLockModeRead, PixelFormat32bppPARGB, &locked) != Gdiplus::Ok) {
			DeleteObject(dib); return nullptr;
		}
		for (int y = 0; y < height; ++y) {
			memcpy(static_cast<BYTE*>(bits) + y * width * 4, static_cast<const BYTE*>(locked.Scan0) + y * locked.Stride, width * 4u);
		}
		source->UnlockBits(&locked);
		return dib;
	}

	static HICON loadPngAsIcon(const std::string& resource) {
		ResourceImage image(resource);
		const auto source = image.get();
		if (!source) { return nullptr; }
		HICON icon = nullptr;
		return source->GetHICON(&icon) == Gdiplus::Ok ? icon : nullptr;
	}

	// Returns a 32bpp copy of a bitmap with an opaque alpha channel, for use
	// in ILC_COLOR32 image lists where a zeroed alpha channel draws invisible.
	static HBITMAP toOpaque32bppDib(HBITMAP source) {
		BITMAP info = {};
		if (GetObjectA(source, sizeof(info), &info) != sizeof(info)) {
			return nullptr;
		}

		void* bits = nullptr;
		const auto dib = createDibSection32(info.bmWidth, info.bmHeight, &bits);
		if (dib == nullptr) {
			return nullptr;
		}

		BITMAPINFO dibInfo = {};
		dibInfo.bmiHeader.biSize = sizeof(dibInfo.bmiHeader);
		dibInfo.bmiHeader.biWidth = info.bmWidth;
		dibInfo.bmiHeader.biHeight = -info.bmHeight;
		dibInfo.bmiHeader.biPlanes = 1;
		dibInfo.bmiHeader.biBitCount = 32;
		dibInfo.bmiHeader.biCompression = BI_RGB;
		const auto hdc = GetDC(nullptr);
		const auto lines = GetDIBits(hdc, source, 0, info.bmHeight, bits, &dibInfo, DIB_RGB_COLORS);
		ReleaseDC(nullptr, hdc);
		if (lines != info.bmHeight) {
			DeleteObject(dib);
			return nullptr;
		}

		const auto pixels = static_cast<DWORD*>(bits);
		for (int i = 0; i < info.bmWidth * info.bmHeight; ++i) {
			pixels[i] |= 0xFF000000;
		}
		return dib;
	}

	HBITMAP loadBitmapOverride(HINSTANCE hInstance, UINT resourceId, bool& outHasAlpha) {
		outHasAlpha = false;
		const auto resource = findEmbeddedResource(hInstance, resourceId, Kind::bitmap);
		if (resource.empty()) { return nullptr; }
		const auto bitmap = loadPngAsDib(resource);
		outHasAlpha = bitmap != nullptr;
		logResult(resource, outHasAlpha);
		return bitmap;
	}

	bool hasDataFilesCheckOverrides() {
		const auto exe = GetModuleHandleA(nullptr);
		return !findEmbeddedResource(exe, 133, Kind::icon).empty() && !findEmbeddedResource(exe, 134, Kind::icon).empty();
	}

	//
	// Import hooks. Resource ids only reach the embedded lookup for the CS exe
	// instance (and CSSE's toolbar strip); everything else passes through.
	//

	static HICON WINAPI hookLoadIconA(HINSTANCE hInstance, LPCSTR lpIconName) {
		if (IS_INTRESOURCE(lpIconName)) {
			const auto id = static_cast<UINT>(reinterpret_cast<ULONG_PTR>(lpIconName));
			const auto resource = findEmbeddedResource(hInstance, id, Kind::icon);
			if (!resource.empty()) {
				const auto icon = loadPngAsIcon(resource);
				logResult(resource, icon != nullptr);
				if (icon) { return icon; }
			}
		}
		return realLoadIconA(hInstance, lpIconName);
	}

	static HANDLE WINAPI hookLoadImageA(HINSTANCE hInst, LPCSTR name, UINT type, int cx, int cy, UINT fuLoad) {
		// The exe's only bitmap LoadImageA caller builds the Show Scene Graph
		// image list, whose creation flags are promoted to ILC_COLOR32.
		if (type != IMAGE_BITMAP || !IS_INTRESOURCE(name)) {
			return realLoadImageA(hInst, name, type, cx, cy, fuLoad);
		}

		const auto id = static_cast<UINT>(reinterpret_cast<ULONG_PTR>(name));
		bool hasAlpha = false;
		auto bitmap = loadBitmapOverride(hInst, id, hasAlpha);
		if (bitmap == nullptr) {
			bitmap = reinterpret_cast<HBITMAP>(realLoadImageA(hInst, name, type, cx, cy, fuLoad));
		}
		if (bitmap != nullptr && !hasAlpha) {
			if (const auto converted = toOpaque32bppDib(bitmap)) {
				DeleteObject(bitmap);
				bitmap = converted;
			}
		}
		return bitmap;
	}

	static HIMAGELIST WINAPI hookImageList_LoadImageA(HINSTANCE hi, LPCSTR lpbmp, int cx, int cGrow, COLORREF crMask, UINT uType, UINT uFlags) {
		if (uType == IMAGE_BITMAP && IS_INTRESOURCE(lpbmp)) {
			const auto id = static_cast<UINT>(reinterpret_cast<ULONG_PTR>(lpbmp));
			bool hasAlpha = false;
			if (const auto bitmap = loadBitmapOverride(hi, id, hasAlpha)) {
				BITMAP info = {};
				GetObjectA(bitmap, sizeof(info), &info);
				const int width = cx > 0 ? cx : static_cast<int>(info.bmWidth);
				const int count = std::max(1, static_cast<int>(info.bmWidth) / std::max(1, width));
				const auto imageList = darkmode::buildImageList(bitmap, width, info.bmHeight, count, cGrow, hasAlpha, crMask);
				DeleteObject(bitmap);
				if (imageList) {
					return imageList;
				}
			}
		}
		return realImageList_LoadImageA(hi, lpbmp, cx, cGrow, crMask, uType, uFlags);
	}

	static HWND WINAPI hookCreateToolbarEx(HWND hwnd, DWORD ws, UINT wID, int nBitmaps, HINSTANCE hBMInst, UINT_PTR wBMID, LPCTBBUTTON lpButtons, int iNumButtons, int dxButton, int dyButton, int dxBitmap, int dyBitmap, UINT uStructSize) {
		const auto toolbar = realCreateToolbarEx(hwnd, ws, wID, nBitmaps, hBMInst, wBMID, lpButtons, iNumButtons, dxButton, dyButton, dxBitmap, dyBitmap, uStructSize);

		// Composites the strip over the dark toolbar fill and picks up any
		// override. This reaches the Render/Preview and Object/Script Editor
		// toolbars; the main toolbar goes through PatchReplaceToolbarBitmap.
		darkmode::remapToolbarImages(toolbar, hBMInst, static_cast<UINT>(wBMID), dxBitmap, dyBitmap);
		return toolbar;
	}

	template <typename T>
	static void hookImportSlot(DWORD slotAddress, T& realFunction, T hookFunction) {
		realFunction = memory::MemAccess<T>::Get(slotAddress);
		memory::writeDoubleWordUnprotected(slotAddress, reinterpret_cast<DWORD>(hookFunction));
	}

	void initialize() {
		log::stream << "Icons: using embedded " << (darkmode::isActive() ? "dark" : "light") << " resources." << std::endl;

		hookImportSlot(0x6D9DA0, realLoadIconA, hookLoadIconA);
		hookImportSlot(0x6D9E38, realLoadImageA, hookLoadImageA);
		hookImportSlot(0x6D9918, realImageList_LoadImageA, hookImageList_LoadImageA);
		hookImportSlot(0x6D990C, realCreateToolbarEx, hookCreateToolbarEx);

		// Allow 32-bpp icons in a few lists.
		memory::genPushEnforced(0x411DBF, static_cast<BYTE>(ILC_COLOR32 | ILC_MASK)); // Data Files
		memory::genPushEnforced(0x493178, static_cast<BYTE>(ILC_COLOR32)); // Show Scene Graph
	}
}
