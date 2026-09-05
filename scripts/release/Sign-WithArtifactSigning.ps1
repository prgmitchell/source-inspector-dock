# Adapted from MIDIMaster's Artifact Signing integration.
[CmdletBinding()]
param([Parameter(Mandatory, Position = 0)][string]$Path)

$ErrorActionPreference = 'Stop'
$requiredModuleVersion = '0.1.17'
$publisher = 'MITCHELL SOFTWARE SOLUTIONS LLC'
foreach ($name in @('AZURE_ARTIFACT_SIGNING_ENDPOINT', 'AZURE_ARTIFACT_SIGNING_ACCOUNT', 'AZURE_ARTIFACT_SIGNING_PROFILE')) {
    if ([string]::IsNullOrWhiteSpace([Environment]::GetEnvironmentVariable($name))) {
        throw "Missing Artifact Signing environment variable: $name"
    }
}
$endpoint = $env:AZURE_ARTIFACT_SIGNING_ENDPOINT.TrimEnd('/')
if ($endpoint -ne 'https://eus.codesigning.azure.net') {
    throw 'Expected the same East US signing endpoint as MIDIMaster.'
}
$resolved = (Resolve-Path -LiteralPath $Path).Path
$extension = [IO.Path]::GetExtension($resolved).ToLowerInvariant()
$innoTemporary = [IO.Path]::GetFileName($resolved) -match '^(uninst|setup)\.e(32|64)\.tmp$'
if ($extension -notin @('.dll', '.exe', '.e32', '.e64') -and -not $innoTemporary) {
    throw "Unsupported signing input: $resolved"
}
$stream = [IO.File]::OpenRead($resolved)
try {
    if ($stream.ReadByte() -ne 0x4D -or $stream.ReadByte() -ne 0x5A) {
        throw "Signing input is not a Windows executable: $resolved"
    }
} finally { $stream.Dispose() }

Import-Module ArtifactSigning -RequiredVersion $requiredModuleVersion -Force -ErrorAction Stop
$correlation = if ($env:GITHUB_RUN_ID) {
    "SourceInspector-GitHub-$($env:GITHUB_RUN_ID)-$($env:GITHUB_RUN_ATTEMPT)"
} else { 'SourceInspector-Local' }

Invoke-ArtifactSigning `
    -Endpoint $endpoint `
    -CodeSigningAccountName $env:AZURE_ARTIFACT_SIGNING_ACCOUNT `
    -CertificateProfileName $env:AZURE_ARTIFACT_SIGNING_PROFILE `
    -Files $resolved `
    -FileDigest SHA256 `
    -TimestampRfc3161 'http://timestamp.acs.microsoft.com' `
    -TimestampDigest SHA256 `
    -Description 'Source Inspector Dock' `
    -DescriptionUrl 'https://github.com/prgmitchell/source-inspector-dock' `
    -CorrelationId $correlation `
    -ExcludeEnvironmentCredential `
    -ExcludeWorkloadIdentityCredential `
    -ExcludeManagedIdentityCredential `
    -ExcludeSharedTokenCacheCredential `
    -ExcludeVisualStudioCredential `
    -ExcludeVisualStudioCodeCredential `
    -ExcludeAzurePowerShellCredential `
    -ExcludeAzureDeveloperCliCredential `
    -ExcludeInteractiveBrowserCredential | Out-Host

& (Join-Path $PSScriptRoot 'Assert-Authenticode.ps1') -Path $resolved `
    -ExpectedPublisher $publisher -RequireTimestamp | Out-Host

# Inno deletes its temporary uninstaller after embedding it. Keep the exact signed
# binary for the packager's verification; CI also verifies the installed copy.
if ($innoTemporary -and $env:SID_SIGNED_UNINSTALLER_COPY) {
    Copy-Item -LiteralPath $resolved -Destination $env:SID_SIGNED_UNINSTALLER_COPY -Force
}
