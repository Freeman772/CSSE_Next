#pragma once

namespace se::cs::dialog::script_list_window {
	constexpr UINT DIALOG_ID = 189;

	// Default IDs.
	constexpr UINT CONTROL_ID_SCRIPT_LIST = 1018;

	// Custom IDs.
	constexpr UINT CONTROL_ID_FILTER_LABEL = 2000;
	constexpr UINT CONTROL_ID_FILTER_EDIT = 2001;
	constexpr UINT CONTROL_ID_SHOW_MODIFIED_ONLY_BUTTON = 2002;

	void installPatches();
}