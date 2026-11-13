#include "NetworkEmulationSDKJsonSDK.h"
#include <string.h>
int main(void){uint64_t session=0;if(network_emulation_sdk_json_open(&session))return 1;char *reply=network_emulation_sdk_json_call(session,"{",1);if(!reply){network_emulation_sdk_json_close(session);return 2;}int failed=strstr(reply,"\"ok\":false")==0;network_emulation_sdk_json_free(reply);if(network_emulation_sdk_json_close(session))return 3;return failed;}
