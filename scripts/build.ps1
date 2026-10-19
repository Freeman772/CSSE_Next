[CmdletBinding()]
param(
    [ValidateSet('Release')]
    [string]$Configuration = 'Release',
    [ValidateSet('v142', 'v143')]
    [string]$PlatformToolset = 'v143',
    [ValidatePattern('^\d+$')]
    [string]$BuildNumber = '0',
    [ValidatePattern('^\d{8}$')]
    [string]$BuildDate = [DateTime]::UtcNow.ToString('yyyyMMdd'),
    [switch]$Rebuild
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repoRoot = Split-Path -Parent $PSScriptRoot
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) {
    throw 'Visual Studio Installer was not found. Install Visual Studio 2022 / Build Tools with Desktop development with C++ and C++ MFC (x86 and x64).'
}

$vsRange = if ($PlatformToolset -eq 'v142') { '[16.0,17.0)' } else { '[17.0,18.0)' }
$installation = & $vswhere -latest -products '*' -version $vsRange -requires Microsoft.Component.MSBuild Microsoft.VisualStudio.Component.VC.Tools.x86.x64 Microsoft.VisualStudio.Component.VC.ATLMFC -property installationPath
if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($installation)) {
    throw "Visual Studio with MSBuild, C++ tools and MFC was not found for $PlatformToolset. Add these components in Visual Studio Installer."
}
$msbuild = Join-Path $installation 'MSBuild\Current\Bin\MSBuild.exe'
if (-not (Test-Path -LiteralPath $msbuild)) {
    throw "MSBuild was not found: $msbuild"
}

$target = if ($Rebuild) { 'Rebuild' } else { 'Build' }
$buildArguments = @(
    (Join-Path $repoRoot 'CSSE.sln'),
    "/t:$target", '/m', '/nr:false', '/v:minimal',
    "/p:Configuration=$Configuration", '/p:Platform=x86',
    "/p:PlatformToolset=$PlatformToolset", "/p:CSSEBuildNumber=$BuildNumber",
    "/p:CSSEBuildDate=$BuildDate"
)
Write-Host "Building CSSE $Configuration / Win32 using $PlatformToolset"
& $msbuild @buildArguments
if ($LASTEXITCODE -ne 0) {
    throw "CSSE build failed with exit code $LASTEXITCODE."
}

$dll = Join-Path $repoRoot "build\$Configuration\CSSE.dll"
$pdb = Join-Path $repoRoot "build\$Configuration\CSSE.pdb"
foreach ($output in @($dll, $pdb)) {
    if (-not (Test-Path -LiteralPath $output)) {
        throw "Expected build output was not produced: $output"
    }
}
Write-Host "Built: $dll"
