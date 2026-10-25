#include "Emulator.h"
#include <winhttp.h>
#include <wincrypt.h>
#include <array>
#include <cstring>
#include <utility>
#pragma comment(lib,"winhttp.lib")
#pragma comment(lib,"crypt32.lib")

namespace {
struct Http {
    HINTERNET value = nullptr;
    explicit Http(HINTERNET handle) : value(handle) {}
    ~Http() { if (value) WinHttpCloseHandle(value); }
    Http(const Http&) = delete;
    Http& operator=(const Http&) = delete;
};
std::string Failure(const char* stage) {
    return std::string(stage) + " failed (Windows error " +
        std::to_string(GetLastError()) + ").";
}
bool ValidKey(std::string_view key) {
    if (key.size() != 81 || key.substr(0, 5) != "app1_" || key[37] != '_')
        return false;
    for (size_t i = 5; i != 37; ++i)
        if (!((key[i] >= '0' && key[i] <= '9') ||
              (key[i] >= 'a' && key[i] <= 'f'))) return false;
    for (size_t i = 38; i != key.size(); ++i)
        if (!((key[i] >= 'a' && key[i] <= 'z') ||
              (key[i] >= 'A' && key[i] <= 'Z') ||
              (key[i] >= '0' && key[i] <= '9') ||
               key[i] == '_' || key[i] == '-')) return false;
    return true;
}
}

namespace {
std::string RequestDriverAuthorization(std::string_view key,
    const TITAN_AUTHORIZATION_CHALLENGE& challenge,
    TITAN_AUTHORIZATION_TOKEN& token) {
    token = {};
    if (!ValidKey(key)) return "Enter a valid application access key (app1_...).";
    if (challenge.magic != TITAN_AUTHORIZATION_MAGIC ||
        challenge.version != TITAN_AUTHORIZATION_VERSION ||
        challenge.duration_seconds != 0 ||
        (challenge.product != TITAN_AUTHORIZATION_NETWORK_EMULATION))
        return "The driver returned an invalid authorization challenge.";
    const char* sku = "network-emulation-sdk";
    std::string nonce;
    constexpr char hex[] = "0123456789abcdef";
    for (unsigned char value : challenge.nonce) {
        nonce += hex[value >> 4]; nonce += hex[value & 15];
    }
    std::string body = "{\"key\":\"" + std::string(key) +
        "\",\"sku\":\"" + sku + "\",\"nonce\":\"" + nonce + "\"}";
    Http session(WinHttpOpen(L"Titan Driver SDK/0.1",
        WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS, 0));
    if (!session.value) { SecureZeroMemory(body.data(), body.size()); return Failure("HTTPS initialization"); }
    WinHttpSetTimeouts(session.value, 5000, 5000, 10000, 10000);
    Http connection(WinHttpConnect(session.value, L"licences.configcsa.com",
        INTERNET_DEFAULT_HTTPS_PORT, 0));
    if (!connection.value) { SecureZeroMemory(body.data(), body.size()); return Failure("HTTPS connection"); }
    Http request(WinHttpOpenRequest(connection.value, L"POST",
        L"/portfolio/api/v1/driver-authorize", nullptr, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE));
    if (!request.value) { SecureZeroMemory(body.data(), body.size()); return Failure("Authorization request"); }
    DWORD disabled = WINHTTP_DISABLE_REDIRECTS | WINHTTP_DISABLE_COOKIES;
    if (!WinHttpSetOption(request.value, WINHTTP_OPTION_DISABLE_FEATURE,
        &disabled, sizeof(disabled))) {
        SecureZeroMemory(body.data(), body.size()); return Failure("HTTPS policy");
    }
    const BOOL sent = WinHttpSendRequest(request.value,
        L"Content-Type: application/json\r\n", static_cast<DWORD>(-1),
        body.data(), static_cast<DWORD>(body.size()),
        static_cast<DWORD>(body.size()), 0);
    const DWORD sendError = sent ? ERROR_SUCCESS : GetLastError();
    SecureZeroMemory(body.data(), body.size());
    if (!sent) return "Authorization request failed (Windows error " + std::to_string(sendError) + ").";
    if (!WinHttpReceiveResponse(request.value, nullptr)) return Failure("Authorization response");
    DWORD status = 0, length = sizeof(status);
    if (!WinHttpQueryHeaders(request.value, WINHTTP_QUERY_STATUS_CODE |
        WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status,
        &length, WINHTTP_NO_HEADER_INDEX)) return Failure("HTTP status");
    std::string response;
    std::array<char, 1024> chunk{};
    for (;;) {
        DWORD count = 0;
        if (!WinHttpReadData(request.value, chunk.data(),
            static_cast<DWORD>(chunk.size()), &count)) return Failure("Response read");
        if (!count) break;
        if (response.size() + count > 8192) return "Authorization response is too large.";
        response.append(chunk.data(), count);
    }
    if (status != 200) {
        for (char& c : response) if (static_cast<unsigned char>(c) < 32) c = ' ';
        return "License server refused authorization (HTTP " +
            std::to_string(status) + "): " + response;
    }
    // Only extract a bounded base64 field. The kernel verifies every byte of
    // the decoded permission and its signature before enabling any producer.
    const auto field = response.find("\"grant_base64\"");
    if (field == std::string::npos) return "Missing authorization grant in server response.";
    const auto colon = response.find(':', field + 14);
    const auto start = colon == std::string::npos ? std::string::npos : response.find('"', colon + 1);
    const auto end = start == std::string::npos ? std::string::npos : response.find('"', start + 1);
    if (end == std::string::npos || end - start - 1 != 152)
        return "Invalid authorization grant encoding.";
    const std::string grant = response.substr(start + 1, end - start - 1);
    DWORD bytes = sizeof(token);
    if (!CryptStringToBinaryA(grant.c_str(), static_cast<DWORD>(grant.size()),
        CRYPT_STRING_BASE64 | CRYPT_STRING_STRICT,
        reinterpret_cast<BYTE*>(&token), &bytes, nullptr, nullptr) ||
        bytes != sizeof(token)) {
        token = {}; return "Invalid authorization grant length or encoding.";
    }
    if (token.permission.magic != challenge.magic ||
        token.permission.version != challenge.version ||
        token.permission.product != challenge.product ||
        token.permission.duration_seconds == 0 ||
        token.permission.duration_seconds > TITAN_AUTHORIZATION_MAX_SECONDS ||
        std::memcmp(token.permission.nonce, challenge.nonce, sizeof(challenge.nonce))) {
        token = {}; return "Authorization grant does not match this driver session.";
    }
    return {};
}

} // private HTTPS implementation

