# MSIX Packaging

This directory holds the Windows packaging scaffold for `US4 V6 Windows Edition`.

Current status:

- packaging structure exists
- manifest template exists
- signing scaffold exists through `scripts/sign-msix.ps1`
- local dev certificate scaffold exists through `scripts/create-dev-signing-cert.ps1`
- local dev-only sign + install smoke exists through `scripts/dev-msix-smoke.ps1`
- install-host smoke scaffold exists through `scripts/install-msix-smoke.ps1`
- signed distribution is not ready yet
- the default local script targets unsigned packaging validation only, while `dev-msix-smoke.ps1` is limited to self-signed local validation

Planned contents:

- `AppxManifest.xml.in`
- release assets and logos
- packaging layout rules for `us4-cli.exe`
- signing integration for CI/release once certificates are available

Expected signing configuration:

- `US4_SIGN_CERT_PASSWORD`
- one of:
  - `US4_SIGN_CERT_PATH`
  - `US4_SIGN_CERT_BASE64`

Dev-only local flow:

```powershell
.\scripts\create-dev-signing-cert.ps1 -CertificatePassword us4-dev-pass -Format json
.\scripts\dev-msix-smoke.ps1 -BuildDir build -CertificatePassword us4-dev-pass -Format json
```

This flow is not a substitute for the real release certificate required by Sprint 12.
