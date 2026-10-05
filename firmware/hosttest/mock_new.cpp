// Renders the new screens from real ESPN data (feeds/nfl_live.json) and real logos.
#include "logo_dir.h"
#include "../scoreboard/sb_parse.h"
#include "../scoreboard/sb_render.h"
#include "../scoreboard/sb_png.h"
#include <fstream>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <glob.h>
void* sbAlloc(size_t n){return malloc(n);} void sbFree(void*p){free(p);} void sbBreathe(){}
static void ppm(const char* path, const Frame& fb){ FILE* f=fopen(path,"wb"); fprintf(f,"P6 %d %d 255\n",W*6,H*6);
 for(int y=0;y<H*6;y++)for(int x=0;x<W*6;x++){RGB c=fb.px[(y/6)*W+x/6];bool d=(x%6)&&(y%6)&&(x%6<5)&&(y%6<5);
 unsigned char p[3]={(unsigned char)(c>>16),(unsigned char)(c>>8),(unsigned char)c}; if(!d)p[0]=p[1]=p[2]=0; else if(!fb.lit[(y/6)*W+x/6])p[0]=p[1]=p[2]=18; fwrite(p,1,3,f);} fclose(f);}
static bool loadLogo(const char* lg, const char* abbr, int sz, Logo& L){ std::string path=findLogo(lg,abbr); if(path.empty()) return false;
 FILE* f=fopen(path.c_str(),"rb"); if(!f) return false;
 fseek(f,0,2); long n=ftell(f); fseek(f,0,0); uint8_t* b=(uint8_t*)malloc(n); fread(b,1,n,f); fclose(f);
 int w,h,e; uint8_t* rgba=decodePngRGBA(b,n,w,h,&e); free(b); if(!rgba) return false; bool ok=shrinkLogo(rgba,w,h,sz,sz,L); free(rgba); return ok;}
int main(){
  std::ifstream in("feeds/nfl_live.json"); std::stringstream ss; ss<<in.rdbuf(); std::string js=ss.str();
  static JsonDocument filter; buildScoreboardFilter(filter); static JsonDocument doc;
  deserializeJson(doc, js, DeserializationOption::Filter(filter), DeserializationOption::NestingLimit(40));
  time_t now=1791148114; static Game tick; buildTicker(doc.as<JsonObjectConst>(),"",FOOTBALL,false,now,tick);
  static FullGame fg[MAX_TICK]; int n=0;
  for(int pass=0;pass<3;pass++) for(int i=0;i<tick.nticker;i++){ const Tick& k=tick.ticker[i]; bool fin=!strcmp(k.status,"F"), sch=!k.score[0]; int kind=fin?1:sch?2:0; if(kind!=pass) continue;
    FullGame& d=fg[n++]; d.t=k; Logo a,h; if(loadLogo("nfl",k.away,SMALL_LOGO,a)){ memcpy(d.pa,a.pix,a.n*sizeof(LogoPix)); d.la=a; d.la.pix=d.pa;} if(loadLogo("nfl",k.home,SMALL_LOGO,h)){ memcpy(d.ph,h.pix,h.n*sizeof(LogoPix)); d.lh=h; d.lh.pix=d.ph;} }
  printf("%d games\n",n);
  static Frame fb;
  for(int p=0;p<3;p++){ renderFullTicker(fb,fg,n,p,"ALL NFL"); char path[64]; snprintf(path,64,"/tmp/ft%d.ppm",p); ppm(path,fb); }
  // full-game: MIN (home) v MIA
  const TeamDef* min=nullptr; for(int i=0;i<NTEAMS;i++) if(TEAMS[i].league==L_NFL && !strcmp(TEAMS[i].abbr,"MIN")) min=&TEAMS[i];
  static Game g; parseGame(doc.as<JsonObjectConst>(),*min,false,now,g);
  printf("MIN %d-%d yl %d d %d dist %d to %d/%d win %d sdd %s poss %d\n",g.home.score,g.away.score,g.yardLine,g.down,g.distance,g.toHome,g.toAway,g.winHome,g.shortDD,g.possession);
  Logo la,lh; loadLogo("nfl","MIA",SMALL_LOGO,la); loadLogo("nfl","MIN",SMALL_LOGO,lh);
  renderScoreColors(WHITE,WHITE); renderFootballFull(fb,g,&la,&lh,2400); ppm("/tmp/fg1.ppm",fb);
  const TeamDef* kc=nullptr; for(int i=0;i<NTEAMS;i++) if(TEAMS[i].league==L_NFL && !strcmp(TEAMS[i].abbr,"LV")) kc=&TEAMS[i];
  parseGame(doc.as<JsonObjectConst>(),*kc,false,now,g); printf("LV yl %d rz %d\n",g.yardLine,g.redzone);
  la=Logo(); lh=Logo(); loadLogo("nfl","KC",SMALL_LOGO,la); loadLogo("nfl","LV",SMALL_LOGO,lh);
  renderFootballFull(fb,g,&la,&lh,0); ppm("/tmp/fg2.ppm",fb);
}
