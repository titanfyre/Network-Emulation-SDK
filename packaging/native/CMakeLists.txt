cmake_minimum_required(VERSION 3.24)
project(NetworkEmulationSDK VERSION 0.1.1 LANGUAGES C CXX)
if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "This SDK requires Windows x64. No driver is installed by this build.")
endif()
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
add_library(TitanNetworkEmulationSDK STATIC
  "sdk/SDK/Emulator.cpp"
)
add_library(NetworkEmulationSDK::sdk ALIAS TitanNetworkEmulationSDK)
set_target_properties(TitanNetworkEmulationSDK PROPERTIES EXPORT_NAME sdk)
target_compile_features(TitanNetworkEmulationSDK PUBLIC cxx_std_17)
target_compile_definitions(TitanNetworkEmulationSDK PUBLIC )
target_compile_definitions(TitanNetworkEmulationSDK PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0A00)
if(MSVC)
  target_compile_options(TitanNetworkEmulationSDK PRIVATE /utf-8 /EHsc /W4)
endif()
target_include_directories(TitanNetworkEmulationSDK PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/sdk>"
  "$<INSTALL_INTERFACE:include/>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/sdk/SDK>"
  "$<INSTALL_INTERFACE:include/SDK>"
)
target_link_libraries(TitanNetworkEmulationSDK PUBLIC advapi32 comctl32 crypt32 fwpuclnt gdi32 newdev ole32 rpcrt4 setupapi shell32 user32 winhttp wintrust ws2_32)
install(TARGETS TitanNetworkEmulationSDK EXPORT NetworkEmulationSDKTargets ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY sdk/ DESTINATION include FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp" PATTERN "*.inl")
install(FILES LICENSE DESTINATION share/network-emulation-sdk)
install(EXPORT NetworkEmulationSDKTargets NAMESPACE NetworkEmulationSDK:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/NetworkEmulationSDK)
configure_file("NetworkEmulationSDKConfig.cmake.in" "${CMAKE_CURRENT_BINARY_DIR}/NetworkEmulationSDKConfig.cmake" @ONLY)
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/NetworkEmulationSDKConfigVersion.cmake" VERSION 0.1.1 COMPATIBILITY SameMajorVersion)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/NetworkEmulationSDKConfig.cmake" "${CMAKE_CURRENT_BINARY_DIR}/NetworkEmulationSDKConfigVersion.cmake" DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/NetworkEmulationSDK)
