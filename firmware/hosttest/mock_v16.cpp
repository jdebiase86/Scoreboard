#include "logo_dir.h"
#include "../scoreboard/sb_render.h"
#include "../scoreboard/sb_png.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
void* sbAlloc(size_t n){return malloc(n);} void sbFree(void*p){free(p);} void sbBreathe(){}
static void ppm(const char* path, const Frame& fb){ FILE* f=fopen(path,"wb"); fprintf(f,"P6 %d %d 255\n",W*6,H*6);
 for(int y=0;y<H*6;y++)for(int x=0;x<W*6;x++){RGB c=fb.px[(y/6)*W+x/6];bool d=(x%6)&&(y%6)&&(x%6<5)&&(y%6<5);
 unsigned char p[3]={(unsigned char)(c>>16),(unsigned char)(c>>8),(unsigned char)c}; if(!d)p[0]=p[1]=p[2]=0; else if(!fb.lit[(y/6)*W+x/6])p[0]=p[1]=p[2]=18; fwrite(p,1,3,f);} fclose(f);}
static bool load(const char* name, int bw, int bh, Logo& L){ char path[300]; snprintf(path,300,LOGO_DIR "%s",name);
 FILE* f=fopen(path,"rb"); if(!f) return false; fseek(f,0,2); long n=ftell(f); fseek(f,0,0); uint8_t* b=(uint8_t*)malloc(n); fread(b,1,n,f); fclose(f);
 int w,h,e; uint8_t* rgba=decodePngRGBA(b,n,w,h,&e); free(b); if(!rgba) return false; bool ok=shrinkLogo(rgba,w,h,bw,bh,L); free(rgba); return ok;}
static void banner(Frame& fb, const char* s, int y, RGB c){ int w=tw(s,F3); int x=(W-w)>>1;
 for(int yy=y-1;yy<=y+5;yy++) for(int xx=x-1;xx<=x+w;xx++) fb.put(xx,yy,0); text(fb,x,y,s,c,F3);}
static void logoCard(Frame& fb, const Logo& L, const char* word){ fb.clear(); drawLogo(fb,L,(W-L.w)>>1,(54-L.h)>>1>0?(54-L.h)>>1:0,1); banner(fb,word,58,0xFFBE00);}
int main(){
  static Frame fb; Logo nyg, dal, nygS, dalS;
  load("nfl/nyg.png",54,54,nyg); load("nfl/dal.png",54,54,dal);
  load("nfl/nyg.png",MATCHUP_W,MATCHUP_H,nygS); load("nfl/dal.png",MATCHUP_W,MATCHUP_H,dalS);
  logoCard(fb,nyg,"FIELD GOAL"); ppm("/tmp/m1.ppm",fb);
  logoCard(fb,nyg,"1ST DOWN"); ppm("/tmp/m2.ppm",fb);
  // kickoff end: both logos + KICKOFF
  fb.clear();
  drawLogo(fb,dalS,14-(dalS.w>>1),12+((MATCHUP_H-dalS.h)>>1),1); drawLogo(fb,nygS,50-(nygS.w>>1),12+((MATCHUP_H-nygS.h)>>1),1);
  text(fb,(W-tw("AT",F3))>>1,22,"AT",DATEC,F3);
  text(fb,(W-tw("KICKOFF",F5))>>1,47,"KICKOFF",0xFFBE00,F5); ppm("/tmp/m3.ppm",fb);
  // score glow / their score flash
  static Game g; g.valid=true; g.sport=FOOTBALL; g.state=ST_IN; scopy(g.periodLabel,"2ND"); scopy(g.clock,"4:12");
  scopy(g.home.abbr,"NYG"); g.home.hasColor=true; g.home.color=0x0B2265; g.home.hasScore=true; g.home.score=17;
  scopy(g.away.abbr,"DAL"); g.away.hasColor=true; g.away.color=0x002A5C; g.away.hasScore=true; g.away.score=10; g.pinnedHome=true; g.possession=2;
  renderGame(fb,g,0,nullptr,nullptr); 
  // recolour the giants score gold (glow)
  for(int y=7;y<21;y++) for(int x=40;x<64;x++) if(fb.px[y*W+x]==WHITE) fb.px[y*W+x]=0xFFBE00;
  ppm("/tmp/m4.ppm",fb);
  renderGame(fb,g,0,nullptr,nullptr);
  RGB dc=ledColor(true,0x002A5C);
  for(int y=22;y<36;y++) for(int x=40;x<64;x++) if(fb.px[y*W+x]==WHITE) fb.px[y*W+x]=dc;
  ppm("/tmp/m5.ppm",fb);
  // win card
  fb.clear(); drawLogo(fb,nyg,(W-nyg.w)>>1,0,1);
  banner(fb,"GIANTS WIN",51,0xFFBE00); banner(fb,"24-17",58,WHITE); ppm("/tmp/m6.ppm",fb);
}
