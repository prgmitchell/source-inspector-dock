[CmdletBinding(SupportsShouldProcess)]
param(
    [string]$Repository = 'prgmitchell/source-inspector-dock',
    [string]$SigningSourceRepository = 'prgmitchell/MIDIMaster'
)

$ErrorActionPreference = 'Stop'
foreach ($repo in @($Repository, $SigningSourceRepository)) {
    if ($repo -notmatch '^[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+$') { throw "Invalid repository name: $repo" }
}
$canonical = & gh repo view $Repository --json nameWithOwner --jq .nameWithOwner
if ($LASTEXITCODE -ne 0) { throw "Create $Repository on GitHub before configuring signing." }
$Repository = $canonical.Trim()
$oidcJson = & gh api "repos/$Repository/actions/oidc/customization/sub"
if ($LASTEXITCODE -ne 0) { throw 'Unable to read the repository OIDC subject configuration.' }
$oidc = $oidcJson | ConvertFrom-Json
if (-not $oidc.use_default -or [string]::IsNullOrWhiteSpace($oidc.sub_claim_prefix)) {
    throw 'Expected default OIDC claims and a repository-provided sub_claim_prefix.'
}
$sourceJson = & gh api "repos/$SigningSourceRepository/environments/windows-release/variables"
if ($LASTEXITCODE -ne 0) { throw 'Unable to read the existing MIDIMaster signing environment.' }
$source = $sourceJson | ConvertFrom-Json
$values = @{}
foreach ($name in @('AZURE_CLIENT_ID', 'AZURE_TENANT_ID', 'AZURE_SUBSCRIPTION_ID',
                    'AZURE_ARTIFACT_SIGNING_ENDPOINT', 'AZURE_ARTIFACT_SIGNING_ACCOUNT', 'AZURE_ARTIFACT_SIGNING_PROFILE')) {
    $value = ($source.variables | Where-Object name -eq $name).value
    if ([string]::IsNullOrWhiteSpace($value)) { throw "Source environment is missing $name." }
    $values[$name] = $value
}
$tenant = & az account show --query tenantId --output tsv
if ($LASTEXITCODE -ne 0 -or $tenant.Trim() -ne $values.AZURE_TENANT_ID) {
    throw 'Use az login with the MIDIMaster signing tenant before running setup.'
}
$credentialsJson = & az ad app federated-credential list --id $values.AZURE_CLIENT_ID --output json
if ($LASTEXITCODE -ne 0) { throw 'Unable to read the Azure application federation configuration.' }
$credentials = $credentialsJson | ConvertFrom-Json
$subject = "$($oidc.sub_claim_prefix):environment:windows-release"
$credentialName = 'github-' + $Repository.Replace('/', '-') + '-windows-release'
$matching = @($credentials | Where-Object subject -eq $subject)
foreach ($credential in $matching) {
    if ($credential.issuer -ne 'https://token.actions.githubusercontent.com' -or
        'api://AzureADTokenExchange' -notin $credential.audiences) {
        throw 'An existing federation for this subject has unexpected issuer/audience settings.'
    }
}

if (-not $PSCmdlet.ShouldProcess($Repository, 'Configure windows-release variables and Azure OIDC federation')) { return }
$temporary = Join-Path ([IO.Path]::GetTempPath()) ('source-inspector-signing-' + [guid]::NewGuid().ToString('N') + '.json')
try {
    # Preserve all protection rules if the environment already exists.
    & gh api "repos/$Repository/environments/windows-release" --silent 2>$null
    if ($LASTEXITCODE -ne 0) {
        @{ deployment_branch_policy = @{ protected_branches = $false; custom_branch_policies = $true } } |
            ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $temporary -Encoding utf8
        & gh api --method PUT "repos/$Repository/environments/windows-release" --input $temporary --silent
        if ($LASTEXITCODE -ne 0) { throw 'Unable to create the windows-release GitHub environment.' }
        foreach ($policy in @(
            @{ name = 'main'; type = 'branch' }, @{ name = 'master'; type = 'branch' },
            @{ name = 'release/*'; type = 'branch' }, @{ name = 'v*'; type = 'tag' }, @{ name = '[0-9]*'; type = 'tag' }
        )) {
            $policy | ConvertTo-Json | Set-Content -LiteralPath $temporary -Encoding utf8
            & gh api --method POST "repos/$Repository/environments/windows-release/deployment-branch-policies" `
                --input $temporary --silent
            if ($LASTEXITCODE -ne 0) { throw 'Unable to create environment branch/tag restriction.' }
        }
    }
    foreach ($name in $values.Keys) {
        & gh variable set $name --repo $Repository --env windows-release --body $values[$name]
        if ($LASTEXITCODE -ne 0) { throw "Unable to set environment variable $name." }
    }
    if (-not $matching.Count) {
        @{
            name = $credentialName
            issuer = 'https://token.actions.githubusercontent.com'
            subject = $subject
            audiences = @('api://AzureADTokenExchange')
            description = 'Source Inspector Dock signed Windows builds'
        } | ConvertTo-Json | Set-Content -LiteralPath $temporary -Encoding utf8
        $ownedCredential = @($credentials | Where-Object name -eq $credentialName)
        if ($ownedCredential.Count) {
            # Update only this script's own credential when GitHub changes the
            # subject format (for example, the immutable repository-ID rollout).
            & az ad app federated-credential update --id $values.AZURE_CLIENT_ID `
                --federated-credential-id $ownedCredential[0].id --parameters "@$temporary" --output none
        } else {
            & az ad app federated-credential create --id $values.AZURE_CLIENT_ID --parameters "@$temporary" --output none
        }
        if ($LASTEXITCODE -ne 0) { throw 'Unable to add Azure federated credential. Application-owner permissions are required.' }
    }
} finally {
    Remove-Item -LiteralPath $temporary -ErrorAction SilentlyContinue
}
Write-Output "Configured $subject using the existing MIDIMaster signing identity."
