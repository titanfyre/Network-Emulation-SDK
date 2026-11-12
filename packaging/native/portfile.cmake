vcpkg_download_distfile(ARCHIVE
    URLS "https://github.com/titanfyre/Network-Emulation-SDK/releases/download/sdk-v0.1.1/network-emulation-sdk-sdk-0.1.1.tar.gz"
    FILENAME "network-emulation-sdk-sdk-0.1.1.tar.gz"
    SHA512 0b9e31a163fcebfea07671783a99525af6a015ed8dc13b3fb93e53a444259d3ec6c37d631ea4868560b3cb25be56c1fd5e816dd2c4e43ec8846d2cae26761f56
)
vcpkg_extract_source_archive(SOURCE_PATH ARCHIVE "${ARCHIVE}" NO_REMOVE_ONE_LEVEL)
vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}")
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME NetworkEmulationSDK CONFIG_PATH lib/cmake/NetworkEmulationSDK)
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include" "${CURRENT_PACKAGES_DIR}/debug/share")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
