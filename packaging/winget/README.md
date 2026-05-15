# winget packaging scaffold

This folder hosts the repository-owned winget packaging scaffold for `US4 V6 Windows Edition`.

Current status:

- manifest templates live under `templates/`
- local rendering is handled by `scripts/render-winget-manifests.ps1`
- publish URLs are still placeholders until signed release assets are available

Typical local render:

```powershell
.\scripts\render-winget-manifests.ps1 `
  -Version 0.1.20 `
  -PortableUrl https://example.invalid/us4-v6-windows-0.1.20-portable.zip `
  -MsixUrl https://example.invalid/us4-v6-windows-0.1.20.msix
```

This scaffold does not publish to winget automatically yet.
