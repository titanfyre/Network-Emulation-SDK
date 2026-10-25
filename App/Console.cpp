#include "../SDK/Emulator.h"
#include <iostream>
#include <ws2tcpip.h>
int main(int argc,char** argv){try{
 NetworkEmulator emulator;if(argc==2&&std::string(argv[1])=="--preview"){std::cout<<"OFFLINE PREVIEW: UDP policy editor; no driver opened.\n";return 0;}
 if(argc==2&&std::string(argv[1])=="status"){auto s=emulator.Status();std::cout<<"Active="<<s.active<<" Matched="<<s.matched<<" Dropped="<<s.dropped<<" Delayed="<<s.delayed<<" Queued="<<s.queued<<"\n";return 0;}
 std::cout<<"Network Emulation SDK: outbound UDP/IPv4 only. Start a fresh UDP socket after enabling.\nAccess key: ";std::string key;std::getline(std::cin,key);
 auto error=emulator.Authorize(key);SecureZeroMemory(key.data(),key.size());if(!error.empty())throw std::runtime_error(error);
 NE_POLICY p{NE_VERSION,sizeof(NE_POLICY)};std::string ip;std::cout<<"Target PID, remote IPv4, remote port, drop every N (0=off), delay ms, jitter ms: ";std::cin>>p.process_id>>ip>>p.remote_port>>p.drop_every>>p.delay_ms>>p.jitter_ms;IN_ADDR address{};if(InetPtonA(AF_INET,ip.c_str(),&address)!=1)throw std::runtime_error("Invalid numeric IPv4 address");p.remote_ipv4=ntohl(address.s_addr);
 error=emulator.Start(p);if(!error.empty())throw std::runtime_error(error);std::cout<<"Active. Press Enter to restore pass-through.\n";std::cin.ignore(10000,'\n');std::cin.get();
 }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}return 0;}
