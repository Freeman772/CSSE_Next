[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$BackupFile
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$backup = Get-Content -LiteralPath $BackupFile -Raw -Encoding UTF8 | ConvertFrom-Json
if ($backup.format -ne 'CSSE plugin timestamps v1') { throw 'This is not a CSSE timestamp backup.' }
$files = @($backup.files)
$seen = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
foreach ($file in $files) {
    if (-not [IO.Path]::IsPathRooted($file.path) -or -not $seen.Add($file.path)) { throw 'Invalid or duplicate plugin path in the backup.' }
    if ([IO.Path]::GetExtension($file.path).ToLowerInvariant() -notin @('.esp', '.esm')) { throw 'The backup contains an unsupported file type.' }
    if (-not [IO.File]::Exists($file.path)) { throw "Plugin was not found: $($file.path)" }
    $current = [IO.File]::GetLastWriteTimeUtc($file.path).ToFileTimeUtc()
    if ($current -ne [long]$file.after_filetime) {
        throw "Plugin date changed since this backup was created: $($file.path). No dates were restored."
    }
    [DateTime]::FromFileTimeUtc([long]$file.before_filetime) | Out-Null
}

$restored = @()
try {
    foreach ($file in $files) {
        [IO.File]::SetLastWriteTimeUtc($file.path, [DateTime]::FromFileTimeUtc([long]$file.before_filetime))
        $restored += $file
    }
} catch {
    $failure = $_
    foreach ($file in $restored) {
        try { [IO.File]::SetLastWriteTimeUtc($file.path, [DateTime]::FromFileTimeUtc([long]$file.after_filetime)) }
        catch { Write-Warning "Could not roll back timestamp restoration: $($file.path)" }
    }
    throw $failure
}
Write-Host "Restored timestamps for $($restored.Count) plugins. Reopen the Data Files window."
