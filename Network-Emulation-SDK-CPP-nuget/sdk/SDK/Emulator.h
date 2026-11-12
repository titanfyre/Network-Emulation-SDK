#pragma once
#include <winsock2.h>
#include <windows.h>
#include "EmulationProtocol.h"
#include "AuthorizationProtocol.h"
#include <string>
#include <string_view>
class NetworkEmulator {
 HANDLE device_=INVALID_HANDLE_VALUE;
 HANDLE engine_=nullptr;
 NE_POLICY policy_{};
 DWORD Call(DWORD,const void*,DWORD,void*,DWORD) noexcept;
 void Open();
 void Filters(const NE_POLICY&);
public:
 NetworkEmulator()=default;
 ~NetworkEmulator();
 NetworkEmulator(const NetworkEmulator&)=delete;
 NetworkEmulator& operator=(const NetworkEmulator&)=delete;
 std::string Authorize(std::string_view key);
 std::string Start(const NE_POLICY& policy);
 void Stop() noexcept;
 NE_STATUS Status();
 HANDLE NativeHandle(); // Advanced protocol diagnostics; retain this object's lifetime.
 static std::string Error(DWORD);
};
