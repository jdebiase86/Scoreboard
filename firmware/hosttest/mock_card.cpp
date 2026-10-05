#include "logo_dir.h"
#include "../scoreboard/sb_render.h"
#include "../scoreboard/sb_png.h"
#include <stdio.h>
#include <stdlib.h>
void* sbAlloc(size_t n){return malloc(n);} void sbFree(void*p){free(p);} void sbBreathe(){}
static void ppm(const char* path, const Frame& fb){ FILE* f=fopen(path,"wb"); fprintf(f,"P6 %d %d 255\n",W*6,H*6);
 for(int y=0;y<H*6;y++)for(int x=0;x<W*6;x++){RGB c=fb.px[(y/6)*W+x/6];bool d=(x%6)&&(y%6)&&(x%6<5)&&(y%6<5);
 unsigned char p[3]={(unsigned char)(c>>16),(unsigned char)(c>>8),(unsigned char)c}; if(!d)p[0]=p[1]=p[2]=0; else if(!fb.lit[(y/6)*W+x/6])p[0]=p[1]=p[2]=18; fwrite(p,1,3,f);} fclose(f);}
static bool load(const char* name, Logo& L){ char path[300]; snprintf(path,300,LOGO_DIR "%s",name);
 FILE* f=fopen(path,"rb"); if(!f) return false; fseek(f,0,2); long n=ftell(f); fseek(f,0,0); uint8_t* b=(uint8_t*)malloc(n); fread(b,1,n,f); fclose(f);
 int w,h,e; uint8_t* rgba=decodePngRGBA(b,n,w,h,&e); free(b); if(!rgba) return false; bool ok=shrinkLogo(rgba,w,h,MATCHUP_W,MATCHUP_H,L); free(rgba); return ok;}
int main(){
  static ChanCard c; static Frame fb;
  scopy(c.away,"DAL"); scopy(c.home,"NYG"); c.hasAwayColor=true; c.awayColor=0x002A5C; c.hasHomeColor=true; c.homeColor=0x0B2265;
  c.hasScore=true; c.awayScore=10; c.homeScore=17; scopy(c.top,"3RD 8:42"); c.yours=true;
  Logo a,h; load("nfl/dal.png",a); load("nfl/nyg.png",h);
  c.la=a; c.lh=h; renderCard(fb,c,2,9); ppm("/tmp/card1.ppm",fb);
  c.la=Logo(); c.lh=Logo(); c.yours=false; scopy(c.away,"BUF"); scopy(c.home,"NE"); c.awayScore=24; c.homeScore=3; scopy(c.top,"2ND 1:12");
  renderCard(fb,c,5,9); ppm("/tmp/card2.ppm",fb);
  load("ncaa/57.png",a); load("ncaa/99.png",h); c.la=a; c.lh=h; c.hasScore=false; scopy(c.away,"FLA"); scopy(c.home,"LSU"); scopy(c.top,"10/11 7:30P"); c.yours=true;
  renderCard(fb,c,3,9); ppm("/tmp/card3.ppm",fb);
}
