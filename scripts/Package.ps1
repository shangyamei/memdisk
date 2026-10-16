param([ValidateSet('Release','Debug')][string]$Configuration = 'Release')
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$dist = Join-Path $root 'dist/MemDisk-0.1.6-dev-win-x64'
New-Item -ItemType Directory -Path $dist -Force | Out-Null
$binary = Join-Path $root "out/build/$Configuration"
foreach ($name in @('memdisk.sys','memdisk.exe','memdisk-service.exe','memdisk-ui.exe','memdisk-access-test.exe')) {
    Copy-Item -LiteralPath (Join-Path $binary $name) -Destination $dist -Force
}
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'vm/Install.ps1') -Destination $dist -Force
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'vm/Uninstall.ps1') -Destination $dist -Force
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'vm/Verify.ps1') -Destination $dist -Force
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'vm/Test-BusyVolume.ps1') -Destination $dist -Force
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'vm/Check-Package.ps1') -Destination $dist -Force
Copy-Item -LiteralPath (Join-Path $root 'docs/VM-VALIDATION.md') -Destination (Join-Path $dist 'README-VM.md') -Force
Copy-Item -LiteralPath (Join-Path $root 'docs/STATUS.md') -Destination $dist -Force
Copy-Item -LiteralPath (Join-Path $root 'docs/CAPACITY-UPDATE.md') -Destination $dist -Force
Copy-Item -LiteralPath (Join-Path $root 'docs/UPGRADE-0.1.1.md') -Destination $dist -Force
Copy-Item -LiteralPath (Join-Path $root 'docs/UPGRADE-0.1.2.md') -Destination $dist -Force
Copy-Item -LiteralPath (Join-Path $root 'docs/UPGRADE-0.1.3.md') -Destination $dist -Force
Copy-Item -LiteralPath (Join-Path $root 'docs/UPGRADE-0.1.4.md') -Destination $dist -Force
Copy-Item -LiteralPath (Join-Path $root 'docs/UPGRADE-0.1.5.md') -Destination $dist -Force
& (Join-Path $PSScriptRoot 'Sign.ps1') -Driver (Join-Path $dist 'memdisk.sys') -PublicCertificate (Join-Path $dist 'MemDisk-Development.cer')
$licenseDir = Join-Path $dist 'licenses'
New-Item -ItemType Directory -Path $licenseDir -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $root 'out/build/_deps/imgui-src/LICENSE.txt') -Destination (Join-Path $licenseDir 'imgui.txt') -Force
Copy-Item -LiteralPath (Join-Path $root 'out/build/_deps/implot-src/LICENSE') -Destination (Join-Path $licenseDir 'implot.txt') -Force
$hashes = Get-ChildItem -LiteralPath $dist -Recurse -File |
    Where-Object { $_.Name -ne 'SHA256SUMS.json' } | ForEach-Object {
        [pscustomobject]@{ Path = $_.FullName.Substring($dist.Length + 1); SHA256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash }
    }
$hashes | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $dist 'SHA256SUMS.json') -Encoding UTF8
$zip = "$dist.zip"
Compress-Archive -Path (Join-Path $dist '*') -DestinationPath $zip -Force
Write-Host "VM package: $zip"
