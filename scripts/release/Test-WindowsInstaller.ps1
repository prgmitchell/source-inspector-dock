# This test installs only on an ephemeral GitHub runner, never the developer's OBS setup.
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$InstallerPath,
    [Parameter(Mandatory)][string]$StagedDll
)
$ErrorActionPreference = 'Stop'
if ($env:GITHUB_ACTIONS -ne 'true' -or [string]::IsNullOrWhiteSpace($env:RUNNER_TEMP)) {
    throw 'Installer smoke test is restricted to GitHub Actions runners.'
}
$installer = (Resolve-Path -LiteralPath $InstallerPath).Path
$testRoot = Join-Path $env:RUNNER_TEMP ('source-inspector-install-' + [guid]::NewGuid().ToString('N'))
$verify = Join-Path $PSScriptRoot 'Assert-Authenticode.ps1'
$publisher = 'MITCHELL SOFTWARE SOLUTIONS LLC'
& $verify -Path $installer -ExpectedPublisher $publisher -RequireTimestamp | Out-Host
$arguments = '/VERYSILENT /SUPPRESSMSGBOXES /NORESTART /DIR="{0}"' -f $testRoot
$process = Start-Process -FilePath $installer -ArgumentList $arguments -WindowStyle Hidden -Wait -PassThru
if ($process.ExitCode -ne 0) { throw "Installer failed: $($process.ExitCode)" }
$uninstaller = Join-Path $testRoot 'unins000.exe'
try {
    $dll = Join-Path $testRoot 'bin/64bit/source-inspector-dock.dll'
    & $verify -Path @($dll, $uninstaller) -ExpectedPublisher $publisher -RequireTimestamp | Out-Host
    if ((Get-FileHash -LiteralPath $dll).Hash -ne (Get-FileHash -LiteralPath $StagedDll).Hash) {
        throw 'Installed DLL differs from the signed payload.'
    }
    if (-not (Test-Path -LiteralPath (Join-Path $testRoot 'data/locale/en-US.ini'))) {
        throw 'Installed locale is missing.'
    }
} finally {
    if (Test-Path -LiteralPath $uninstaller) {
        $process = Start-Process -FilePath $uninstaller -ArgumentList '/VERYSILENT /SUPPRESSMSGBOXES /NORESTART' `
            -WindowStyle Hidden -Wait -PassThru
        if ($process.ExitCode -ne 0) { throw "Uninstaller failed: $($process.ExitCode)" }
    }
}
if (Test-Path -LiteralPath (Join-Path $testRoot 'bin/64bit/source-inspector-dock.dll')) {
    throw 'Uninstaller left the plugin DLL behind.'
}
Write-Output 'Signed installer, installed DLL, locale, and signed uninstaller verified.'
