#pragma once
#ifdef _KERNEL_MODE
#include <ntddk.h>
typedef ULONG NE_U32;
typedef ULONGLONG NE_U64;
#else
#include <windows.h>
#include <winioctl.h>
#include <cstdint>
typedef uint32_t NE_U32;
typedef uint64_t NE_U64;
#endif
#define NE_VERSION 1u
#define NE_DEVICE_TYPE 0x8394u
#define NE_QUEUE_CAPACITY 128u
#define NE_FLOW_CAPACITY 64u
#define NE_CTL(n) CTL_CODE(NE_DEVICE_TYPE,0x800u+(n),METHOD_BUFFERED,FILE_READ_DATA|FILE_WRITE_DATA)
#define NE_IOCTL_CHALLENGE NE_CTL(0)
#define NE_IOCTL_AUTHORIZE NE_CTL(1)
#define NE_IOCTL_START NE_CTL(2)
#define NE_IOCTL_STOP NE_CTL(3)
#define NE_IOCTL_STATUS NE_CTL(4)
typedef struct NE_POLICY {
 NE_U32 version,size,process_id,remote_ipv4,remote_port,drop_every,delay_ms,jitter_ms;
 NE_U32 reserved[4];
} NE_POLICY;
typedef struct NE_STATUS {
 NE_U32 version,size,active,queued;
 NE_U64 matched,dropped,delayed,injected,failed,bypassed,flow_overflow,queue_overflow;
 NE_U64 generation,remaining_100ns;
 NE_POLICY policy;
} NE_STATUS;
static __inline int NeValidPolicy(const NE_POLICY* p){
 unsigned i;
 if(p->version!=NE_VERSION||p->size!=sizeof(*p)||!p->process_id||!p->remote_ipv4||
    p->remote_ipv4>=0xe0000000u||p->remote_ipv4<0x01000000u||!p->remote_port||p->remote_port>65535||p->drop_every>1000000||p->delay_ms>1000||p->jitter_ms>p->delay_ms)return 0;
 for(i=0;i<4;++i)if(p->reserved[i])return 0;
 return 1;
}
