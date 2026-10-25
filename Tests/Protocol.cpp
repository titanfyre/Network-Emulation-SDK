#include "../SDK/Emulator.h"
#include <iostream>
int main(){unsigned checks=0;auto test=[&](bool v){++checks;if(!v)throw "policy boundary failure";};
 NE_POLICY p{NE_VERSION,sizeof(NE_POLICY),123,0x7f000001,45000,3,40,5};test(NeValidPolicy(&p));
 for(unsigned i=0;i<100000;++i){auto q=p;switch(i%8){case 0:q.version=0;break;case 1:q.size=0;break;case 2:q.process_id=0;break;case 3:q.remote_ipv4=0;break;case 4:q.remote_port=65536;break;case 5:q.delay_ms=1001;break;case 6:q.jitter_ms=41;break;case 7:q.reserved[i%4]=1;break;}test(!NeValidPolicy(&q));}
 p.delay_ms=1000;p.jitter_ms=1000;p.drop_every=1000000;test(NeValidPolicy(&p));p.jitter_ms=1001;test(!NeValidPolicy(&p));std::cout<<checks<<" bounded policy checks passed\n";
}
