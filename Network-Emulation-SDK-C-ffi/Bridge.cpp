#include "SDK/Emulator.h"
#include "WireTypes.h"
#include <map>
struct ProductSession{
NetworkEmulator client;
Json Invoke(const std::string& op,const Json& a){

if(op=="check"){auto policy=FromWire<NE_POLICY>(a.at("policy"));return {{"valid",bool(NeValidPolicy(&policy))},{"value",EncodeWire(policy)}};}
if(op=="authorize"){CheckMessage(client.Authorize(a.at("key").get<std::string>()));return Json::object();}
if(op=="start"){auto policy=FromWire<NE_POLICY>(a.at("policy"));Require(NeValidPolicy(&policy),"Invalid emulation policy");CheckMessage(client.Start(policy));return Json::object();}
if(op=="stop"){client.Stop();return Json::object();}
if(op=="status")return EncodeWire(client.Status());

throw SdkError(50,"Unknown operation");}
};
static Runtime<ProductSession> runtime;
extern "C" __declspec(dllexport) unsigned network_emulation_sdk_json_open(std::uint64_t* id)noexcept{return runtime.Open(id);}
extern "C" __declspec(dllexport) unsigned network_emulation_sdk_json_close(std::uint64_t id)noexcept{return runtime.Close(id);}
extern "C" __declspec(dllexport) char* network_emulation_sdk_json_call(std::uint64_t id,const char* request,std::uint32_t length)noexcept{return runtime.Call(id,request,length);}
extern "C" __declspec(dllexport) void network_emulation_sdk_json_free(char* response)noexcept{delete[] response;}
