# Pure mocked state-machine checks: never stop/delete a real service or driver.
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'vm/Uninstall-Helpers.ps1')
function Test-Scenario {
    param([string]$Mode, [string]$Expected, [string]$Actions)
    $script:scenario = $Mode
    $script:removed = [Collections.Generic.List[string]]::new()
    $queryDriver = {
        if ('MemDisk' -in $script:removed -or $script:scenario -eq 'Absent') {
            return [pscustomobject]@{ Exists = $false; Status = 'Missing' }
        }
        $status = if ($script:scenario -eq 'Stopped') { 'Stopped' } else { 'Running' }
        [pscustomobject]@{ Exists = $true; Status = $status }
    }
    $queryDisks = {
        $missing = 'MemDisk' -in $script:removed -or $script:scenario -in @('Absent','Stopped','RunningMissing')
        $errorCode = if ($missing) { 2 } elseif ($script:scenario -eq 'Denied') { 5 } else { 0 }
        $count = if ($script:scenario -eq 'Active' -or
            ($script:scenario -eq 'NewDisk' -and 'MemDiskService' -in $script:removed)) { 1 } else { 0 }
        [pscustomobject]@{
            ExitCode = [int]($errorCode -ne 0); Error = $errorCode; Count = $count; Message = 'injected'
        }
    }
    $remove = {
        param($name)
        if ($script:scenario -eq 'StopFailure') { throw 'stop failed' }
        $script:removed.Add($name)
    }
    $failure = ''
    try { Invoke-MdUninstallPlan $queryDriver $queryDisks $remove }
    catch { $failure = $_.Exception.Message }
    if (($Expected -eq '' -and $failure -ne '') -or
        ($Expected -ne '' -and $failure -notlike "*$Expected*")) {
        throw "Scenario $Mode unexpected result: $failure"
    }
    if (($script:removed -join ',') -ne $Actions) { throw "Scenario $Mode performed unexpected actions." }
    Write-Host "PASS: $Mode"
}
Test-Scenario 'Stopped' '' 'MemDiskService,MemDisk'
Test-Scenario 'Absent' '' 'MemDiskService,MemDisk'
Test-Scenario 'Running' '' 'MemDiskService,MemDisk'
Test-Scenario 'Active' 'Active RAM disks' ''
Test-Scenario 'Denied' 'Cannot establish driver state' ''
Test-Scenario 'RunningMissing' 'Cannot establish driver state' ''
Test-Scenario 'NewDisk' 'Active RAM disks' 'MemDiskService'
Test-Scenario 'StopFailure' 'stop failed' ''

function Test-Cli {
    param($Operation, $Format)
    $global:LASTEXITCODE = $script:cliExit
    $script:cliJson
}
foreach ($invalid in @('{}','not-json','{"version":1,"error":0}',
    '{"version":1,"error":false,"disks":[]}', '{"version":1,"error":0,"disks":null}')) {
    $script:cliExit = 0; $script:cliJson = $invalid
    $failed = $false
    try { Read-MdDiskState 'Test-Cli' | Out-Null } catch { $failed = $true }
    if (-not $failed) { throw 'Invalid driver response accepted.' }
}
$script:cliExit = 1; $script:cliJson = '{"version":1,"error":2,"disks":[],"message":"Open driver"}'
$probe = Read-MdDiskState 'Test-Cli'
if ($probe.Error -ne 2 -or $probe.ExitCode -ne 1 -or $probe.Count -ne 0) { throw 'Missing device parse failed.' }
Write-Host 'PASS: malformed protocol rejected; missing control device preserved as explicit error 2.'
exit 0
