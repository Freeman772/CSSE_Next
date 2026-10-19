#include "EditBasicExtended.h"

#include "WindowsUtil.h"

namespace se::cs::ui_subclass::edit {
	LRESULT OnChar(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
		// Decode parameters.
		const auto vkCode = LOWORD(wParam);
		const auto keyFlags = HIWORD(lParam);
		const auto isExtendedKey = (keyFlags & KF_EXTENDED) == KF_EXTENDED;
		const auto scanCode = isExtendedKey ? MAKEWORD(WORD(LOBYTE(keyFlags)), 0xE0) : WORD(LOBYTE(keyFlags));
		const auto wasKeyDown = (keyFlags & KF_REPEAT) == KF_REPEAT;
		const auto repeatCount = LOWORD(lParam);
		const auto isKeyReleased = (keyFlags & KF_UP) == KF_UP;

		// Allow the use of control+A to select all text.
		if (vkCode == 1) {
			Edit_SetSel(hWnd, 0, -1);
			return FALSE;
		}

		// Perform default behavior.
		return DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}

	LRESULT CALLBACK BasicExtendedProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
		switch (uMsg) {
		case WM_CHAR:
			return OnChar(hWnd, uMsg, wParam, lParam, uIdSubclass, dwRefData);
		}

		return DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}
}