param([Parameter(Mandatory)][string]$Letter,[Parameter(Mandatory)][string]$Id)
$ErrorActionPreference = 'Stop'
$start = New-Object Diagnostics.ProcessStartInfo
$start.FileName = Join-Path $PSScriptRoot 'memdisk-access-test.exe'
$start.Arguments = "--hold $Letter"
$start.UseShellExecute = $false
$start.CreateNoWindow = $true
$start.RedirectStandardInput = $true
$start.RedirectStandardOutput = $true
$start.RedirectStandardError = $true
$holder = [Diagnostics.Process]::Start($start)
try {
    $ready = $holder.StandardOutput.ReadLineAsync()
    if (-not $ready.Wait(5000) -or $ready.Result -ne 'HOLD_READY') { throw 'Busy-volume test helper did not become ready.' }
    $text = & (Join-Path $PSScriptRoot 'memdisk.exe') unmount $Id --json
    $code = $LASTEXITCODE
    $reply = ($text -join "`n") | ConvertFrom-Json
    if ($code -eq 0 -or $reply.message -notlike '*FSCTL_LOCK_VOLUME*') {
        throw "Expected safe lock refusal, got: $text"
    }
    $state = & (Join-Path $PSScriptRoot 'memdisk.exe') list --json
    if ($LASTEXITCODE) { throw 'Cannot verify retained instance.' }
    $disk = (($state -join "`n") | ConvertFrom-Json).disks | Where-Object { "$($_.id)" -eq $Id }
    if (-not $disk -or -not $disk.mounted) { throw 'Occupied disk was unexpectedly removed.' }
    Write-Host 'PASS: occupied volume refused safely; instance retained.'
} finally {
    if (-not $holder.HasExited) {
        $holder.StandardInput.WriteLine('release')
        if (-not $holder.WaitForExit(5000)) { $holder.Kill(); $holder.WaitForExit() }
    }
    $holder.Dispose()
}
