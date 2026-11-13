# SDK publication status

Current source layout and registry coverage. A GitHub download is distinct from publication on a central registry.

| Target | Status | Package / documentation |
| --- | --- | --- |
| CPP/vcpkg | published_custom_registry | network-emulation-sdk |
| CPP/conan | published_custom_source_index | [network-emulation-sdk](https://github.com/TitanCSA/sdk-packages/tree/089bc73095aa8eda89bacbe10d0b9e58394b0207/conan-index) [Source](packaging/native/) |
| CPP/nuget | published_payload_verified | [Network-Emulation-SDK.Native](https://www.nuget.org/packages/Network-Emulation-SDK.Native/0.1.1) |
| NET/nuget | published_payload_verified | [NetworkEmulationSDK.SDK](https://www.nuget.org/packages/NetworkEmulationSDK.SDK/1.0.0) |
| Python/pypi | built_fixture_and_production_verified | network-emulation-sdk [Source](Network-Emulation-SDK-Python-pypi/) |
| Python/conda-forge | custom_channel_install_verified_requires_conda_forge_review | [network-emulation-sdk](https://raw.githubusercontent.com/TitanCSA/sdk-packages/089bc73095aa8eda89bacbe10d0b9e58394b0207/conda-channel) [Source](packaging/conda/) |
| Rust/cargo | published_install_verified | [network-emulation-sdk](https://crates.io/crates/network-emulation-sdk) [Source](Network-Emulation-SDK-Rust-cargo/) |
| JavaScript/npm | stopped_before_submission | network-emulation-sdk [Source](Network-Emulation-SDK-JavaScript-npm/) |
| Java/maven | published_install_verified | [network-emulation-sdk](https://central.sonatype.com/artifact/io.github.titancsa/network-emulation-sdk/1.0.0) [Source](Network-Emulation-SDK-Java-maven/) |
| Java/jitpack | published_install_verified | [network-emulation-sdk](https://jitpack.io/#titanfyre/Network-Emulation-SDK/java-sdk-v1.0.0) [Source](Network-Emulation-SDK-Java-maven/) |
| Go/modules | published_install_verified | [network-emulation-sdk](https://pkg.go.dev/github.com/titanfyre/Network-Emulation-SDK/Network-Emulation-SDK-Go-modules@v1.0.0) [Source](Network-Emulation-SDK-Go-modules/) |
| Go/pkg-go-dev | indexed_verified | [Documentation](https://pkg.go.dev/github.com/titanfyre/Network-Emulation-SDK/Network-Emulation-SDK-Go-modules@v1.0.0) |
| PowerShell/gallery | published_install_verified | [NetworkEmulationSDK](https://www.powershellgallery.com/packages/NetworkEmulationSDK/1.0.0) [Source](Network-Emulation-SDK-PowerShell-gallery/) |
| Dart/pub-dev | built_fixture_and_production_verified | network_emulation_sdk [Source](Network-Emulation-SDK-Dart-pub-dev/) |
| Ruby/rubygems | published_install_verified | [network-emulation-sdk](https://rubygems.org/gems/network-emulation-sdk) [Source](Network-Emulation-SDK-Ruby-rubygems/) |
| PHP/packagist | composer_source_and_tag_published | [titancsa/network-emulation-sdk](https://packagist.org/packages/titancsa/network-emulation-sdk) |

Native SDK source and CMake/Conan/vcpkg/NuGet recipes share [`packaging/native`](packaging/native/LAYOUT.md). Previous release archives and tags remain unchanged.

Application and driver prerequisites and original licensing remain those documented by each SDK. No driver installer is bundled with these source packages.
