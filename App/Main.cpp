#include "../SDK/Emulator.h"
#include <ws2tcpip.h>
#include <shellapi.h>
#include <array>
#include <thread>
#include <mutex>
#include <atomic>
#include <memory>
namespace {
HWND window{},key{},start{},stop{},message{};std::array<HWND,6> inputs{};HFONT font{},titleFont{};
NetworkEmulator emulator;std::mutex lock;std::thread worker;std::atomic_bool busy=false;bool preview=false,opened=false;
NE_STATUS latest{},pending{};std::wstring detail=L"Choose one process and remote endpoint. Open a fresh UDP socket after enabling.";
constexpr COLORREF Black=RGB(16,22,34),Blue=RGB(36,101,235),White=RGB(255,255,255),Gray=RGB(230,235,244);
std::wstring Text(HWND h){int n=GetWindowTextLengthW(h);std::wstring s(static_cast<size_t>(n)+1,L'\0');GetWindowTextW(h,s.data(),n+1);s.resize(n);return s;}
unsigned Number(HWND h){auto s=Text(h);if(s.empty()||s.size()>10)throw std::runtime_error("Enter a bounded nonnegative integer");unsigned long long value=0;for(wchar_t c:s){if(c<L'0'||c>L'9')throw std::runtime_error("Enter numeric policy values");value=value*10+static_cast<unsigned>(c-L'0');if(value>0xffffffffu)throw std::runtime_error("Numeric policy value is too large");}return static_cast<unsigned>(value);}
void Run(bool enabling){
 if(busy.exchange(true))return;
 if(worker.joinable())worker.join();
 NE_POLICY p{NE_VERSION,sizeof(NE_POLICY)};std::string access;
 try{if(enabling){p.process_id=Number(inputs[0]);IN_ADDR addr{};auto ip=Text(inputs[1]);if(InetPtonW(AF_INET,ip.c_str(),&addr)!=1)throw std::runtime_error("Enter a numeric IPv4 address");p.remote_ipv4=ntohl(addr.s_addr);p.remote_port=Number(inputs[2]);p.drop_every=Number(inputs[3]);p.delay_ms=Number(inputs[4]);p.jitter_ms=Number(inputs[5]);if(!NeValidPolicy(&p))throw std::runtime_error("Invalid policy. Delay <=1000 ms, jitter <= delay, UDP port 1..65535.");auto k=Text(key);for(wchar_t c:k){if(c>127)throw std::runtime_error("Access keys contain ASCII characters only");access.push_back(static_cast<char>(c));}SecureZeroMemory(k.data(),k.size()*sizeof(wchar_t));}}
 catch(const std::exception& e){busy=false;std::string s=e.what();detail.assign(s.begin(),s.end());SetWindowTextW(message,detail.c_str());return;}
 EnableWindow(start,FALSE);EnableWindow(stop,FALSE);SetWindowTextW(message,L"Working... The interface remains responsive.");
 try{worker=std::thread([p,access=std::move(access),enabling]()mutable{
  std::string result;try{std::lock_guard<std::mutex> guard(lock);if(preview){pending={};pending.active=enabling?1u:0u;detail=L"OFFLINE PREVIEW - synthetic editor state; no driver is opened.";}
   else{if(enabling){result=emulator.Authorize(access);if(result.empty())result=emulator.Start(p);opened=true;}else emulator.Stop();if(opened)pending=emulator.Status();detail=result.empty()?(enabling?L"Active. New UDP flows to this endpoint follow the selected policy.":L"Stopped. New traffic passes through; queued packets are released."):std::wstring(result.begin(),result.end());}}
   catch(const std::exception& e){std::lock_guard<std::mutex> guard(lock);std::string s=e.what();detail.assign(s.begin(),s.end());}
   SecureZeroMemory(access.data(),access.size());PostMessageW(window,WM_APP+1,0,0);
  });}catch(...){busy=false;EnableWindow(start,TRUE);EnableWindow(stop,TRUE);SetWindowTextW(message,L"Not enough resources to start the worker.");}
}
void Draw(HDC dc,int x,int y,const std::wstring& text,HFONT selected,COLORREF color){SelectObject(dc,selected);SetTextColor(dc,color);SetBkMode(dc,TRANSPARENT);TextOutW(dc,x,y,text.c_str(),static_cast<int>(text.size()));}
LRESULT CALLBACK Proc(HWND h,UINT m,WPARAM w,LPARAM l){switch(m){
 case WM_CREATE:{font=CreateFontW(18,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");titleFont=CreateFontW(30,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
  const wchar_t* names[]={L"Target process ID",L"Remote IPv4",L"Remote UDP port",L"Drop every N (0 = off)",L"Delay (0..1000 ms)",L"Jitter (+/- ms)"};
  const std::wstring values[]={std::to_wstring(GetCurrentProcessId()),L"127.0.0.1",L"45000",L"0",L"40",L"0"};
  for(unsigned i=0;i<6;++i){int x=28+static_cast<int>(i%3)*230,y=150+static_cast<int>(i/3)*90;auto label=CreateWindowW(L"STATIC",names[i],WS_CHILD|WS_VISIBLE,x,y,215,22,h,nullptr,nullptr,nullptr);SendMessageW(label,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);inputs[i]=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",values[i].c_str(),WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_AUTOHSCROLL,x,y+27,212,34,h,nullptr,nullptr,nullptr);SendMessageW(inputs[i],EM_SETLIMITTEXT,i==1?15:10,0);SendMessageW(inputs[i],WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);}
  auto label=CreateWindowW(L"STATIC",L"Product access key",WS_CHILD|WS_VISIBLE,28,338,240,22,h,nullptr,nullptr,nullptr);SendMessageW(label,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);
  key=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_PASSWORD|ES_AUTOHSCROLL,28,365,442,34,h,nullptr,nullptr,nullptr);SendMessageW(key,EM_SETLIMITTEXT,81,0);
  start=CreateWindowW(L"BUTTON",L"Authorize & start",WS_CHILD|WS_VISIBLE|WS_TABSTOP,488,363,210,38,h,reinterpret_cast<HMENU>(1),nullptr,nullptr);stop=CreateWindowW(L"BUTTON",L"Stop",WS_CHILD|WS_VISIBLE|WS_TABSTOP,558,96,140,36,h,reinterpret_cast<HMENU>(2),nullptr,nullptr);
  message=CreateWindowW(L"STATIC",detail.c_str(),WS_CHILD|WS_VISIBLE,28,420,670,62,h,nullptr,nullptr,nullptr);for(auto item:{key,start,stop,message})SendMessageW(item,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);SetTimer(h,1,200,nullptr);return 0;}
 case WM_COMMAND:if(HIWORD(w)==BN_CLICKED){if(LOWORD(w)==1)Run(true);if(LOWORD(w)==2)Run(false);}return 0;
 case WM_APP+1:if(worker.joinable())worker.join();latest=pending;busy=false;EnableWindow(start,TRUE);EnableWindow(stop,TRUE);SetWindowTextW(message,detail.c_str());InvalidateRect(h,nullptr,FALSE);return 0;
 case WM_TIMER:{std::unique_lock<std::mutex> guard(lock,std::try_to_lock);if(guard.owns_lock()&&opened&&!busy){try{latest=emulator.Status();}catch(const std::exception& e){std::string s=e.what();detail.assign(s.begin(),s.end());SetWindowTextW(message,detail.c_str());}}InvalidateRect(h,nullptr,FALSE);return 0;}
 case WM_ERASEBKGND:return 1;
 case WM_PAINT:{PAINTSTRUCT ps;HDC dc=BeginPaint(h,&ps);RECT r;GetClientRect(h,&r);HDC mem=CreateCompatibleDC(dc);HBITMAP bitmap=CreateCompatibleBitmap(dc,r.right,r.bottom),old=static_cast<HBITMAP>(SelectObject(mem,bitmap));HBRUSH bg=CreateSolidBrush(White);FillRect(mem,&r,bg);DeleteObject(bg);
  Draw(mem,28,24,L"Network Emulation",titleFont,Black);Draw(mem,28,66,preview?L"OFFLINE PREVIEW - no driver or packets":L"Outbound UDP / IPv4 - one explicit process and endpoint",font,Blue);
  Draw(mem,28,108,latest.active?L"ACTIVE":L"PASS-THROUGH",font,latest.active?Blue:Black);
  RECT card{28,505,698,618};bg=CreateSolidBrush(Gray);FillRect(mem,&card,bg);DeleteObject(bg);
  Draw(mem,46,521,L"Matched "+std::to_wstring(latest.matched)+L"     Dropped "+std::to_wstring(latest.dropped)+L"     Delayed "+std::to_wstring(latest.delayed),font,Black);
  Draw(mem,46,554,L"Retained "+std::to_wstring(latest.queued)+L" / 128     Injection errors "+std::to_wstring(latest.failed)+L"     Bypassed "+std::to_wstring(latest.bypassed),font,Black);
  Draw(mem,28,641,L"TCP, IPv6 and inbound traffic pass unchanged. Queue overflow passes through.",font,Black);
  BitBlt(dc,0,0,r.right,r.bottom,mem,0,0,SRCCOPY);SelectObject(mem,old);DeleteObject(bitmap);DeleteDC(mem);EndPaint(h,&ps);return 0;}
 case WM_SETCURSOR:if(reinterpret_cast<HWND>(w)==start||reinterpret_cast<HWND>(w)==stop){SetCursor(LoadCursorW(nullptr,IDC_HAND));return TRUE;}break;
 case WM_CLOSE:if(busy){MessageBoxW(h,L"Wait for the authorization request to finish.",L"Working",MB_OK);return 0;}DestroyWindow(h);return 0;
 case WM_DESTROY:KillTimer(h,1);if(worker.joinable())worker.join();emulator.Stop();DeleteObject(font);DeleteObject(titleFont);PostQuitMessage(0);return 0;
 }return DefWindowProcW(h,m,w,l);}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR command,int show){preview=command&&wcsstr(command,L"--preview");SetProcessDPIAware();WNDCLASSW c{};c.lpfnWndProc=Proc;c.hInstance=instance;c.lpszClassName=L"TitanNetworkEmulationApp";c.hCursor=LoadCursorW(nullptr,IDC_ARROW);RegisterClassW(&c);window=CreateWindowW(c.lpszClassName,L"Network Emulation SDK",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,746,720,nullptr,nullptr,instance,nullptr);if(!window)return 1;ShowWindow(window,show);MSG m;while(GetMessageW(&m,nullptr,0,0)>0){if(!IsDialogMessageW(window,&m)){TranslateMessage(&m);DispatchMessageW(&m);}}return 0;}
