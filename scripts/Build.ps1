param(
    [ValidateSet('Release','Debug')][string]$Configuration = 'Release',
    [ValidateSet('Auto','2022','2026')][string]$VisualStudio = 'Auto',
    [ValidatePattern('^$|^10\.0\.\d+\.0$')][string]$WdkVersion = ''
)
$ErrorActionPreference = 'Stop'
# Windows PowerShell can corrupt native UTF-8 JSON; packaging also requires PS7.
if ($PSVersionTable.PSVersion.Major -lt 7) {
    $pwsh = Get-Command pwsh -CommandType Application -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if (-not $pwsh) {
        throw 'PowerShell 7 is required. Install it, then run: pwsh -NoProfile -File .\scripts\Build.ps1'
    }
    Write-Host 'Restarting build with PowerShell 7.'
    & $pwsh.Source -NoProfile -File $PSCommandPath @PSBoundParameters
    exit $LASTEXITCODE
}
$root = Split-Path $PSScriptRoot -Parent
& (Join-Path $PSScriptRoot 'Check-Source.ps1')
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path -LiteralPath $vswhere)) { throw 'Visual Studio Installer / vswhere is required.' }
$range = switch ($VisualStudio) {
    '2022' { '[17.0,18.0)' }
    '2026' { '[18.0,19.0)' }
    default { '[17.0,19.0)' }
}
# Decode the redirected stream explicitly: even PS7 can inherit a legacy code page.
$start = [System.Diagnostics.ProcessStartInfo]::new($vswhere)
$start.UseShellExecute = $false
$start.CreateNoWindow = $true
$start.RedirectStandardOutput = $true
$start.StandardOutputEncoding = [System.Text.UTF8Encoding]::new($false, $true)
foreach ($argument in @('-latest', '-products', '*', '-version', $range,
    '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-format', 'json', '-utf8')) {
    $start.ArgumentList.Add($argument)
}
$discovery = [System.Diagnostics.Process]::new()
try {
    $discovery.StartInfo = $start
    if (-not $discovery.Start()) { throw 'Cannot start Visual Studio discovery.' }
    $instances = $discovery.StandardOutput.ReadToEnd()
    $discovery.WaitForExit()
    if ($discovery.ExitCode) { throw "Visual Studio discovery failed ($($discovery.ExitCode))." }
} finally {
    $discovery.Dispose()
}
$vs = @($instances | ConvertFrom-Json)[0]
if (-not $vs) { throw "Visual Studio $VisualStudio with MSVC x64 tools is required." }
$major = ([version]$vs.installationVersion).Major
$generator = if ($major -eq 18) { 'Visual Studio 18 2026' } else { 'Visual Studio 17 2022' }
$minimumCMake = if ($major -eq 18) { [version]'4.2' } else { [version]'3.24' }
$cmakeVersion = & cmake --version
if ($LASTEXITCODE -or $cmakeVersion[0] -notmatch 'cmake version (\d+\.\d+\.\d+)') {
    throw 'Cannot determine CMake version.'
}
if ([version]$Matches[1] -lt $minimumCMake) {
    throw "$generator requires CMake $minimumCMake or newer. Upgrade CMake or select -VisualStudio 2022."
}
$build = Join-Path $root 'out/build'
$configureArgs = @('-S', $root, '-B', $build, '-G', $generator, '-A', 'x64',
    "-DCMAKE_GENERATOR_INSTANCE:INTERNAL=$($vs.installationPath)")
$cache = Join-Path $build 'CMakeCache.txt'
if (Test-Path -LiteralPath $cache) {
    $previous = Get-Content -LiteralPath $cache
    $oldGenerator = $previous | Where-Object { $_ -like 'CMAKE_GENERATOR:INTERNAL=*' }
    $oldInstance = $previous | Where-Object { $_ -like 'CMAKE_GENERATOR_INSTANCE:INTERNAL=*' }
    if ($oldGenerator -ne "CMAKE_GENERATOR:INTERNAL=$generator" -or
        $oldInstance -ne "CMAKE_GENERATOR_INSTANCE:INTERNAL=$($vs.installationPath)") {
        # Refresh CMake metadata when switching VS; retain downloaded _deps sources.
        $configureArgs += '--fresh'
    }
}
Write-Host "Build toolchain: $generator ($($vs.installationPath))"
if ($WdkVersion) { $configureArgs += "-DWDK_VERSION:STRING=$WdkVersion" }
& cmake @configureArgs
if ($LASTEXITCODE) { throw 'CMake configure failed.' }
& cmake --build $build --config $Configuration --parallel
if ($LASTEXITCODE) { throw 'Build failed.' }
& ctest --test-dir $build -C $Configuration --output-on-failure
if ($LASTEXITCODE) { throw 'Tests failed.' }
& (Join-Path $PSScriptRoot 'Package.ps1') -Configuration $Configuration
