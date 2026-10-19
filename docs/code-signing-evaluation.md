# Windows code-signing evaluation (issue #21)

Research snapshot: 2026-10-18. Read the [issue](https://github.com/michaelmawhinney/redlight/issues/21) and its complete [SignPath comment](https://github.com/michaelmawhinney/redlight/issues/21#issuecomment-4587444291), then inspected the local build, assets, metadata, workflow, and package. This is an evaluation, not enrollment or an adopted signing policy. No account, certificate, secret, workflow, release, or remote repository was changed. No application was executed.

Recommendation: investigate SignPath Foundation first if the maintainer accepts its publisher identity and approval process. Keep Microsoft Artifact Signing as the paid alternative when individual/organization eligibility is confirmed. Traditional CA signing is a fallback for identity, geography, or procurement requirements. Signing is useful for downloaded executable authenticity and publisher continuity; it does not establish display safety or remove the pending Windows/hardware qualification.

## Options and operating costs

These are provider-documentation estimates, not quotes or authority to purchase. Recheck eligibility, taxes, currency, storage charges, and current terms before choosing.

| Option | Cost and identity | Operational fit |
| --- | --- | --- |
| SignPath Foundation | Free for accepted OSS projects; certificate publisher is SignPath Foundation | Managed private key; verifiable source/build origin and human release approval. Strong candidate for this small public project, subject to acceptance. |
| Microsoft Artifact Signing | Microsoft lists approximately US$9.99/month; validated legal identity | Managed HSM/certificate lifecycle, signing from local tools or CI. Requires paid Azure setup, identity validation, roles, and maintenance. |
| Traditional OV | Microsoft's broad estimate is US$150–300/year; obtain a real provider/storage quote | Maintainer controls the CA relationship and token/cloud-HSM operation; more renewal, access, and recovery administration. |
| Traditional EV | Microsoft's broad estimate is US$400+/year | Consider only for a separate procurement/identity requirement, rather than paying for an assumed SmartScreen bypass. |
| Self-signed / unsigned | No public certificate fee | Self-signing is unsuitable for public trust. Current releases remain unsigned while selection is pending. |

Cost estimates and the OV/EV comparison come from [Microsoft's options guide](https://learn.microsoft.com/en-us/windows/apps/package-and-deploy/code-signing-options). The [Azure price page](https://azure.microsoft.com/en-us/pricing/details/artifact-signing/) currently renders monetary values as placeholders in the retrieved page; it lists 5,000 monthly Basic signatures and 100,000 Premium signatures. The approximate price above is the guide's estimate, not a verified billing quote. [Artifact Signing's FAQ](https://learn.microsoft.com/en-us/azure/artifact-signing/faq) excludes free/trial/sponsored subscriptions, uses the validated legal name rather than a customizable project CN/O, and explains identity renewal and full-SKU billing. Maintainer location and legal identity have not been inferred from a username or this machine.

The service's [current setup prerequisites](https://learn.microsoft.com/en-us/azure/artifact-signing/quickstart) list US/Canada individuals and a broader organization-country list than the general Windows options article. Use the service-specific prerequisites when checking eligibility; do not infer eligibility from the older, narrower comparison. Microsoft describes [managed HSM certificate lifecycle and digest signing](https://learn.microsoft.com/en-us/azure/artifact-signing/overview). For traditional certificates, [DigiCert documents protected token/HSM key storage](https://knowledge.digicert.com/general-information/new-private-key-storage-requirement-for-standard-code-signing-certificates-november-2022); an exportable PFX in repository/CI secrets is not the proposed storage model.

## SignPath eligibility: promising, not confirmed

The [Foundation conditions](https://signpath.org/terms.html) require maintained, released, documented OSS with acceptable component licensing; verifiable builds, repository ownership, MFA, assigned author/reviewer/approver roles, manual signing approval, and a published signing/privacy policy. Acceptance is discretionary. Its certificate identifies the Foundation. The [Foundation overview](https://signpath.org/) says the private key remains in its HSM.

Local evidence and unresolved items:

| Area | Evidence / remaining confirmation |
| --- | --- |
| Source and project | Root `LICENSE` contains GPLv3; README describes the red-only behavior and portable release. This is useful evidence, not confirmation of every contribution's rights or Foundation acceptance. |
| Generated tray assets | Four tray resources are reproducible original pixel artwork in `tools/Generate-TrayIcons.ps1`; static reproduction checks pass. |
| Historical assets | `resources/redlight.ico` dates to `d128957` (initial beta); four `docs/images/*.webp` date to `73b5740` (demo images). No separate provenance/rights statement was found. Confirm origin, permissions, and screenshot content before claiming a complete licensing review. Do not erase attribution or replace assets merely to claim eligibility. |
| Dependencies | Production builds use native Windows imports and statically linked MSVC runtime; no third-party runtime DLL is packaged. Confirm the runtime/system-library treatment and contribution provenance with the program if applying. Tests/probes/build tools also need an accurate source inventory. |
| People and policy | Signing roles, MFA, policy publication, and Foundation approval are external decisions/evidence, unavailable from this checkout. Do not publish an accepted-provider badge or promise before acceptance. |

Optional Windows startup has explicit consent/removal instructions, and automatic scheduling is disclosed and Off by default. Include those behaviors in any future application review. The present evaluation makes no representation that all program conditions are satisfied.

## Local versus trusted CI signing

The [SignPath GitHub connector](https://docs.signpath.io/trusted-build-systems/github) verifies a workflow-produced artifact and its origin; OSS jobs leading to signing must run on GitHub-hosted agents. A token alone does not establish trusted origin. Its [project policies](https://docs.signpath.io/projects) separate submission, approval, permitted origins, and artifact configuration; OSS requires trusted-build/origin verification. Locally built binaries cannot be assumed to qualify for that path.

The local `.github/workflows/build.yml` already builds/tests on `windows-2022`, uploads executable/checksums/ZIP, and has a tag-only draft-release job. It has no signing step or provider credentials. A future integration would need an explicitly authorized provider/project/policy, restricted release origins and approvers, an exact executable allowlist, uploaded unsigned artifact identity, signed-output retrieval, and verification before packaging. PR builds must have no release-signing credentials. Current local commits do not trigger this workflow.

[Artifact Signing's integration guide](https://learn.microsoft.com/en-us/azure/artifact-signing/how-to-signing-integrations) supports local SignTool plus a signing plugin and authenticated profile access as well as CI. Signing-node runtimes/plugins are build infrastructure, not new RedLight runtime dependencies. Certificate validity is short and timestamping is critical. A token-backed CA can also support deliberate local signing; a physical token needs appropriate attached signing infrastructure for automation. These alternatives avoid Foundation origin requirements but retain identity, authorization, key-service availability, renewal, and compromise-response obligations.

## Artifact readiness and future sequence

Static inspection at `bbe0eb0` found an x64 GUI EXE with an empty PE certificate table, seven reviewed Windows imports, static MSVC runtime, `asInvoker`/`uiAccess=false`, and version `0.6.0-rc.1`. `resources/app.rc` sets ProductName RedLight, OriginalFilename RedLight.exe, and CompanyName RedLight contributors. Version-resource CompanyName is not the certificate publisher. Product/version metadata can support provider restrictions, but the existing verifier does not enforce every provider-specific field or any signature.

The portable package contains only the EXE, executable checksum, and documents/assets. This structure appears suitable for signing the EXE before ZIP assembly; that is an engineering inference, not a service-accepted artifact. CPack hashes the installed executable, and `Verify-Package.ps1` compares the archive with the selected binary/docs. Signing changes EXE bytes, so unsigned hashes/ZIPs cannot be published as the final signed artifacts.

Future implementation, only after provider/identity approval:

1. Build the reviewed clean revision; run permitted isolated tests and static metadata/import checks. Record the unsigned artifact identity.
2. Submit exactly the release EXE through the approved signing path, with the required human approval. Keep signed output separate until verified.
3. Verify Authenticode trust, expected publisher/EKU, timestamp, exact artifact identity, and preserved PE/version/manifest requirements. Reject absent/unexpected signatures and warnings; do not silently fall back to unsigned publication.
4. Generate the executable SHA-256 from the final signed bytes, package those same bytes, then verify every packaged file and generate the ZIP checksum. Never rebuild/edit resources after signing.
5. Review the final artifacts before any separately authorized release. Keep the signed bytes and source/build/signing evidence together for later verification or incident response.

[Microsoft SignTool documentation](https://learn.microsoft.com/en-us/windows/win32/seccrypto/signtool) specifies SHA-256 file/timestamp digest options, RFC 3161 timestamping, `/pa` Authenticode verification and `/tw` timestamp warnings. Exit code 2 means warnings; future automation should reject it. Successful signing without an independently checked timestamp is insufficient. No signing or signature-verification command was run here.

## Reputation and qualification limits

[Microsoft's SmartScreen guidance](https://learn.microsoft.com/en-us/windows/apps/package-and-deploy/smartscreen-reputation) distinguishes file-hash and publisher reputation: new signed files may still warn, and current EV signing does not promise an immediate bypass. Consistent signing identity helps continuity; no download threshold or warning-free result is promised. Do not advise users to disable Windows security to install RedLight. Store packaging would be a separate distribution project and does not replace evaluation of the existing portable EXE.

Before adoption, the maintainer must select publisher/identity and budget, resolve asset provenance and eligibility, approve account/repository access and signing roles, and authorize integration. Before claiming success, validate the actual signed artifact's chain, publisher, timestamp/expiry/revocation behavior, download and Windows policy interaction, and package identity. Display behavior and restoration still require the separate authorized Windows qualification in [release validation](release-validation.md). Current builds remain unsigned, with no certificate or signing service configured.
