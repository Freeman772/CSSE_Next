[CmdletBinding()]
param(
    [ValidateSet('Release')]
    [string]$Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repoRoot = Split-Path -Parent $PSScriptRoot
$output = Join-Path $repoRoot "build\$Configuration"
$dll = Join-Path $output 'CSSE.dll'
$pdb = Join-Path $output 'CSSE.pdb'
foreach ($file in @($dll, $pdb)) {
    if (-not (Test-Path -LiteralPath $file)) {
        throw "Build CSSE first; missing output: $file"
    }
}

$stageRoot = Join-Path $repoRoot "build\package\$Configuration"
$dist = Join-Path $repoRoot "dist\$Configuration"
if (Test-Path -LiteralPath $stageRoot) {
    Remove-Item -LiteralPath $stageRoot -Recurse -Force
}
New-Item -Path $stageRoot -ItemType Directory -Force | Out-Null
New-Item -Path $dist -ItemType Directory -Force | Out-Null
$runtime = Join-Path $stageRoot 'runtime'
$symbols = Join-Path $stageRoot 'symbols'
New-Item -Path $runtime, $symbols -ItemType Directory -Force | Out-Null

Get-ChildItem -LiteralPath (Join-Path $repoRoot 'package') -Force | ForEach-Object {
    Copy-Item -LiteralPath $_.FullName -Destination $runtime -Recurse -Force
}
Copy-Item -LiteralPath $dll -Destination $runtime
Copy-Item -LiteralPath $pdb -Destination $symbols
Copy-Item -LiteralPath (Join-Path $repoRoot 'LICENSE.txt') -Destination $runtime
Copy-Item -LiteralPath (Join-Path $repoRoot 'THIRD_PARTY_NOTICES.md') -Destination $runtime
$licenses = Join-Path $runtime 'licenses'
New-Item -Path $licenses -ItemType Directory | Out-Null
Copy-Item -LiteralPath (Join-Path $repoRoot 'deps\fmt\LICENSE.txt') -Destination (Join-Path $licenses 'fmt.txt')
Copy-Item -LiteralPath (Join-Path $repoRoot 'deps\toml11\LICENSE') -Destination (Join-Path $licenses 'toml11.txt')

$buildInfo = [ordered]@{
    project = 'CSSE'
    configuration = $Configuration
    architecture = 'x86'
    commit = $env:GITHUB_SHA
    run_number = $env:GITHUB_RUN_NUMBER
    packaged_utc = [DateTime]::UtcNow.ToString('o')
    dll_sha256 = (Get-FileHash -LiteralPath $dll -Algorithm SHA256).Hash.ToLowerInvariant()
}
$buildInfo | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $runtime 'build-info.json') -Encoding UTF8

Add-Type -AssemblyName System.IO.Compression.FileSystem
foreach ($entry in @(
    @{ Folder = $runtime; Name = "CSSE-$Configuration-Win32.zip" },
    @{ Folder = $symbols; Name = "CSSE-$Configuration-Win32-symbols.zip" }
)) {
    $archive = Join-Path $dist $entry.Name
    if (Test-Path -LiteralPath $archive) {
        Remove-Item -LiteralPath $archive -Force
    }
    [IO.Compression.ZipFile]::CreateFromDirectory($entry.Folder, $archive)
    $hash = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant()
    "$hash  $($entry.Name)" | Set-Content -LiteralPath "$archive.sha256" -Encoding ASCII
    Write-Host "Packaged: $archive"
}
