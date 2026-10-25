# Validation and boundaries

Application/SDK **0.1.1**, Microsoft-signed driver **0.1.0.0**, Windows x64. The complete release includes actual self-contained UI/console installers; the kernel implementation stays private.

## Signed package identity

The returned INF is byte-identical to the submission. Windows catalog hashing confirms that protected SYS content matches the qualified original image, apart from Authenticode exclusions. Microsoft kernel-policy catalog trust, hardware-publisher identity and exact INF/SYS membership were verified on the development host and again inside the guest. Both installers were inspected as PE resources without executing host installer code; each embeds the exact three returned files.

- Original qualified SYS SHA-256: `E36A187DBC13FA29893B71F42B7DE6D86506239AF67C59011F3286D9C50F62A1`.
- Returned signed SYS SHA-256: `89d9c9bbf8d338cff1fa9f118544dc9268c91f7257e4fb8c177828016d1792b5`.
- No kernel recompilation, catalog regeneration, lab certificate or replacement driver image was used for the return.

## Actual signed Windows tests

The exact returned image and resource-verified console installer passed isolated **Windows 10 build 19045 x64** campaigns on 2026-10-25: normal: **918 checks**, targeted Driver Verifier: **913 checks**. Test-signing was explicitly disabled before installation in both campaigns; the guest's pre-existing Secure Boot setting was not changed.

Each campaign exercised real UDP with unchanged payloads, every-third loss, a configured 40 ms delay with actual clone/injection counters, a 512-datagram burst against the 128-packet bound, socket closure while queued, expiry restoring pass-through and abrupt owner termination. The loopback fixture and synthetic adapter do not establish physical-link behavior or performance.

Restricted-token control access, administrator zero/read-only/write-only opens and a second control owner were refused. Actual console setup installation and uninstallation were exercised. Every owned package was removed, its service unloaded and temporary network configuration cleaned up. No new minidump was observed. The guest returned to its original powered-off baseline, with all nine historical checkpoints retained. No driver, certificate, host boot setting or host network policy was installed or changed on the development PC.

Standard Driver Verifier targeted the two candidate drivers and their exact synthetic fixture. Special pool, forced IRQL, pool tracking, I/O, deadlock, DMA, security, miscellaneous and DDI checks were enabled. NDIS/WIFI-specific checking, low-resource simulation and prolonged pressure were not enabled. The UI and console setups share one installation core; UI click automation is not claimed.

## Offline checks

100,003 policy/boundary checks pass in the signed user-mode build. The unchanged C++ code also passed its separate AddressSanitizer configuration. Native builds use W4/WX, CFG, ASLR, NX and static CRT. Kernel PREfast reports zero defects; the original InfVerif/catalog and submission evidence are preserved. These source builds and protocol tests do not install a host driver or alter network policy.

## Scope

Bounded UDP IPv4 loss and delay for an explicitly selected application and endpoint. No TCP/IPv6, full WAN simulation or physical-link latency claim.

Windows 11, Secure Boot/HVCI, representative physical NICs, offloads, concurrent security/filter products, suspend/resume, recovery and prolonged workloads remain separate qualification work. Functional delay checks are not latency/throughput benchmarks. Microsoft signing is not WHQL certification, exhaustive testing, zero overhead or a guarantee of universal compatibility.

Signed per-handle permissions last at most one hour and can be explicitly renewed while a product key is enabled. Revocation refuses new grants rather than instantly cancelling accepted permissions. Private kernel source, CABs, PDBs, lab material and signing credentials are excluded from public repositories and downloads.
