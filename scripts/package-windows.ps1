[CmdletBinding()]
param(
    [string]$BuildDirectory = 'build_x64',
    [ValidateSet('Release', 'RelWithDebInfo')][string]$Configuration = 'RelWithDebInfo',
    [switch]$Install,
    [string]$PluginDirectory = (Join-Path $env:ProgramData 'obs-studio\plugins')
)

$ErrorActionPreference = 'Stop'
$projectDirectory = Split-Path $PSScriptRoot -Parent
$buildPath = [IO.Path]::GetFullPath((Join-Path $projectDirectory $BuildDirectory))
$spec = Get-Content -LiteralPath (Join-Path $projectDirectory 'buildspec.json') -Raw | ConvertFrom-Json

function Remove-LegacyPluginFiles([string]$PluginPath) {
    # Remove only this plugin's old binaries, preserving any other files.
    foreach ($extension in @('dll', 'pdb')) {
        $legacyFile = Join-Path $PluginPath "bin\64bit\$($spec.name).$extension"
        if (Test-Path -LiteralPath $legacyFile -PathType Leaf) {
            Remove-Item -LiteralPath $legacyFile -Force
        }
    }
    foreach ($relative in @('bin\64bit', 'bin')) {
        $directory = Join-Path $PluginPath $relative
        if ((Test-Path -LiteralPath $directory -PathType Container) -and
            -not (Get-ChildItem -LiteralPath $directory -Force | Select-Object -First 1)) {
            Remove-Item -LiteralPath $directory
        }
    }
}

$releasePath = Join-Path $projectDirectory 'release'
New-Item -ItemType Directory -Force -Path $releasePath | Out-Null

& cmake --install $buildPath --config $Configuration --prefix $releasePath
if ($LASTEXITCODE -ne 0) { throw 'Packaging failed.' }
$pluginPath = Join-Path $releasePath $spec.name
$builtDll = Join-Path $pluginPath "$($spec.name).dll"
if (-not (Test-Path -LiteralPath $builtDll -PathType Leaf)) {
    throw 'Expected an OBS 33 plugin layout. Reconfigure and rebuild before packaging.'
}
Remove-LegacyPluginFiles $pluginPath
Copy-Item -LiteralPath (Join-Path $projectDirectory 'LICENSE') -Destination $pluginPath -Force
Copy-Item -LiteralPath (Join-Path $projectDirectory 'README.md') -Destination $pluginPath -Force

$archive = Join-Path $releasePath "$($spec.name)-$($spec.version)-windows-x64.zip"
Compress-Archive -LiteralPath $pluginPath -DestinationPath $archive -Force
Write-Output "Package: $archive"

if ($Install) {
    $installedPlugin = Join-Path $PluginDirectory $spec.name
    New-Item -ItemType Directory -Force -Path $PluginDirectory | Out-Null
    & cmake --install $buildPath --config $Configuration --prefix $PluginDirectory
    if ($LASTEXITCODE -ne 0) { throw 'Installation failed. Close OBS if the DLL is in use, then retry.' }
    Copy-Item -LiteralPath (Join-Path $projectDirectory 'LICENSE') -Destination $installedPlugin -Force
    Copy-Item -LiteralPath (Join-Path $projectDirectory 'README.md') -Destination $installedPlugin -Force
    $installedDll = Join-Path $installedPlugin "$($spec.name).dll"
    if ((Get-FileHash -LiteralPath $builtDll).Hash -ne (Get-FileHash -LiteralPath $installedDll).Hash) {
        throw 'Installed DLL does not match the packaged build.'
    }
    Remove-LegacyPluginFiles $installedPlugin
    Write-Output "Installed and verified: $installedPlugin"
    Write-Output 'Restart OBS, then enable Docks > Source Inspector.'
}
