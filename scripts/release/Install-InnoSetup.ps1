[CmdletBinding()]
param([string]$Destination = (Join-Path $PSScriptRoot '../../.deps/inno-setup'))

$ErrorActionPreference = 'Stop'
$destinationPath = [IO.Path]::GetFullPath($Destination)
$compiler = Join-Path $destinationPath 'ISCC.exe'
if (-not (Test-Path -LiteralPath $compiler -PathType Leaf)) {
    $download = Join-Path ([IO.Path]::GetTempPath()) ('innosetup-' + [guid]::NewGuid().ToString('N') + '.exe')
    try {
        Invoke-WebRequest 'https://github.com/jrsoftware/issrc/releases/download/is-6_4_3/innosetup-6.4.3.exe' -OutFile $download
        $expected = 'f3c42116542c4cc57263c5ba6c4feabfc49fe771f2f98a79d2f7628b8762723b'
        if ((Get-FileHash -LiteralPath $download -Algorithm SHA256).Hash.ToLowerInvariant() -ne $expected) {
            throw 'Inno Setup download checksum mismatch.'
        }
        $arguments = '/VERYSILENT /SUPPRESSMSGBOXES /NORESTART /CURRENTUSER /NOICONS /DIR="{0}"' -f $destinationPath
        $process = Start-Process -FilePath $download -ArgumentList $arguments -WindowStyle Hidden -Wait -PassThru
        if ($process.ExitCode -ne 0) { throw "Inno Setup installation failed: $($process.ExitCode)" }
    } finally {
        if (Test-Path -LiteralPath $download) { Remove-Item -LiteralPath $download }
    }
}
if (-not (Test-Path -LiteralPath $compiler -PathType Leaf)) { throw 'ISCC.exe is missing.' }
Write-Output $compiler
