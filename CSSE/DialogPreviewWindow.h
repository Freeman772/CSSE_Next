#pragma once

#include "NIDefines.h"

namespace se::cs::dialog::preview_window {
	struct NetImmerseInstance;

	struct PreviewController {
		NetImmerseInstance* niInstance; // 0x0
		NI::AVObject* field_0x4; // 0x4
		NI::Node* field_0x8; // 0x8
		NI::Node* field_0xC; // 0xC
		NI::Node* field_0x10; // 0x10
		NI::ZBufferProperty* zBufferProperty; // 0x14
		NI::Property* wireframeProperty; // 0x18
		int field_0x1C; // 0x1C

		static inline auto get() {
			return memory::ExternalGlobal<PreviewController*, 0x6CF110>::get();
		}
	};
	static_assert(sizeof(PreviewController) == 0x20, "CS::PreviewController failed size validation");

	constexpr UINT DIALOG_ID = 181;

	void installPatches();
}