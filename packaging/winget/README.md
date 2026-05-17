# winget packaging scaffold

This folder hosts the repository-owned winget packaging scaffold for `US4 V6 Windows Edition`.

Current status:

- manifest templates live under `templates/`
- local rendering is handled by `scripts/render-winget-manifests.ps1`
- local structural validation is handled by `scripts/validate-winget-manifests.ps1`
- local examples may still use placeholders, but `release.yml` now renders publishable GitHub release URLs before validation

Typical local render:

```powershell
.\scripts\render-winget-manifests.ps1 `
  -Version 0.1.30 `
  -PortableUrl https://example.invalid/us4-v6-windows-0.1.30-portable.zip `
  -MsixUrl https://example.invalid/us4-v6-windows-0.1.30.0.msix

.\scripts\validate-winget-manifests.ps1 `
  -ManifestDir packaging\winget\manifests `
  -ExpectedVersion 0.1.30
```

This scaffold does not publish to winget automatically yet.
