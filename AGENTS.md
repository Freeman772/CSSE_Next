# CSSE project instructions

- Do not take control of the user's PC unless explicitly asked.
- Preserve source encodings and existing EOF newlines. CSSE.rc is UTF-16LE.
- Standard library and third-party C++ includes belong in CSSE/framework.h or CSSE/pch.h, which is forcibly included for every compilation unit.
- Build CSSE.sln with Platform=x86, or CSSE/CSSE.vcxproj with Platform=Win32. Only Release is supported.
- Build using scripts/build.ps1. All outputs must stay in build/ and dist/ unless the user explicitly enables local deployment.
- Keep SE_USE_LUA=0 and SE_IS_MWSE=0. Do not add MWSE, its updater, LuaJIT, Boost or yaml-cpp to the build graph.
- SharedSE code is compiled directly into CSSE.dll. Preserve its TES Construction Set target configuration.
- Run python scripts/verify.py after build-system changes. On Windows, also compile Release and verify the resulting DLL with --binary.
