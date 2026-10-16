param([Parameter(Mandatory)][string]$Driver, [Parameter(Mandatory)][string]$PublicCertificate)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$sdkBin = "${env:ProgramFiles(x86)}\Windows Kits\10\bin"
$signTool = Join-Path $sdkBin '10.0.26100.0/x64/signtool.exe'
if (-not (Test-Path -LiteralPath $signTool)) {
    $installed = Get-ChildItem -LiteralPath $sdkBin -Directory |
        Where-Object { $_.Name -match '^10\.0\.\d+\.0$' } |
        Sort-Object { [version]$_.Name } -Descending
    $signTool = $installed | ForEach-Object { Join-Path $_.FullName 'x64/signtool.exe' } |
        Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
}
if (-not $signTool -or -not (Test-Path -LiteralPath $signTool)) {
    throw 'Install Windows SDK x64 signing tools on the build host.'
}
$signDir = Join-Path $root 'out/signing'
New-Item -ItemType Directory -Path $signDir -Force | Out-Null
$pfxPath = Join-Path $signDir 'development.pfx'
$passwordPath = Join-Path $signDir 'development.password'
if (-not (Test-Path -LiteralPath $pfxPath)) {
    # Local file certificate only: no build-host certificate store modification.
    $rsa = [System.Security.Cryptography.RSA]::Create(3072)
    $request = [System.Security.Cryptography.X509Certificates.CertificateRequest]::new(
        'CN=MemDisk Development Only', $rsa, [System.Security.Cryptography.HashAlgorithmName]::SHA256,
        [System.Security.Cryptography.RSASignaturePadding]::Pkcs1)
    $oids = [System.Security.Cryptography.OidCollection]::new()
    [void]$oids.Add([System.Security.Cryptography.Oid]::new('1.3.6.1.5.5.7.3.3'))
    $request.CertificateExtensions.Add(
        [System.Security.Cryptography.X509Certificates.X509EnhancedKeyUsageExtension]::new($oids, $false))
    $request.CertificateExtensions.Add(
        [System.Security.Cryptography.X509Certificates.X509KeyUsageExtension]::new(
            [System.Security.Cryptography.X509Certificates.X509KeyUsageFlags]::DigitalSignature, $true))
    $cert = $request.CreateSelfSigned([DateTimeOffset]::Now.AddDays(-1), [DateTimeOffset]::Now.AddYears(2))
    $password = [Guid]::NewGuid().ToString('N')
    [IO.File]::WriteAllBytes($pfxPath, $cert.Export(
        [System.Security.Cryptography.X509Certificates.X509ContentType]::Pfx, $password))
    [IO.File]::WriteAllText($passwordPath, $password)
    $cert.Dispose(); $rsa.Dispose()
}
$password = [IO.File]::ReadAllText($passwordPath)
$cert = [System.Security.Cryptography.X509Certificates.X509Certificate2]::new($pfxPath, $password)
[IO.File]::WriteAllBytes($PublicCertificate, $cert.Export(
    [System.Security.Cryptography.X509Certificates.X509ContentType]::Cert))
$cert.Dispose()
& $signTool sign /fd SHA256 /f $pfxPath /p $password $Driver
if ($LASTEXITCODE) { throw 'Development signing failed.' }
# Self-signed root is intentionally not trusted on the build host.
Write-Host 'Development signature applied. Private key remains in out/signing; never ship it.'
