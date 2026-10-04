#include "../scoreboard/sb_parse.h"
#include <stdlib.h>
void* sbAlloc(size_t n){return malloc(n);} void sbFree(void*p){free(p);} void sbBreathe(){}
int main(){
  const char* c[][2]={{"1.4","1.3"},{"1.10","1.9"},{"1.3","1.3"},{"2.0","1.99"},{"1.3.1","1.3"},{"1.3","1.3.0"}};
  for(auto&p:c) printf("cmp(%s,%s)=%d\n",p[0],p[1],versionCompare(p[0],p[1]));
  const char* n[]={"scoreboard-1.4.bin","Scoreboard-1.10.BIN","scoreboard_v1_2.merged.bin","scoreboard-.bin","scoreboard-1.4 (1).bin","scoreboard-hw.bin"};
  for(auto s:n){ char v[16]=""; bool ok=versionFromAsset(s,v); printf("%-28s -> %s %s\n",s,ok?"yes":"no",v);}
}
