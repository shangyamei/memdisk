# Built-in Windows PowerShell 5.1 compatible. No actions run when dot-sourced.
function Get-MdServiceState {
    param([ValidateSet('MemDisk','MemDiskService')][string]$Name)
    $ErrorActionPreference = 'Stop'
    # Get-Service enumeration omits device drivers on some PowerShell versions.
    $items = if ($Name -eq 'MemDisk') {
        [System.ServiceProcess.ServiceController]::GetDevices()
    } else { [System.ServiceProcess.ServiceController]::GetServices() }
    try {
        foreach ($item in $items) {
            if ($item.ServiceName -eq $Name) {
                $item.Refresh()
                return [pscustomobject]@{ Exists = $true; Status = $item.Status.ToString() }
            }
        }
        return [pscustomobject]@{ Exists = $false; Status = 'Missing' }
    } finally {
        # Do not keep SCM handles open across DeleteService.
        foreach ($item in $items) { $item.Dispose() }
    }
}

function Read-MdDiskState {
    param([string]$Cli)
    $raw = @(& $Cli driver-list --json)
    $exitCode = $LASTEXITCODE
    try { $reply = ($raw -join "`n") | ConvertFrom-Json -ErrorAction Stop }
    catch { throw "Invalid driver-list response (exit=$exitCode): $($raw -join ' ')" }
    $names = @($reply.PSObject.Properties.Name)
    if ($null -eq $reply -or $reply.version -ne 1 -or
        'error' -notin $names -or 'disks' -notin $names -or
        ($reply.error -isnot [int] -and $reply.error -isnot [long]) -or
        $reply.disks -isnot [Array] -or
        @($reply.disks).Count -gt 8) {
        throw "Invalid driver-list protocol (exit=$exitCode)."
    }
    [pscustomobject]@{
        ExitCode = $exitCode; Error = [long]$reply.error
        Count = @($reply.disks).Count; Message = [string]$reply.message
    }
}

function Assert-MdDriverSafe {
    param($Driver, $Disks)
    if ($Disks.ExitCode -eq 0 -and $Disks.Error -eq 0) {
        if ($Disks.Count -ne 0) {
            throw 'Active RAM disks exist. Destroy them explicitly before uninstalling.'
        }
        if (-not $Driver.Exists -or $Driver.Status -ne 'Running') {
            throw 'Control device exists but SCM driver state does not match. Preserve state; reboot and retry.'
        }
        return
    }
    # ERROR_FILE_NOT_FOUND alone is insufficient: SCM must independently confirm
    # that the driver is stopped/absent. Access denial and protocol errors fail closed.
    if ($Disks.ExitCode -ne 0 -and $Disks.Error -eq 2 -and $Disks.Count -eq 0 -and
        ((-not $Driver.Exists) -or $Driver.Status -eq 'Stopped')) { return }
    throw "Cannot establish driver state (SCM=$($Driver.Status), exit=$($Disks.ExitCode), error=$($Disks.Error)): $($Disks.Message)"
}

function Remove-MdRegistration {
    param([ValidateSet('MemDisk','MemDiskService')][string]$Name)
    $state = Get-MdServiceState $Name
    if (-not $state.Exists) { Write-Host "$Name registration already absent."; return }
    if ($state.Status -ne 'Stopped') {
        if ($state.Status -ne 'StopPending') {
            $output = @(& sc.exe stop $Name)
            $code = $LASTEXITCODE
            if ($code -notin @(0,1062,1060)) {
                throw "Cannot stop $Name (SC=$code): $($output -join ' ')"
            }
        }
        $controller = [System.ServiceProcess.ServiceController]::new($Name)
        try { $controller.WaitForStatus('Stopped',[TimeSpan]::FromSeconds(20)) }
        finally { $controller.Dispose() }
    }
    $output = @(& sc.exe delete $Name)
    $code = $LASTEXITCODE
    if ($code -notin @(0,1060)) {
        throw "Cannot remove $Name registration (SC=$code): $($output -join ' '). Close service tools, reboot and retry."
    }
    if ((Get-MdServiceState $Name).Exists) {
        throw "$Name deletion is pending. Close service tools, reboot and retry; do not reinstall yet."
    }
}

function Invoke-MdUninstallPlan {
    param([scriptblock]$QueryDriver, [scriptblock]$QueryDisks, [scriptblock]$Remove)
    Assert-MdDriverSafe (& $QueryDriver) (& $QueryDisks)
    & $Remove 'MemDiskService'
    # A second check catches new disks/state changes while stopping the service.
    Assert-MdDriverSafe (& $QueryDriver) (& $QueryDisks)
    & $Remove 'MemDisk'
    $driver = & $QueryDriver
    $disks = & $QueryDisks
    if ($driver.Exists -or $disks.ExitCode -eq 0 -or $disks.Error -ne 2 -or $disks.Count -ne 0) {
        throw 'Driver removal could not be confirmed. Preserve state; close service tools, reboot and retry.'
    }
}
