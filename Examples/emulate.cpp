#include "../SDK/Emulator.h"
#include <iostream>
int main(){
 NetworkEmulator emulator;
 auto error=emulator.Authorize("app1_REPLACE_WITH_YOUR_PRODUCT_ACCESS_KEY");
 if(!error.empty()){std::cerr<<error;return 1;}
 NE_POLICY policy{NE_VERSION,sizeof(NE_POLICY)};
 policy.process_id=GetCurrentProcessId();policy.remote_ipv4=0x7f000001;policy.remote_port=45000;
 policy.drop_every=3;policy.delay_ms=40;
 error=emulator.Start(policy);if(!error.empty()){std::cerr<<error;return 1;}
 std::cout<<"Open a NEW UDP socket in this process to 127.0.0.1:45000. Every third datagram is dropped; others are delayed. Enter stops.\n";
 std::cin.get();
}
