#pragma once

namespace se::cs::iconoverride {
	// Load a DLL-embedded PNG as a premultiplied 32bpp DIB, or nullptr if the
	// original resource has no replacement. No loose asset files are read.
	HBITMAP loadBitmapOverride(HINSTANCE hInstance, UINT resourceId, bool& outHasAlpha);

	// Both original Data Files checkbox glyphs have embedded replacements.
	bool hasDataFilesCheckOverrides();

	// Redirect CS image imports to the embedded dark/light resource set.
	// Run after darkmode::initialize() and before the original WinMain.
	void initialize();
}
