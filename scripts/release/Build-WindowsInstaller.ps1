[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$SourceDirectory,
    [string]$OutputDirectory = 'release',
    [string]$CompilerPath,
    [switch]$Unsigned
)

$ErrorActionPreference = 'Stop'
$root = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$source = (Resolve-Path -LiteralPath $SourceDirectory).Path
$output = [IO.Path]::GetFullPath($OutputDirectory)
$spec = Get-Content -LiteralPath (Join-Path $root 'buildspec.json') -Raw | ConvertFrom-Json
if ($spec.version -notmatch '^\d+\.\d+\.\d+$') { throw 'Installer requires a three-part numeric buildspec version.' }
foreach ($relative in @('bin/64bit/source-inspector-dock.dll', 'data/locale/en-US.ini', 'LICENSE', 'README.md')) {
    if (-not (Test-Path -LiteralPath (Join-Path $source $relative) -PathType Leaf)) {
        throw "Missing installer input: $relative"
    }
}
New-Item -ItemType Directory -Force -Path $output | Out-Null
if (-not $CompilerPath) {
    $candidates = @(
        (Join-Path $env:LOCALAPPDATA 'Programs/Inno Setup 6/ISCC.exe'),
        (Join-Path ${env:ProgramFiles(x86)} 'Inno Setup 6/ISCC.exe'),
        (Join-Path $root '.deps/inno-setup/ISCC.exe')
    )
    $CompilerPath = $candidates | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1
}
if (-not $CompilerPath) { throw 'Install Inno Setup 6 or pass -CompilerPath.' }
$arguments = @("/DSourceDir=$source", "/DOutputDir=$output", "/DPackageVersion=$($spec.version)")
if ($Unsigned) {
    $arguments += '/DSignedBuild=0'
} else {
    & (Join-Path $PSScriptRoot 'Assert-Authenticode.ps1') `
        -Path (Join-Path $source 'bin/64bit/source-inspector-dock.dll') `
        -ExpectedPublisher 'MITCHELL SOFTWARE SOLUTIONS LLC' -RequireTimestamp | Out-Host
    $pwsh = (Get-Command pwsh -ErrorAction Stop).Source
    $signer = Join-Path $PSScriptRoot 'Sign-WithArtifactSigning.ps1'
    # $q and $f are Inno Setup placeholders, not PowerShell interpolation.
    $signCommand = '$q{0}$q -NoProfile -NonInteractive -File $q{1}$q -Path $f' -f $pwsh, $signer
    $arguments += '/DSignedBuild=1', "/Sartifact=$signCommand"
}
$arguments += (Join-Path $root 'installer/source-inspector-dock.iss')
$previousCopyPath = $env:SID_SIGNED_UNINSTALLER_COPY
try {
    if (-not $Unsigned) {
        $uninstallerDirectory = Join-Path $output 'signed-uninstaller'
        New-Item -ItemType Directory -Force -Path $uninstallerDirectory | Out-Null
        $env:SID_SIGNED_UNINSTALLER_COPY = Join-Path $uninstallerDirectory 'verified-uninstaller.exe'
    }
    & $CompilerPath @arguments
} finally { $env:SID_SIGNED_UNINSTALLER_COPY = $previousCopyPath }
if ($LASTEXITCODE -ne 0) { throw "Inno Setup failed with exit code $LASTEXITCODE." }
$suffix = if ($Unsigned) { '-unsigned' } else { '' }
$installer = Join-Path $output "source-inspector-dock-$($spec.version)-windows-x64-setup$suffix.exe"
if (-not (Test-Path -LiteralPath $installer -PathType Leaf)) { throw 'Installer was not produced.' }
if (-not $Unsigned) {
    $uninstallers = @(Get-ChildItem -LiteralPath (Join-Path $output 'signed-uninstaller') -File |
        Where-Object { $_.Extension -in @('.exe', '.e32', '.e64') })
    if (-not $uninstallers.Count) { throw 'No signed uninstaller was produced.' }
    & (Join-Path $PSScriptRoot 'Assert-Authenticode.ps1') `
        -Path (@($installer) + @($uninstallers.FullName)) `
        -ExpectedPublisher 'MITCHELL SOFTWARE SOLUTIONS LLC' -RequireTimestamp | Out-Host
}
Write-Output $installer
