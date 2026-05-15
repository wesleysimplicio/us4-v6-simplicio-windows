# MSIX Packaging

This directory holds the Windows packaging scaffold for `US4 V6 Windows Edition`.

Current status:

- packaging structure exists
- manifest template exists
- signing scaffold exists through `scripts/sign-msix.ps1`
- signed distribution is not ready yet
- the default local script targets unsigned packaging validation only

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
