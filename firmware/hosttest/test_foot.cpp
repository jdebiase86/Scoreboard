// Host test: Yankees live on top, college football live elsewhere -> college ticker underneath
#include "../scoreboard/sb_parse.h"
#include "../scoreboard/sb_render.h"
#include <fstream>
#include <stdlib.h>
void* sbAlloc(size_t n){return malloc(n);} void sbFree(void*p){free(p);} void sbBreathe(){}
static void load(const char* f, JsonDocument& d, JsonDocument& filt){ std::ifstream in(f); deserializeJson(d,in,DeserializationOption::Filter(filt),DeserializationOption::NestingLimit(40)); }
static const TeamDef* team(League l,const char*a){ for(auto&t:TEAMS) if(t.league==l&&!strcmp(t.abbr,a)) return &t; return nullptr; }
static void ppm(const char* path, const Frame& fb){ FILE* f=fopen(path,"wb"); fprintf(f,"P6 %d %d 255\n",W*8,H*8);
 for(int y=0;y<H*8;y++)for(int x=0;x<W*8;x++){RGB c=fb.px[(y/8)*W+x/8];bool d=(x%8)&&(y%8)&&(x%8<7)&&(y%8<7);
 unsigned char p[3]={(unsigned char)(d?c>>16:0),(unsigned char)(d?c>>8:0),(unsigned char)(d?c:0)}; if(d&&!c)p[0]=p[1]=p[2]=22; fwrite(p,1,3,f);} fclose(f);}
int main(){
  setenv("TZ","EST5EDT,M3.2.0,M11.1.0",1); tzset();
  JsonDocument filt; buildScoreboardFilter(filt);
  JsonDocument mlb, cfb, nfl; load("/tmp/feeds/mlb_live.json",mlb,filt); load("/tmp/feeds/cfb_ranked.json",cfb,filt); load("/tmp/feeds/nfl_pre.json",nfl,filt);
  time_t now=time(nullptr);
  static Game g, tc, tn; bool live; time_t kick;
  parseGame(mlb.as<JsonObjectConst>(),*team(L_MLB,"NYY"),false,now,g);
  printf("yankees own ticker: %d games, ranked=%d\n", g.nticker, g.ranked);
  footballSummary(cfb.as<JsonObjectConst>(), true, now, live, kick, tc);
  printf("college: live=%d nextKick=%ld ticker=%d\n", live, (long)kick, tc.nticker);
  bool nlive; footballSummary(nfl.as<JsonObjectConst>(), false, now, nlive, kick, tn);
  printf("nfl: live=%d nextKick=%s ticker=%d\n", nlive, kick? "set":"none", tn.nticker);
  mergeTicker(g, nlive?&tn:nullptr, live?&tc:nullptr, live);
  printf("merged under yankees: %d games, ranked=%d:", g.nticker, g.ranked);
  for(int i=0;i<g.nticker;i++) printf(" %s@%s(%s)", g.ticker[i].away, g.ticker[i].home, g.ticker[i].status); puts("");
  static Frame fb;
  for (int p=0;p<2;p++){ renderGame(fb,g,p,nullptr,nullptr); char n[64]; snprintf(n,64,"/tmp/foot_%d.ppm",p); ppm(n,fb);}
  // college on top: its own game must not appear in the ticker
  static Game f; parseGame(cfb.as<JsonObjectConst>(),*team(L_CFB,"FLA"),true,now,f);
  mergeTicker(f,&tc,nullptr,true); int self=0; for(int i=0;i<f.nticker;i++) if(!strcmp(f.ticker[i].home,"FLA")) self++;
  printf("FLA on top: ticker %d games, own game in ticker: %d\n", f.nticker, self);
}
