# Network Emulation SDK - Microsoft-signed Windows Kernel UDP Network Emulator

**Reproduce outbound UDP loss and delay for one selected Windows process and endpoint.**

<!-- full-version-contact:start -->
> [!IMPORTANT]
> **Get the full version without demo restrictions**
>
> **Discord username: `titancsa`**
>
> Open Discord, choose **Friends > Add Friend**, enter **`titancsa`**, then send a direct message. Ask for the **full version without the demo time limit**, and mention **Network Emulation SDK**.
>
> Tell me whether you need personal use, business integration or OEM redistribution, and ask for pricing and license / activation instructions.
<!-- full-version-contact:end -->

A KMDF/WFP callout operates in the Windows datagram path. The native C++ app and compact SDK configure deterministic packet loss, a bounded delay and cyclic jitter without a proxy or changes to the application's payload.

![Policy and packet path](docs/overview.svg)

**Microsoft-signed evaluation for Windows x64.** The complete 0.1.1 release contains the app, CLI, SDK, example, installer sources and self-contained UI/console installers with the signed 0.1.0.0 driver. Actual signed installation, UDP tests and full removal passed on Windows 10 x64 normally and under targeted Driver Verifier, with test-signing disabled. The labelled offline preview remains available without opening a driver.

## Scope

| Setting | Meaning |
| --- | --- |
| Process ID + remote IPv4/UDP port | One explicitly selected process and destination; no global traffic policy. |
| Drop every N | Drop each Nth matching classifier indication after Start/policy replacement; zero disables loss. Supported traffic has one UDP datagram per indication. |
| Delay | 0..1000 ms before outbound reinjection; actual Windows scheduling adds variability. |
| Jitter | Deterministic cyclic variation within +/- the selected amount, no larger than the base delay; the effective target can reach 2000 ms. |
| Retained packets | At most 128 retained packets, including allocation reservations and in-flight injections; overflow passes through and increments a counter. |
| Flow tracking | At most 64 associated UDP flows. Unsupported/overflow traffic passes through. |

Only ordinary outbound UDP/IPv4 with one network buffer is supported. TCP, IPv6, inbound traffic, raw sockets, multiple-buffer batches and unsupported ancillary metadata pass unchanged. The destination validator excludes multicast and reserved high address ranges; directed broadcast is not qualified. No content inspection, replay, corruption, bandwidth limiter, network tunnelling or arbitrary packet generation is provided.

**Start a fresh UDP socket after first enabling a policy or selecting a different process/endpoint.** Existing flows cannot be associated retroactively. Editing loss/delay for the currently associated scope applies to subsequent packets of that flow. A process reference prevents PID reuse from silently selecting a different process; target exit disables the policy.

Closing the owner or allowing its permission to expire restores pass-through. Already queued packets are released immediately; a socket that closes before reinjection can cause a reported injection error. FIFO deadlines prevent deliberate reordering inside the delay queue, but overflow pass-through can overtake delayed packets. No timing or delivery guarantee is made under CPU pressure.

## Run

