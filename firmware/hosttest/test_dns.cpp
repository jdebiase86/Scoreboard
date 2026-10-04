#include "../scoreboard/sb_dns.h"
#include <stdio.h>
#include <arpa/inet.h>
#include <stdlib.h>
int main(int argc,char**argv){
  // query bytes in as hex on argv[1], reply hex out
  const char* h=argv[1]; size_t n=strlen(h)/2; uint8_t q[600],r[600];
  for(size_t i=0;i<n;i++){ unsigned v; sscanf(h+2*i,"%2x",&v); q[i]=v; }
  uint32_t ip=inet_addr("192.168.4.1");
  size_t len=dnsBuildReply(q,n,ip,r,sizeof r);
  for(size_t i=0;i<len;i++) printf("%02x",r[i]); puts("");
}
