# Signed Windows releases

The **Signed Windows Installer** workflow uses MIDIMaster's Azure Artifact Signing
account and certificate profile with GitHub OIDC. No PFX, certificate password, or
Azure client secret is copied into this repository.

Every push to `main`, `master`, or `release/**` builds and tests the plugin, then
uploads a signed Windows installer and portable ZIP to that workflow's artifacts.
A matching version tag, such as `v1.0.0` (or `1.0.0`), also creates a **draft GitHub
release** with those files and `SHA256SUMS.txt`. Publish the draft when ready.
Update `buildspec.json` before tagging. Actions also supports manual runs.

Pull requests build without access to the signing environment. macOS/Linux still
use template CI independently. The old template release publisher is disabled to
avoid duplicate releases or unsigned assets being published beside signed builds.

## One-time setup

After creating the GitHub repository, authenticate `gh` as a repository administrator
and `az` as an owner of MIDIMaster's signing application, then run:

```powershell
./scripts/release/Setup-GitHubSigning.ps1
```

This copies six non-secret variables from MIDIMaster's `windows-release` environment:

| Variable | Purpose |
| --- | --- |
| AZURE_CLIENT_ID | Existing Entra signing application |
| AZURE_TENANT_ID | Azure tenant |
| AZURE_SUBSCRIPTION_ID | Signing subscription |
| AZURE_ARTIFACT_SIGNING_ENDPOINT | `https://eus.codesigning.azure.net` |
| AZURE_ARTIFACT_SIGNING_ACCOUNT | Existing signing account |
| AZURE_ARTIFACT_SIGNING_PROFILE | Existing public certificate profile |

It adds this Azure federated identity without replacing MIDIMaster's trust:

```text
Issuer:   https://token.actions.githubusercontent.com
Subject:  repo:prgmitchell@86465454/source-inspector-dock@1358579005:environment:windows-release
Audience: api://AzureADTokenExchange
```

The setup script reads GitHub's actual `sub_claim_prefix` rather than guessing it.
New repositories use immutable owner/repository IDs in their subject claims;
older MIDIMaster repositories can still use the previous name-only format.

New environments restrict deployments to main/master, release branches, and version
tags. Existing protections are preserved. The signing app retains its existing
certificate-profile signer role; setup grants no new Azure roles. Repository
variables alone are insufficient: Azure must authorize the new OIDC subject.

## Signing and verification

1. Build and run the libobs/Qt integration tests, then stage the plugin.
2. Sign the staged DLL with `azure/artifact-signing-action@v2` after `azure/login@v3`.
3. Build an Inno Setup installer. Its signing callback uses the pinned
   `ArtifactSigning` PowerShell module **0.1.17**, as in MIDIMaster, to sign the
   uninstaller and installer with SHA-256 and Microsoft's RFC3161 timestamp service.
4. Require valid signatures, timestamps, and the exact publisher
   **MITCHELL SOFTWARE SOLUTIONS LLC** on all three binaries. Errors fail the build;
   there is no unsigned fallback in the release workflow.
5. Install on the disposable Windows runner, verify the installed DLL and uninstaller,
   compare the payload hash, and uninstall it.
6. ZIP the already-signed staged plugin and calculate checksums from final files.

The installer targets `C:\ProgramData\obs-studio\plugins\source-inspector-dock`,
requests administrator privileges, and registers a Windows uninstaller. It does
not launch OBS. Inno Setup 6.4.3 is pinned and downloaded with a SHA-256 check in CI.

## Local builds

Install the .NET 8 x64 runtime (required by Artifact Signing) and PowerShell 7.

Stage via `scripts/package-windows.ps1`, set the three `AZURE_ARTIFACT_SIGNING_*`
environment variables, authenticate Azure CLI, and install the pinned module:

```powershell
Install-Module ArtifactSigning -RequiredVersion 0.1.17 -Scope CurrentUser
./scripts/release/Sign-WithArtifactSigning.ps1 release/source-inspector-dock/bin/64bit/source-inspector-dock.dll
./scripts/release/Build-WindowsInstaller.ps1 -SourceDirectory release/source-inspector-dock
```

Local builds use an installed Inno Setup 6 compiler or `-CompilerPath`. To develop
packaging without signing access, explicitly pass `-Unsigned`; the output filename
ends in `-unsigned.exe`. Release CI never uses that mode. The installation smoke
test runs only on GitHub runners to avoid touching a developer's OBS installation.

References: [Azure Artifact Signing](https://github.com/Azure/artifact-signing-action),
[Inno Setup signing](https://jrsoftware.org/ishelp/topic_setup_signtool.htm),
[signed uninstallers](https://jrsoftware.org/ishelp/topic_setup_signeduninstaller.htm).
