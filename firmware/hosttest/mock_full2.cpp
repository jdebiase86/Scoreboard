// Renders the new screens from real ESPN data (feeds/nfl_live.json) and real logos.
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
extern int fullStyle;
static bool loadLogoWH(const char* lg, const char* abbr, int bw, int bh, Logo& L){ char pat[300]; snprintf(pat,300,"/mnt/user-data/uploads/Documents/Scoreboard/scoreboard_logos/all/%s_%s_*_full-dark.png",lg,abbr);
 glob_t g; if(glob(pat,0,0,&g)||!g.gl_pathc) return false; FILE* f=fopen(g.gl_pathv[0],"rb"); globfree(&g); if(!f) return false;
 fseek(f,0,2); long n=ftell(f); fseek(f,0,0); uint8_t* b=(uint8_t*)malloc(n); fread(b,1,n,f); fclose(f);
 int w,h,e; uint8_t* rgba=decodePngRGBA(b,n,w,h,&e); free(b); if(!rgba) return false; bool ok=shrinkLogo(rgba,w,h,bw,bh,L); free(rgba); return ok;}
int main(){
  std::ifstream in("feeds/nfl_live.json"); std::stringstream ss; ss<<in.rdbuf(); std::string js=ss.str();
  static JsonDocument filter; buildScoreboardFilter(filter); static JsonDocument doc;
  deserializeJson(doc, js, DeserializationOption::Filter(filter), DeserializationOption::NestingLimit(40));
  time_t now=1791148114; static Frame fb; static Game g;
  const char* games[2][3]={{"MIN","MIA","MIN"},{"LV","KC","LV"}};
  renderScoreColors(WHITE,WHITE);
  for(int gi=0;gi<2;gi++){
    const TeamDef* t=nullptr; for(int i=0;i<NTEAMS;i++) if(TEAMS[i].league==L_NFL && !strcmp(TEAMS[i].abbr,games[gi][0])) t=&TEAMS[i];
    parseGame(doc.as<JsonObjectConst>(),*t,false,now,g);
    if(gi==1){ g.redzone=true; }
    for(int st=0;st<2;st++){
      fullStyle=st; static Logo la,lh; la=Logo(); lh=Logo();
      int bw=st?26:26, bh=st?24:18;
      loadLogoWH("nfl",g.away.abbr,bw,bh,la); loadLogoWH("nfl",g.home.abbr,bw,bh,lh);
      renderFootballFull(fb,g,&la,&lh,2400); char p[64]; snprintf(p,64,"/tmp/fx2_%d_%d.ppm",gi,st); ppm(p,fb);
      printf("%s v %s style %d logos %dx%d %dx%d\n",g.away.abbr,g.home.abbr,st,la.w,la.h,lh.w,lh.h);
    }
  }
}
