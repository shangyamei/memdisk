$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$paths = @('driver', 'include', 'src', 'tests', 'scripts', 'cmake', 'docs')
$files = @($paths | ForEach-Object {
    $path = Join-Path $root $_
    if (Test-Path -LiteralPath $path) { Get-ChildItem -LiteralPath $path -Recurse -File }
}) + @(Get-ChildItem -LiteralPath $root -File)
$failures = @()
foreach ($file in $files) {
    $lines = @(Get-Content -LiteralPath $file.FullName).Count
    if ($lines -gt 500) { $failures += "$($file.FullName): $lines lines" }
}
if ($failures.Count) { throw ($failures -join "`n") }
Write-Host "Line limit passed: $($files.Count) authored files, each <=500 lines."
