[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repoRoot = Split-Path -Parent $PSScriptRoot
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$installation = & $vswhere -latest -products '*' -version '[17.0,18.0)' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($installation)) {
    throw 'Visual Studio 2022 C++ tools were not found.'
}
$vcvars = Join-Path $installation 'VC\Auxiliary\Build\vcvarsall.bat'
$testDir = Join-Path $repoRoot 'build\tests'
New-Item -Path $testDir -ItemType Directory -Force | Out-Null
$batch = Join-Path $testDir 'compile-and-run.cmd'
$commands = @"
@echo off
chcp 65001 >nul
call "$vcvars" x86 >nul
if errorlevel 1 exit /b %errorlevel%
pushd "$testDir"
cl.exe /nologo /std:c++20 /EHsc /MT /utf-8 /W4 /WX /FI"$repoRoot\tests\portable_pch.h" /I"$repoRoot\CSSE" "$repoRoot\CSSE\PluginLoadOrder.cpp" "$repoRoot\tests\plugin_load_order.cpp" /Fe:plugin-load-order.exe /Fo.\
if errorlevel 1 exit /b %errorlevel%
plugin-load-order.exe
exit /b %errorlevel%
"@
[IO.File]::WriteAllText($batch, $commands.Replace("`r`n", "`n").Replace("`n", "`r`n"), [Text.UTF8Encoding]::new($false))
& $env:ComSpec /d /c "`"$batch`""
if ($LASTEXITCODE -ne 0) { throw "Load-order tests failed with exit code $LASTEXITCODE." }