1. Download and extract the complete [release ZIP](https://github.com/titanfyre/Network-Emulation-SDK/releases/tag/v0.1.1).
2. Open `NetworkEmulation.exe --preview` for the offline editor, or request a Network Emulation SDK access key from **`titancsa` on Discord** for live operation. The signed bundle does not require test-signing.
3. With a signed bundle, run `NetworkEmulationSetup.exe`, choose **Install / reinstall**, and restart only if requested. Console deployment uses `/install` and `/uninstall`; exit code `3010` requests a manual restart. Setup verifies exact product identity and Microsoft-root catalog plus INF/SYS membership; it never restarts automatically.
4. Open the application as administrator, enter your product access key, select an authorized target PID and numeric IPv4/UDP destination, then choose **Authorize & start**. Create the application's new UDP socket afterwards.
5. **Stop** or close the app to release the policy. Test only software and traffic you own or are authorized to administer.

`NetworkEmulationCLI.exe --preview` is offline. `status` reads the real driver state; interactive use asks for the access key and explicit policy. Only one administrator owns the control endpoint at a time. The UI refreshes counters at 200 ms; it does not set packet scheduling cadence. Authorization runs on a worker thread.

## Minimal C++ integration

Keep the entire `SDK/` directory and compile `Emulator.cpp`; link `fwpuclnt.lib`, `ws2_32.lib`, `winhttp.lib`, `crypt32.lib` and `rpcrt4.lib`.

```cpp
#include "SDK/Emulator.h"

NetworkEmulator emulator;
auto error = emulator.Authorize(accessKey); // English message, empty on success.
if (!error.empty()) return;

NE_POLICY policy{NE_VERSION, sizeof(NE_POLICY)};
policy.process_id = GetCurrentProcessId();
policy.remote_ipv4 = 0x7f000001; // Host-order 127.0.0.1.
policy.remote_port = 45000;
policy.drop_every = 3;
policy.delay_ms = 40;
error = emulator.Start(policy);
// Open a NEW UDP socket in this process after Start() succeeds.
// Scope exit restores pass-through and removes this session's WFP filters.
```

`Start(policy)` returns an English error string. `Authorize(key)` obtains a signed permission for the current handle. `Status()` returns real counters or throws an English error. `Stop()` is idempotent. Serialize calls on a single SDK object; stop all callers before destruction. `NativeHandle()` exists for advanced protocol diagnostics, not ordinary integration.

`matched`, `dropped`, `delayed` and `queue_overflow` count this classifier's supported indications cumulatively for the driver lifetime. The loss/jitter sequence resets whenever a policy is started/replaced. `injected` counts reinjection calls accepted by WFP, not confirmed network delivery. `failed` counts immediate reinjection refusals. A full queue increments `bypassed` and `queue_overflow`; it does not wait in a classification callback. No data is exported unless you explicitly add export code to your application.

## Access and commercial integration

Add **`titancsa` on Discord** for evaluation access, an OEM integration pilot, redistribution rights, branded deployment and Windows maintenance. Specify your UDP workload, target Windows builds and acceptance criteria. Free alternatives such as [clumsy](https://jagt.github.io/clumsy/) exist; this component offers a bounded native integration API and scoped qualification rather than claiming a unique emulator or guaranteed revenue.

Product-specific keys request ECDSA P-256 per-handle permissions from the shared administration. Permissions last at most one hour and can be explicitly renewed while the key is enabled. Revocation prevents future grants; it does not instantly cancel accepted permissions. There is no TPM, machine fingerprint, automatic payment collection or per-machine seat enforcement. No cryptography or HTTPS runs for each datagram.

Public app/SDK/example/installer source is MIT licensed. The prebuilt driver binary has separate evaluation and OEM terms. Microsoft signing alone is not WHQL certification or evidence of universal reliability. See [validation](docs/VALIDATION.md).

## Build

Demo/evaluation driver source is available; see [driver/README.md](driver/README.md) for
build instructions and separate evaluation-source terms. Those terms apply to
Titan-authored driver code. Third-party source licenses and existing application/SDK
licenses remain applicable. Full/commercial implementation and private signing or
activation material are excluded.

Use Visual Studio 2022 Desktop C++, CMake and Windows SDK on x64:

```powershell
.\Build.ps1
.\Build.ps1 -Asan
# Embed an independently supplied opaque signed package when rebuilding setup.
.\Build.ps1 -DriverPackageDirectory C:\Signed\TitanNetworkEmulation
```

Outputs are under `obj/Build/bin/Release`. Public builds do not need a WDK or driver source. They never install a driver or change a network policy. Unsigned/test-signed packages are refused by setup.

Use only for lawful diagnostics and integration on authorized systems. You are responsible for your deployment and use; the software is supplied without warranty, subject to applicable licenses and mandatory statutory rights. No unlawful or unauthorized use is endorsed.

<!-- central-sdk-packages:start -->
## Paquets et installation

Les sources, licences, guides API et exemples sont regroupes dans [le depot SDK dedie](https://github.com/TitanCSA/sdk-packages/tree/main/projects/Network-Emulation-SDK). Choisis le paquet a installer puis execute sa commande dans son dossier.

| Registre | Paquet | Installation / utilisation |
| --- | --- | --- |
| vcpkg | `network-emulation-sdk` | `vcpkg install network-emulation-sdk:x64-windows` |
| Conan | `network-emulation-sdk` | `conan install --requires=network-emulation-sdk/0.1.1 -r titancsa --build=missing -s compiler.cppstd=17` |
| NuGet | `Network-Emulation-SDK.Native` | `Install-Package Network-Emulation-SDK.Native -Version 0.1.1` |
| NuGet | `NetworkEmulationSDK.SDK` | `dotnet add package NetworkEmulationSDK.SDK --version 1.0.0` |
| PyPI | `network_emulation_sdk` | Commande prevue apres publication : `python -m pip install network-emulation-sdk==1.0.0` |
| Conda | `network-emulation-sdk` | `conda install --override-channels -c https://raw.githubusercontent.com/TitanCSA/sdk-packages/089bc73095aa8eda89bacbe10d0b9e58394b0207/conda-channel -c conda-forge network-emulation-sdk` (canal TitanCSA) |
| crates.io | `network_emulation_sdk` | `cargo add network-emulation-sdk@1.0.0` |
| npm | `network_emulation_sdk` | Commande prevue apres publication : `npm install network-emulation-sdk@1.0.0` |
| Maven Central | `io.github.titancsa:network-emulation-sdk` | `io.github.titancsa:network-emulation-sdk:1.0.0 (pom.xml / Gradle)` |
| JitPack | `com.github.titanfyre:Network-Emulation-SDK` | `com.github.titanfyre:Network-Emulation-SDK:java-sdk-v1.0.0 (avec le depot JitPack)` |
| Go Modules | `github.com/titanfyre/Network-Emulation-SDK/Network-Emulation-SDK-Go-modules` | `go get github.com/titanfyre/Network-Emulation-SDK/Network-Emulation-SDK-Go-modules@v1.0.0` |
| pkg.go.dev | `github.com/titanfyre/Network-Emulation-SDK/Network-Emulation-SDK-Go-modules` | `Documentation : https://pkg.go.dev/github.com/titanfyre/Network-Emulation-SDK/Network-Emulation-SDK-Go-modules@v1.0.0` |
| PowerShell Gallery | `network_emulation_sdk` | `Install-Module NetworkEmulationSDK -RequiredVersion 1.0.0 -Scope CurrentUser` |
| pub.dev | `network_emulation_sdk` | Commande prevue apres publication : `dart pub add network_emulation_sdk:1.0.0` |
| RubyGems | `network_emulation_sdk` | `gem install network-emulation-sdk -v 1.0.0` |
| Packagist | `network_emulation_sdk` | Commande prevue apres publication : `composer require titancsa/network-emulation-sdk:1.0.0` |

Pour vcpkg et Conan, configure d abord le registre TitanCSA avec [le guide complet](https://github.com/TitanCSA/sdk-packages/tree/main/projects/Network-Emulation-SDK/README.md). Le guide explique aussi Maven/Gradle, les prerequis et le chargement des DLL. Les pilotes/applications du produit sont installes separement. Les commandes marquees prevues ne sont pas encore disponibles.

Les anciens imports Go et versions JitPack gardent leurs tags. Pour les anciennes archives SDK, utilise [les liens de remplacement](https://github.com/TitanCSA/sdk-packages/blob/main/release-assets/URL-MIGRATION.json).
<!-- central-sdk-packages:end -->
