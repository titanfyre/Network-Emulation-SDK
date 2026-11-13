# Network-Emulation-SDK PowerShell SDK

PowerShell7.4+ Windows x64. `Install-Module NetworkEmulationSDK -Scope CurrentUser`.

```powershell
$client=Connect-NetworkEmulationSDKSDK
try { Invoke-NetworkEmulationSDKSDK -Client $client -Operation status } finally { Disconnect-NetworkEmulationSDKSDK -Client $client }
```

Use -Arguments with a hashtable matching [schema](docs/API.md). Native errors retain their Windows code through the inner exception. Matching driver installed separately.
