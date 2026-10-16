param([ValidateSet('Release','Debug')][string]$Configuration = 'Release')
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
& (Join-Path $PSScriptRoot 'Check-Source.ps1')
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw 'MSVC x64 tools are required on the build host.' }
$build = Join-Path $root 'out/build'
& cmake -S $root -B $build -G 'Visual Studio 17 2022' -A x64
if ($LASTEXITCODE) { throw 'CMake configure failed.' }
& cmake --build $build --config $Configuration --parallel
if ($LASTEXITCODE) { throw 'Build failed.' }
& ctest --test-dir $build -C $Configuration --output-on-failure
if ($LASTEXITCODE) { throw 'Tests failed.' }
& (Join-Path $PSScriptRoot 'Package.ps1') -Configuration $Configuration
