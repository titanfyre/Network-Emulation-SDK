#include <ntifs.h>
#include <wdf.h>
#define NDIS630 1
#pragma warning(push)
#pragma warning(disable:4201)
#include <ndis.h>
#include <fwpsk.h>
#pragma warning(pop)
#include <initguid.h>
#include <fwpmk.h>
#include "../SDK/EmulationProtocol.h"
#include "../Common/DriverAuthorization.h"
#define TAG 'eNtT'
DEFINE_GUID(Interface,0x67be6a00,0x5ae7,0x4ac5,0x99,0xb2,0xa9,0x5e,0x31,0xc8,0x70,0x10);
static const GUID Keys[2]={
 {0x67be6a01,0x5ae7,0x4ac5,{0x99,0xb2,0xa9,0x5e,0x31,0xc8,0x70,0x10}},
 {0x67be6a02,0x5ae7,0x4ac5,{0x99,0xb2,0xa9,0x5e,0x31,0xc8,0x70,0x10}}};
typedef struct FLOW {LIST_ENTRY Link;UINT64 Handle;PEPROCESS Process;} FLOW;
typedef struct PACKET {
 LIST_ENTRY Link;struct DEVICE* Device;NET_BUFFER_LIST *Clone,*Original;UINT64 Endpoint;COMPARTMENT_ID Compartment;
 FWPS_TRANSPORT_SEND_PARAMS0 Args;ULONG Remote;ULONGLONG Deadline,Generation;
} PACKET;
typedef struct DEVICE {
 KSPIN_LOCK Lock;EX_RUNDOWN_REF Rundown;LIST_ENTRY Flows,Packets;
 WDFFILEOBJECT Owner;PEPROCESS Target;NE_POLICY Policy;NE_STATUS Counters;
 ULONGLONG Deadline,LastDue,Sequence;BOOLEAN Online,Active,Stopping;
 ULONG FlowCount,Queued,Reserved,Outstanding;UINT32 Callouts[2];BOOLEAN Registered[2],ProcessRegistered;
 HANDLE Injection;KEVENT Wake;PETHREAD Worker;
} DEVICE;
typedef struct FILE_CONTEXT {PEPROCESS Process;TITAN_AUTHORIZATION_STATE Authorization;BOOLEAN Closing;} FILE_CONTEXT;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE,Context)
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(FILE_CONTEXT,FileContext)
static PVOID volatile Singleton;
DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD DeviceAdd;
EVT_WDF_OBJECT_CONTEXT_CLEANUP Cleanup;
EVT_WDF_DEVICE_FILE_CREATE Create;
EVT_WDF_FILE_CLEANUP FileCleanup;
EVT_WDF_FILE_CLOSE FileClose;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL Control;
static BOOLEAN Live(DEVICE* c){return c->Online&&c->Active&&c->Deadline&&KeQueryInterruptTime()<c->Deadline;}
static VOID FreePacket(PACKET* p){DEVICE* c=p->Device;KIRQL old;FwpsFreeCloneNetBufferList0(p->Clone,0);FwpsDereferenceNetBufferList0(p->Original,KeGetCurrentIrql()==DISPATCH_LEVEL);ExFreePoolWithTag(p,TAG);KeAcquireSpinLock(&c->Lock,&old);--c->Outstanding;KeReleaseSpinLock(&c->Lock,old);}
static VOID NTAPI InjectComplete(VOID* context,NET_BUFFER_LIST* nbl,BOOLEAN dispatch){
 PACKET* p=context;UNREFERENCED_PARAMETER(nbl);UNREFERENCED_PARAMETER(dispatch);FreePacket(p);
}
static VOID Inject(DEVICE* c,PACKET* p){
 NTSTATUS status=FwpsInjectTransportSendAsync0(c->Injection,NULL,p->Endpoint,0,&p->Args,AF_INET,p->Compartment,p->Clone,InjectComplete,p);
 KIRQL old;KeAcquireSpinLock(&c->Lock,&old);if(NT_SUCCESS(status))++c->Counters.injected;else ++c->Counters.failed;KeReleaseSpinLock(&c->Lock,old);
 if(!NT_SUCCESS(status))FreePacket(p);
}
static VOID Worker(PVOID parameter){
 DEVICE* c=parameter;
 for(;;){
  PACKET* p=NULL;PEPROCESS target=NULL;LARGE_INTEGER timeout;PLARGE_INTEGER wait=NULL;KIRQL old;BOOLEAN stop;PVOID objects[2];NTSTATUS waited;
  KeAcquireSpinLock(&c->Lock,&old);KeClearEvent(&c->Wake);stop=c->Stopping;
  if(!IsListEmpty(&c->Packets)){
   PACKET* first=CONTAINING_RECORD(c->Packets.Flink,PACKET,Link);ULONGLONG now=KeQueryInterruptTime();
   if(stop||!Live(c)||first->Generation!=c->Counters.generation||first->Deadline<=now){RemoveEntryList(&first->Link);--c->Queued;if(!c->Queued)c->LastDue=0;p=first;}
   else{ULONGLONG due=first->Deadline;if(c->Deadline<due)due=c->Deadline;timeout.QuadPart=-(LONGLONG)(due-now);wait=&timeout;}
  }
  if(!p&&!stop&&Live(c)&&c->Target){target=c->Target;ObReferenceObject(target);if(!wait){timeout.QuadPart=-(LONGLONG)(c->Deadline-KeQueryInterruptTime());if(timeout.QuadPart>=0)timeout.QuadPart=-1;wait=&timeout;}}
  KeReleaseSpinLock(&c->Lock,old);
  if(p){Inject(c,p);continue;}if(stop)break;
  objects[0]=&c->Wake;objects[1]=target;
  waited=KeWaitForMultipleObjects(target?2u:1u,objects,WaitAny,Executive,KernelMode,FALSE,wait,NULL);
  if(target){if(waited==STATUS_WAIT_1){KeAcquireSpinLock(&c->Lock,&old);if(c->Target==target)c->Active=FALSE;KeReleaseSpinLock(&c->Lock,old);}ObDereferenceObject(target);}
 }
 PsTerminateSystemThread(STATUS_SUCCESS);
}
static VOID Unregister(UINT32 id){
 NTSTATUS status;LARGE_INTEGER pause;pause.QuadPart=-100000;do{status=FwpsCalloutUnregisterById0(id);if(!NT_SUCCESS(status)&&status!=STATUS_FWP_CALLOUT_NOT_FOUND)(VOID)KeDelayExecutionThread(KernelMode,FALSE,&pause);}while(!NT_SUCCESS(status)&&status!=STATUS_FWP_CALLOUT_NOT_FOUND);
}
static VOID NTAPI FlowDelete(UINT16 layer,UINT32 id,UINT64 context){
 WDFDEVICE device=(WDFDEVICE)InterlockedCompareExchangePointer(&Singleton,NULL,NULL);FLOW* f=(FLOW*)context;
 UNREFERENCED_PARAMETER(layer);UNREFERENCED_PARAMETER(id);
 if(device){DEVICE* c=Context(device);KIRQL old;KeAcquireSpinLock(&c->Lock,&old);RemoveEntryList(&f->Link);--c->FlowCount;KeReleaseSpinLock(&c->Lock,old);}
 ObDereferenceObject(f->Process);ExFreePoolWithTag(f,TAG);
}
static VOID NTAPI ProcessNotify(PEPROCESS process,HANDLE pid,PPS_CREATE_NOTIFY_INFO create){
 WDFDEVICE device=(WDFDEVICE)InterlockedCompareExchangePointer(&Singleton,NULL,NULL);UNREFERENCED_PARAMETER(pid);
 if(!create&&device){DEVICE* c=Context(device);KIRQL old;if(!ExAcquireRundownProtection(&c->Rundown))return;
  KeAcquireSpinLock(&c->Lock,&old);if(c->Target==process)c->Active=FALSE;KeReleaseSpinLock(&c->Lock,old);KeSetEvent(&c->Wake,IO_NO_INCREMENT,FALSE);ExReleaseRundownProtection(&c->Rundown);}
}
static VOID Continue(FWPS_CLASSIFY_OUT0* out){if(out->rights&FWPS_RIGHT_ACTION_WRITE)out->actionType=FWP_ACTION_CONTINUE;}
static VOID NTAPI Classify(const FWPS_INCOMING_VALUES0* v,const FWPS_INCOMING_METADATA_VALUES0* m,VOID* data,const VOID* classify,const FWPS_FILTER1* filter,UINT64 flow,FWPS_CLASSIFY_OUT0* out){
 WDFDEVICE device=(WDFDEVICE)InterlockedCompareExchangePointer(&Singleton,NULL,NULL);DEVICE* c;KIRQL old;
 UNREFERENCED_PARAMETER(classify);UNREFERENCED_PARAMETER(filter);Continue(out);if(!device)return;c=Context(device);if(!ExAcquireRundownProtection(&c->Rundown))return;
 if(v->layerId==FWPS_LAYER_ALE_FLOW_ESTABLISHED_V4){
  FLOW* f=NULL;
  if(!FWPS_IS_METADATA_FIELD_PRESENT(m,FWPS_METADATA_FIELD_PROCESS_ID)||!FWPS_IS_METADATA_FIELD_PRESENT(m,FWPS_METADATA_FIELD_FLOW_HANDLE)||!m->flowHandle)goto done;
  KeAcquireSpinLock(&c->Lock,&old);
  if(Live(c)&&c->Target&&(UINT64)c->Policy.process_id==m->processId){
   if(c->FlowCount<NE_FLOW_CAPACITY){f=ExAllocatePool2(POOL_FLAG_NON_PAGED,sizeof(*f),TAG);if(f){f->Handle=m->flowHandle;f->Process=c->Target;ObReferenceObject(f->Process);InsertTailList(&c->Flows,&f->Link);++c->FlowCount;}}
   else ++c->Counters.flow_overflow;
  }
  KeReleaseSpinLock(&c->Lock,old);
  if(f){NTSTATUS s=FwpsFlowAssociateContext0(f->Handle,FWPS_LAYER_DATAGRAM_DATA_V4,c->Callouts[1],(UINT64)f);if(!NT_SUCCESS(s)){KeAcquireSpinLock(&c->Lock,&old);RemoveEntryList(&f->Link);--c->FlowCount;KeReleaseSpinLock(&c->Lock,old);ObDereferenceObject(f->Process);ExFreePoolWithTag(f,TAG);}}
 }else if(v->layerId==FWPS_LAYER_DATAGRAM_DATA_V4&&flow&&data&&(out->rights&FWPS_RIGHT_ACTION_WRITE)){
  FLOW* f=(FLOW*)flow;NET_BUFFER_LIST* nbl=data;PACKET* p=NULL;ULONG remote,port,direction;NE_POLICY policy;ULONGLONG number,now,generation;
  if(v->valueCount<=FWPS_FIELD_DATAGRAM_DATA_V4_DIRECTION)goto done;
  remote=v->incomingValue[FWPS_FIELD_DATAGRAM_DATA_V4_IP_REMOTE_ADDRESS].value.uint32;port=v->incomingValue[FWPS_FIELD_DATAGRAM_DATA_V4_IP_REMOTE_PORT].value.uint16;direction=v->incomingValue[FWPS_FIELD_DATAGRAM_DATA_V4_DIRECTION].value.uint32;
  if(direction!=FWP_DIRECTION_OUTBOUND)goto done;
  if(FwpsQueryPacketInjectionState0(c->Injection,nbl,NULL)!=FWPS_PACKET_NOT_INJECTED)goto done;
  KeAcquireSpinLock(&c->Lock,&old);
  if(!Live(c)||f->Process!=c->Target||remote!=c->Policy.remote_ipv4||port!=c->Policy.remote_port){KeReleaseSpinLock(&c->Lock,old);goto done;}
  /* Reject unsupported layouts before either loss or delay can alter them. */
  if(NET_BUFFER_LIST_NEXT_NBL(nbl)||!NET_BUFFER_LIST_FIRST_NB(nbl)||NET_BUFFER_NEXT_NB(NET_BUFFER_LIST_FIRST_NB(nbl))||NET_BUFFER_DATA_LENGTH(NET_BUFFER_LIST_FIRST_NB(nbl))>65535||
     !FWPS_IS_METADATA_FIELD_PRESENT(m,FWPS_METADATA_FIELD_TRANSPORT_ENDPOINT_HANDLE)||
     (FWPS_IS_METADATA_FIELD_PRESENT(m,FWPS_METADATA_FIELD_IP_HEADER_SIZE)&&m->ipHeaderSize)||(FWPS_IS_METADATA_FIELD_PRESENT(m,FWPS_METADATA_FIELD_TRANSPORT_CONTROL_DATA)&&m->controlDataLength)){++c->Counters.bypassed;KeReleaseSpinLock(&c->Lock,old);goto done;}
  policy=c->Policy;generation=c->Counters.generation;++c->Counters.matched;number=++c->Sequence;
  if(policy.drop_every&&number%policy.drop_every==0){++c->Counters.dropped;KeReleaseSpinLock(&c->Lock,old);out->actionType=FWP_ACTION_BLOCK;out->flags|=FWPS_CLASSIFY_OUT_FLAG_ABSORB;out->rights&=~FWPS_RIGHT_ACTION_WRITE;goto done;}
  if(!policy.delay_ms){KeReleaseSpinLock(&c->Lock,old);goto done;}
  if(c->Outstanding>=NE_QUEUE_CAPACITY){++c->Counters.queue_overflow;++c->Counters.bypassed;KeReleaseSpinLock(&c->Lock,old);goto done;}
  ++c->Reserved;++c->Outstanding;KeReleaseSpinLock(&c->Lock,old);
  p=ExAllocatePool2(POOL_FLAG_NON_PAGED,sizeof(*p),TAG);if(!p)goto bypass;
  if(!NT_SUCCESS(FwpsAllocateCloneNetBufferList0(nbl,NULL,NULL,0,&p->Clone))){ExFreePoolWithTag(p,TAG);goto bypass;}
  FwpsReferenceNetBufferList0(nbl,FALSE);p->Original=nbl;p->Device=c;p->Endpoint=m->transportEndpointHandle;
  /* Compartment metadata is optional; retain the fixed layer value when supplied. */
  p->Compartment=UNSPECIFIED_COMPARTMENT_ID;
  if(FWPS_IS_METADATA_FIELD_PRESENT(m,FWPS_METADATA_FIELD_COMPARTMENT_ID))p->Compartment=m->compartmentId;
  else if(v->valueCount>FWPS_FIELD_DATAGRAM_DATA_V4_COMPARTMENT_ID&&v->incomingValue[FWPS_FIELD_DATAGRAM_DATA_V4_COMPARTMENT_ID].value.type==FWP_UINT32)p->Compartment=v->incomingValue[FWPS_FIELD_DATAGRAM_DATA_V4_COMPARTMENT_ID].value.uint32;
  p->Remote=RtlUlongByteSwap(remote);p->Args.remoteAddress=(UCHAR*)&p->Remote;p->Generation=generation;
  now=KeQueryInterruptTime();p->Deadline=now+((ULONGLONG)policy.delay_ms+(policy.jitter_ms?number%(2*policy.jitter_ms+1):0)-policy.jitter_ms)*10000;
  KeAcquireSpinLock(&c->Lock,&old);--c->Reserved;
  if(!Live(c)||c->Target!=f->Process||c->Counters.generation!=generation||c->Queued>=NE_QUEUE_CAPACITY){++c->Counters.bypassed;KeReleaseSpinLock(&c->Lock,old);FreePacket(p);goto done;}
  if(c->LastDue>p->Deadline)p->Deadline=c->LastDue;c->LastDue=p->Deadline;InsertTailList(&c->Packets,&p->Link);++c->Queued;++c->Counters.delayed;
  KeReleaseSpinLock(&c->Lock,old);out->actionType=FWP_ACTION_BLOCK;out->flags|=FWPS_CLASSIFY_OUT_FLAG_ABSORB;out->rights&=~FWPS_RIGHT_ACTION_WRITE;KeSetEvent(&c->Wake,IO_NO_INCREMENT,FALSE);goto done;
 bypass:KeAcquireSpinLock(&c->Lock,&old);--c->Reserved;--c->Outstanding;++c->Counters.bypassed;KeReleaseSpinLock(&c->Lock,old);
 }
done:ExReleaseRundownProtection(&c->Rundown);
}
static NTSTATUS NTAPI Notify(FWPS_CALLOUT_NOTIFY_TYPE type,const GUID* key,const FWPS_FILTER1* filter){UNREFERENCED_PARAMETER(type);UNREFERENCED_PARAMETER(key);UNREFERENCED_PARAMETER(filter);return STATUS_SUCCESS;}
VOID Create(WDFDEVICE device,WDFREQUEST request,WDFFILEOBJECT file){
 PIO_STACK_LOCATION stack=IoGetCurrentIrpStackLocation(WdfRequestWdmGetIrp(request));FILE_CONTEXT* f=FileContext(file);UNREFERENCED_PARAMETER(device);
 if(!stack->Parameters.Create.SecurityContext||!(stack->Parameters.Create.SecurityContext->DesiredAccess&FILE_READ_DATA)||!(stack->Parameters.Create.SecurityContext->DesiredAccess&FILE_WRITE_DATA)){WdfRequestComplete(request,STATUS_ACCESS_DENIED);return;}
 if(WdfFileObjectGetFileName(file)->Length){WdfRequestComplete(request,STATUS_OBJECT_NAME_INVALID);return;}
 f->Process=IoGetRequestorProcess(WdfRequestWdmGetIrp(request));if(!f->Process){WdfRequestComplete(request,STATUS_ACCESS_DENIED);return;}ObReferenceObject(f->Process);TitanAuthorizationInitialize(&f->Authorization,TITAN_AUTHORIZATION_NETWORK_EMULATION);WdfRequestComplete(request,STATUS_SUCCESS);
}
VOID FileCleanup(WDFFILEOBJECT file){DEVICE* c=Context(WdfFileObjectGetDevice(file));KIRQL old;PEPROCESS target=NULL;KeAcquireSpinLock(&c->Lock,&old);FileContext(file)->Closing=TRUE;if(c->Owner==file){c->Active=FALSE;c->Owner=NULL;target=c->Target;c->Target=NULL;c->Deadline=0;}KeReleaseSpinLock(&c->Lock,old);if(target)ObDereferenceObject(target);KeSetEvent(&c->Wake,IO_NO_INCREMENT,FALSE);}
VOID FileClose(WDFFILEOBJECT file){if(FileContext(file)->Process)ObDereferenceObject(FileContext(file)->Process);}
VOID Control(WDFQUEUE queue,WDFREQUEST request,size_t output,size_t input,ULONG code){
 DEVICE* c=Context(WdfIoQueueGetDevice(queue));WDFFILEOBJECT file=WdfRequestGetFileObject(request);FILE_CONTEXT* f=file?FileContext(file):NULL;PVOID buffer=NULL;NTSTATUS s=STATUS_INVALID_DEVICE_REQUEST;size_t bytes=0;KIRQL old;
 if(!f||f->Closing||f->Process!=IoGetRequestorProcess(WdfRequestWdmGetIrp(request))){WdfRequestComplete(request,STATUS_ACCESS_DENIED);return;}
 if(code==NE_IOCTL_CHALLENGE&&input==0&&output==sizeof(TITAN_AUTHORIZATION_CHALLENGE)){s=WdfRequestRetrieveOutputBuffer(request,output,&buffer,NULL);if(NT_SUCCESS(s)){s=TitanAuthorizationChallenge(&f->Authorization,buffer);if(NT_SUCCESS(s))bytes=output;}}
 else if(code==NE_IOCTL_AUTHORIZE&&input==sizeof(TITAN_AUTHORIZATION_TOKEN)&&output==0){s=WdfRequestRetrieveInputBuffer(request,input,&buffer,NULL);if(NT_SUCCESS(s)){s=TitanAuthorizationAccept(&f->Authorization,buffer);if(NT_SUCCESS(s)){KeAcquireSpinLock(&c->Lock,&old);if(c->Owner==file)c->Deadline=(ULONGLONG)f->Authorization.deadline;KeReleaseSpinLock(&c->Lock,old);KeSetEvent(&c->Wake,IO_NO_INCREMENT,FALSE);}}}
 else if(code==NE_IOCTL_START&&input==sizeof(NE_POLICY)&&output==0){
  NE_POLICY* policy=NULL;PEPROCESS target=NULL,previous=NULL;s=WdfRequestRetrieveInputBuffer(request,input,(PVOID*)&policy,NULL);
  if(NT_SUCCESS(s)&&!NeValidPolicy(policy))s=STATUS_INVALID_PARAMETER;
  if(NT_SUCCESS(s)&&!TitanAuthorizationAllowed(&f->Authorization))s=STATUS_ACCESS_DENIED;
  if(NT_SUCCESS(s))s=PsLookupProcessByProcessId((HANDLE)(ULONG_PTR)policy->process_id,&target);
  if(NT_SUCCESS(s)){LARGE_INTEGER zero;zero.QuadPart=0;if(KeWaitForSingleObject(target,Executive,KernelMode,FALSE,&zero)==STATUS_SUCCESS)s=STATUS_PROCESS_IS_TERMINATING;}
  if(NT_SUCCESS(s)){KeAcquireSpinLock(&c->Lock,&old);if(!c->Online)s=STATUS_DEVICE_NOT_READY;else if(f->Closing)s=STATUS_FILE_CLOSED;else{previous=c->Target;c->Target=target;target=NULL;c->Policy=*policy;c->Owner=file;c->Deadline=(ULONGLONG)f->Authorization.deadline;c->Active=TRUE;c->LastDue=0;c->Sequence=0;++c->Counters.generation;}KeReleaseSpinLock(&c->Lock,old);}
  if(target)ObDereferenceObject(target);if(previous)ObDereferenceObject(previous);KeSetEvent(&c->Wake,IO_NO_INCREMENT,FALSE);
 }else if(code==NE_IOCTL_STOP&&input==0&&output==0){PEPROCESS target;KeAcquireSpinLock(&c->Lock,&old);if(f->Closing||(c->Owner&&c->Owner!=file)){KeReleaseSpinLock(&c->Lock,old);WdfRequestComplete(request,STATUS_ACCESS_DENIED);return;}c->Active=FALSE;c->Owner=NULL;target=c->Target;c->Target=NULL;c->Deadline=0;KeReleaseSpinLock(&c->Lock,old);if(target)ObDereferenceObject(target);KeSetEvent(&c->Wake,IO_NO_INCREMENT,FALSE);s=STATUS_SUCCESS;}
 else if(code==NE_IOCTL_STATUS&&input==0&&output==sizeof(NE_STATUS)){s=WdfRequestRetrieveOutputBuffer(request,output,&buffer,NULL);if(NT_SUCCESS(s)){NE_STATUS* status=buffer;KeAcquireSpinLock(&c->Lock,&old);if(f->Closing){KeReleaseSpinLock(&c->Lock,old);WdfRequestComplete(request,STATUS_FILE_CLOSED);return;}*status=c->Counters;status->version=NE_VERSION;status->size=sizeof(*status);status->active=Live(c);status->queued=c->Outstanding;status->policy=c->Policy;{ULONGLONG now=KeQueryInterruptTime();status->remaining_100ns=c->Deadline>now?c->Deadline-now:0;}KeReleaseSpinLock(&c->Lock,old);bytes=output;}}
 else s=STATUS_INFO_LENGTH_MISMATCH;
 WdfRequestCompleteWithInformation(request,s,NT_SUCCESS(s)?bytes:0);
}
VOID Cleanup(WDFOBJECT object){
 DEVICE* c=Context(object);ULONG i;PEPROCESS target;KIRQL old;
 KeAcquireSpinLock(&c->Lock,&old);c->Active=FALSE;c->Online=FALSE;target=c->Target;c->Target=NULL;KeReleaseSpinLock(&c->Lock,old);if(target)ObDereferenceObject(target);
 if(c->ProcessRegistered){(VOID)PsSetCreateProcessNotifyRoutineEx(ProcessNotify,TRUE);c->ProcessRegistered=FALSE;}
 if(c->Registered[0]){Unregister(c->Callouts[0]);c->Registered[0]=FALSE;}
 ExWaitForRundownProtectionRelease(&c->Rundown);
 KeAcquireSpinLock(&c->Lock,&old);c->Stopping=TRUE;KeReleaseSpinLock(&c->Lock,old);KeSetEvent(&c->Wake,IO_NO_INCREMENT,FALSE);
 if(c->Worker){(VOID)KeWaitForSingleObject(c->Worker,Executive,KernelMode,FALSE,NULL);ObDereferenceObject(c->Worker);c->Worker=NULL;}
 for(;;){UINT64 handle;NTSTATUS removed;KeAcquireSpinLock(&c->Lock,&old);if(IsListEmpty(&c->Flows)){KeReleaseSpinLock(&c->Lock,old);break;}handle=CONTAINING_RECORD(c->Flows.Flink,FLOW,Link)->Handle;KeReleaseSpinLock(&c->Lock,old);removed=FwpsFlowRemoveContext0(handle,FWPS_LAYER_DATAGRAM_DATA_V4,c->Callouts[1]);if(!NT_SUCCESS(removed)){LARGE_INTEGER retry;retry.QuadPart=-10000;(VOID)KeDelayExecutionThread(KernelMode,FALSE,&retry);}}
 for(i=0;i<2;++i)if(c->Registered[i]){Unregister(c->Callouts[i]);c->Registered[i]=FALSE;}
 if(c->Injection){(VOID)FwpsInjectionHandleDestroy0(c->Injection);c->Injection=NULL;}
 InterlockedCompareExchangePointer(&Singleton,NULL,(PVOID)object);
}
NTSTATUS DeviceAdd(WDFDRIVER driver,PWDFDEVICE_INIT init){
 WDFDEVICE device;WDF_OBJECT_ATTRIBUTES a;WDF_FILEOBJECT_CONFIG files;WDF_IO_QUEUE_CONFIG queue;WDFQUEUE q;DEVICE* c;NTSTATUS s;HANDLE thread=NULL;ULONG i;
 DECLARE_CONST_UNICODE_STRING(sddl,L"D:P(A;;GA;;;SY)(A;;GA;;;BA)");DECLARE_CONST_UNICODE_STRING(link,L"\\DosDevices\\Global\\TitanNetworkEmulation");UNREFERENCED_PARAMETER(driver);
 WdfDeviceInitSetDeviceType(init,NE_DEVICE_TYPE);WdfDeviceInitSetExclusive(init,TRUE);WdfDeviceInitSetCharacteristics(init,FILE_DEVICE_SECURE_OPEN|FILE_AUTOGENERATED_DEVICE_NAME,FALSE);s=WdfDeviceInitAssignSDDLString(init,&sddl);if(!NT_SUCCESS(s))return s;
 WDF_FILEOBJECT_CONFIG_INIT(&files,Create,FileClose,FileCleanup);WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&a,FILE_CONTEXT);a.ExecutionLevel=WdfExecutionLevelPassive;a.SynchronizationScope=WdfSynchronizationScopeNone;WdfDeviceInitSetFileObjectConfig(init,&files,&a);
 WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&a,DEVICE);a.ExecutionLevel=WdfExecutionLevelPassive;a.SynchronizationScope=WdfSynchronizationScopeNone;a.EvtCleanupCallback=Cleanup;s=WdfDeviceCreate(&init,&a,&device);if(!NT_SUCCESS(s))return s;
 c=Context(device);KeInitializeSpinLock(&c->Lock);ExInitializeRundownProtection(&c->Rundown);InitializeListHead(&c->Flows);InitializeListHead(&c->Packets);KeInitializeEvent(&c->Wake,NotificationEvent,FALSE);
 if(InterlockedCompareExchangePointer(&Singleton,device,NULL)!=NULL)return STATUS_OBJECT_NAME_COLLISION;
 s=WdfDeviceCreateDeviceInterface(device,&Interface,NULL);if(!NT_SUCCESS(s))return s;s=WdfDeviceCreateSymbolicLink(device,&link);if(!NT_SUCCESS(s))return s;
 WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queue,WdfIoQueueDispatchSequential);queue.EvtIoDeviceControl=Control;WDF_OBJECT_ATTRIBUTES_INIT(&a);a.ExecutionLevel=WdfExecutionLevelPassive;s=WdfIoQueueCreate(device,&queue,&a,&q);if(!NT_SUCCESS(s))return s;
 s=FwpsInjectionHandleCreate0(AF_INET,FWPS_INJECTION_TYPE_TRANSPORT,&c->Injection);if(!NT_SUCCESS(s))return s;
 s=PsCreateSystemThread(&thread,THREAD_ALL_ACCESS,NULL,NULL,NULL,Worker,c);if(!NT_SUCCESS(s))return s;
 s=ObReferenceObjectByHandle(thread,SYNCHRONIZE,*PsThreadType,KernelMode,(PVOID*)&c->Worker,NULL);if(!NT_SUCCESS(s)){c->Stopping=TRUE;KeSetEvent(&c->Wake,IO_NO_INCREMENT,FALSE);(VOID)ZwWaitForSingleObject(thread,FALSE,NULL);ZwClose(thread);return s;}ZwClose(thread);
 for(i=0;i<2;++i){FWPS_CALLOUT1 callout;RtlZeroMemory(&callout,sizeof(callout));callout.calloutKey=Keys[i];callout.classifyFn=Classify;callout.notifyFn=Notify;if(i==1)callout.flowDeleteFn=FlowDelete;s=FwpsCalloutRegister1(WdfDeviceWdmGetDeviceObject(device),&callout,&c->Callouts[i]);if(!NT_SUCCESS(s))return s;c->Registered[i]=TRUE;}
 s=PsSetCreateProcessNotifyRoutineEx(ProcessNotify,FALSE);if(!NT_SUCCESS(s))return s;c->ProcessRegistered=TRUE;c->Online=TRUE;return STATUS_SUCCESS;
}
NTSTATUS DriverEntry(PDRIVER_OBJECT object,PUNICODE_STRING registry){WDF_DRIVER_CONFIG config;WDF_OBJECT_ATTRIBUTES a;WDF_DRIVER_CONFIG_INIT(&config,DeviceAdd);WDF_OBJECT_ATTRIBUTES_INIT(&a);a.ExecutionLevel=WdfExecutionLevelPassive;return WdfDriverCreate(object,registry,&a,&config,WDF_NO_HANDLE);}
