#include "../scoreboard/sb_render.h"
#include <stdio.h>
#include <stdlib.h>
void* sbAlloc(size_t n){return malloc(n);} void sbFree(void*p){free(p);} void sbBreathe(){}
static void ppm(const char* path, const Frame& fb){ FILE* f=fopen(path,"wb"); fprintf(f,"P6 %d %d 255\n",W*6,H*6);
 for(int y=0;y<H*6;y++)for(int x=0;x<W*6;x++){RGB c=fb.px[(y/6)*W+x/6];bool d=(x%6)&&(y%6)&&(x%6<5)&&(y%6<5);
 unsigned char p[3]={(unsigned char)(d?c>>16:20),(unsigned char)(d?c>>8:20),(unsigned char)(d?c:20)}; if(!d)p[0]=p[1]=p[2]=0; else if(!fb.lit[(y/6)*W+x/6])p[0]=p[1]=p[2]=18; fwrite(p,1,3,f);} fclose(f);}
int main(){
  static Game g; g.valid=true; g.sport=FOOTBALL; g.state=ST_IN; g.period=3; scopy(g.periodLabel,"3RD"); scopy(g.clock,"8:42");
  scopy(g.home.abbr,"NYG"); g.home.hasColor=true; g.home.color=0x0B2265; g.home.hasScore=true; g.home.score=17;
  scopy(g.away.abbr,"ARI"); g.away.hasColor=true; g.away.color=0x97233F; g.away.hasScore=true; g.away.score=10;
  g.pinnedHome=true; g.possession=1; scopy(g.downDistance,"2nd & 7 at ARI 34");
  scopy(g.lastPlay,"J.Dart pass short right to M.Nabers for 12 yards");
  static Frame fb; renderGame(fb,g,0,nullptr,nullptr,true,0); ppm("/tmp/big_fb.ppm",fb);
  renderGame(fb,g,0,nullptr,nullptr,true,2000); ppm("/tmp/big_fb2.ppm",fb);
  g.home.score=104; g.away.score=99; g.sport=BASKETBALL; scopy(g.home.abbr,"NYK"); scopy(g.away.abbr,"BOS"); g.possession=0;
  renderGame(fb,g,0,nullptr,nullptr,true,0); ppm("/tmp/big_bb.ppm",fb);
  g.sport=FOOTBALL; scopy(g.home.abbr,"TENN"); scopy(g.away.abbr,"FLA"); g.home.score=28; g.away.score=21;
  renderGame(fb,g,0,nullptr,nullptr,true,0); ppm("/tmp/big_cfb.ppm",fb);
}
