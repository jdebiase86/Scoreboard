#include "../scoreboard/sb_render.h"
#include <stdio.h>
#include <stdlib.h>
void* sbAlloc(size_t n){return malloc(n);} void sbFree(void*p){free(p);} void sbBreathe(){}
static void ppm(const char* path, const Frame& fb){ FILE* f=fopen(path,"wb"); fprintf(f,"P6 %d %d 255\n",W*6,H*6);
 for(int y=0;y<H*6;y++)for(int x=0;x<W*6;x++){RGB c=fb.px[(y/6)*W+x/6];bool d=(x%6)&&(y%6)&&(x%6<5)&&(y%6<5);
 unsigned char p[3]={(unsigned char)(c>>16),(unsigned char)(c>>8),(unsigned char)c}; if(!d)p[0]=p[1]=p[2]=0; else if(!fb.lit[(y/6)*W+x/6])p[0]=p[1]=p[2]=18; fwrite(p,1,3,f);} fclose(f);}
int main(){ static Game g; static Frame fb; g.valid=true; g.sport=FOOTBALL; g.state=ST_IN; scopy(g.periodLabel,"2ND"); scopy(g.clock,"11:23");
 scopy(g.home.abbr,"NYG"); g.home.hasColor=true; g.home.color=0x0B2265; g.home.hasScore=true; g.home.score=6;
 scopy(g.away.abbr,"ARI"); g.away.hasColor=true; g.away.color=0x97233F; g.away.hasScore=true; g.away.score=14; g.pinnedHome=true; g.possession=1;
 scopy(g.downDistance,"1st & 10 at NYG 25"); renderGame(fb,g,0,nullptr,nullptr); ppm("/tmp/shot.ppm",fb);
 for(int y=0;y<H;y++){ for(int x=0;x<W;x++) putchar(fb.lit[y*W+x]? (fb.px[y*W+x]==WHITE?'#':'+'):'.'); putchar('\n'); if(y>36) break;} }