#include <fwpmu.h>
#include <stdexcept>
namespace {
const GUID Provider={0x67be6a03,0x5ae7,0x4ac5,{0x99,0xb2,0xa9,0x5e,0x31,0xc8,0x70,0x10}};
const GUID Sublayer={0x67be6a04,0x5ae7,0x4ac5,{0x99,0xb2,0xa9,0x5e,0x31,0xc8,0x70,0x10}};
const GUID Keys[2]={
 {0x67be6a01,0x5ae7,0x4ac5,{0x99,0xb2,0xa9,0x5e,0x31,0xc8,0x70,0x10}},
 {0x67be6a02,0x5ae7,0x4ac5,{0x99,0xb2,0xa9,0x5e,0x31,0xc8,0x70,0x10}}};
void Check(DWORD e,const char* stage){if(e)throw std::runtime_error(std::string(stage)+": "+NetworkEmulator::Error(e));}
}
std::string NetworkEmulator::Error(DWORD e){
 char* text=nullptr;FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,nullptr,e,MAKELANGID(LANG_ENGLISH,SUBLANG_ENGLISH_US),reinterpret_cast<char*>(&text),0,nullptr);
 std::string s=text?text:"Windows operation failed";if(text)LocalFree(text);while(!s.empty()&&(s.back()=='\r'||s.back()=='\n'))s.pop_back();return s+" (Windows error "+std::to_string(e)+")";
}
void NetworkEmulator::Open(){if(device_!=INVALID_HANDLE_VALUE)return;device_=CreateFileW(L"\\\\.\\TitanNetworkEmulation",GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);if(device_==INVALID_HANDLE_VALUE)Check(GetLastError(),"Open Network Emulation driver (install driver and run as administrator)");}
HANDLE NetworkEmulator::NativeHandle(){Open();return device_;}
DWORD NetworkEmulator::Call(DWORD code,const void* input,DWORD inputBytes,void* output,DWORD outputBytes) noexcept {
 DWORD actual=0;if(device_==INVALID_HANDLE_VALUE)return ERROR_INVALID_HANDLE;
 if(!DeviceIoControl(device_,code,const_cast<void*>(input),inputBytes,output,outputBytes,&actual,nullptr))return GetLastError();return actual==outputBytes?ERROR_SUCCESS:ERROR_INVALID_DATA;
}
std::string NetworkEmulator::Authorize(std::string_view key){try{Open();TITAN_AUTHORIZATION_CHALLENGE c{};TITAN_AUTHORIZATION_TOKEN token{};Check(Call(NE_IOCTL_CHALLENGE,nullptr,0,&c,sizeof(c)),"Driver challenge");auto error=RequestDriverAuthorization(key,c,token);if(!error.empty())return error;auto e=Call(NE_IOCTL_AUTHORIZE,&token,sizeof(token),nullptr,0);SecureZeroMemory(&token,sizeof(token));return e?"Driver permission: "+Error(e):std::string{};}catch(const std::exception& e){return e.what();}}
void NetworkEmulator::Filters(const NE_POLICY& p){
 FWPM_SESSION0 session{};session.flags=FWPM_SESSION_FLAG_DYNAMIC;session.txnWaitTimeoutInMSec=1000;Check(FwpmEngineOpen0(nullptr,RPC_C_AUTHN_WINNT,nullptr,&session,&engine_),"Open dynamic WFP session");
 Check(FwpmTransactionBegin0(engine_,0),"Begin WFP transaction");
 FWPM_PROVIDER0 provider{};provider.providerKey=Provider;provider.displayData.name=const_cast<wchar_t*>(L"Titan Network Emulation");Check(FwpmProviderAdd0(engine_,&provider,nullptr),"Add own provider");
 FWPM_SUBLAYER0 sub{};sub.subLayerKey=Sublayer;sub.providerKey=const_cast<GUID*>(&Provider);sub.displayData.name=const_cast<wchar_t*>(L"Explicit UDP emulation");sub.weight=0x100;Check(FwpmSubLayerAdd0(engine_,&sub,nullptr),"Add own sublayer");
 const GUID* layers[2]={&FWPM_LAYER_ALE_FLOW_ESTABLISHED_V4,&FWPM_LAYER_DATAGRAM_DATA_V4};
 for(unsigned i=0;i<2;++i){
  FWPM_CALLOUT0 callout{};callout.calloutKey=Keys[i];callout.providerKey=const_cast<GUID*>(&Provider);callout.applicableLayer=*layers[i];callout.displayData.name=const_cast<wchar_t*>(L"Titan bounded UDP emulation");Check(FwpmCalloutAdd0(engine_,&callout,nullptr,nullptr),"Add callout");
  FWPM_FILTER_CONDITION0 conditions[4]{};
  conditions[0].fieldKey=FWPM_CONDITION_IP_PROTOCOL;conditions[0].matchType=FWP_MATCH_EQUAL;conditions[0].conditionValue.type=FWP_UINT8;conditions[0].conditionValue.uint8=17;
  conditions[1].fieldKey=FWPM_CONDITION_IP_REMOTE_ADDRESS;conditions[1].matchType=FWP_MATCH_EQUAL;conditions[1].conditionValue.type=FWP_UINT32;conditions[1].conditionValue.uint32=p.remote_ipv4;
  conditions[2].fieldKey=FWPM_CONDITION_IP_REMOTE_PORT;conditions[2].matchType=FWP_MATCH_EQUAL;conditions[2].conditionValue.type=FWP_UINT16;conditions[2].conditionValue.uint16=static_cast<UINT16>(p.remote_port);
  conditions[3].fieldKey=FWPM_CONDITION_DIRECTION;conditions[3].matchType=FWP_MATCH_EQUAL;conditions[3].conditionValue.type=FWP_UINT32;conditions[3].conditionValue.uint32=FWP_DIRECTION_OUTBOUND;
  FWPM_FILTER0 f{};f.providerKey=const_cast<GUID*>(&Provider);f.displayData.name=const_cast<wchar_t*>(L"Explicit selected UDP endpoint");f.layerKey=*layers[i];f.subLayerKey=Sublayer;f.weight.type=FWP_EMPTY;f.action.type=i?FWP_ACTION_CALLOUT_UNKNOWN:FWP_ACTION_CALLOUT_INSPECTION;if(i)f.flags|=FWPM_FILTER_FLAG_PERMIT_IF_CALLOUT_UNREGISTERED;f.action.calloutKey=Keys[i];f.numFilterConditions=i?4:3;f.filterCondition=conditions;Check(FwpmFilterAdd0(engine_,&f,nullptr,nullptr),"Add selected endpoint filter");
 }
 Check(FwpmTransactionCommit0(engine_),"Commit WFP transaction");
}
std::string NetworkEmulator::Start(const NE_POLICY& p){try{
 if(!NeValidPolicy(&p))return "Invalid policy: choose a PID, numeric IPv4 endpoint/port, drop interval <=1000000 and delay 0..1000 ms with jitter <= delay.";
 Open();if(engine_&&(policy_.process_id!=p.process_id||policy_.remote_ipv4!=p.remote_ipv4||policy_.remote_port!=p.remote_port))Stop();
 if(!engine_)Filters(p);Check(Call(NE_IOCTL_START,&p,sizeof(p),nullptr,0),"Start emulation (authorization required)");policy_=p;return {};
 }catch(const std::exception& e){Stop();return e.what();}}
void NetworkEmulator::Stop() noexcept {if(device_!=INVALID_HANDLE_VALUE)(void)Call(NE_IOCTL_STOP,nullptr,0,nullptr,0);if(engine_){FwpmEngineClose0(engine_);engine_=nullptr;}}
NetworkEmulator::~NetworkEmulator(){Stop();if(device_!=INVALID_HANDLE_VALUE)CloseHandle(device_);}
NE_STATUS NetworkEmulator::Status(){Open();NE_STATUS s{};Check(Call(NE_IOCTL_STATUS,nullptr,0,&s,sizeof(s)),"Read status");if(s.version!=NE_VERSION||s.size!=sizeof(s)||s.active>1||s.queued>NE_QUEUE_CAPACITY)throw std::runtime_error("Invalid driver status response");return s;}
