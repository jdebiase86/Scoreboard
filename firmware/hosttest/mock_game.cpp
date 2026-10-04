#include "../scoreboard/sb_render.h"
#include "../scoreboard/sb_png.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
void* sbAlloc(size_t n){return malloc(n);} void sbFree(void*p){free(p);} void sbBreathe(){}
static void ppm(const char* path, const Frame& fb){ FILE* f=fopen(path,"wb"); fprintf(f,"P6 %d %d 255\n",W*6,H*6);
 for(int y=0;y<H*6;y++)for(int x=0;x<W*6;x++){RGB c=fb.px[(y/6)*W+x/6];bool d=(x%6)&&(y%6)&&(x%6<5)&&(y%6<5);
 unsigned char p[3]={(unsigned char)(c>>16),(unsigned char)(c>>8),(unsigned char)c}; if(!d)p[0]=p[1]=p[2]=0; else if(!fb.lit[(y/6)*W+x/6])p[0]=p[1]=p[2]=18; fwrite(p,1,3,f);} fclose(f);}
static bool load(const char* name, int bw, int bh, Logo& L){ char path[300]; snprintf(path,300,"/mnt/user-data/uploads/Documents/Scoreboard/scoreboard_logos/%s",name);
 FILE* f=fopen(path,"rb"); if(!f) return false; fseek(f,0,2); long n=ftell(f); fseek(f,0,0); uint8_t* b=(uint8_t*)malloc(n); fread(b,1,n,f); fclose(f);
 int w,h,e; uint8_t* rgba=decodePngRGBA(b,n,w,h,&e); free(b); if(!rgba) return false; bool ok=shrinkLogo(rgba,w,h,bw,bh,L); free(rgba); return ok;}
static const char* const BALL[] = {"0011100", "0111110", "1112111", "0111110", "0011100"};
static RGB dim(RGB c,int pct){ return rgb(((c>>16)&255)*pct/100,((c>>8)&255)*pct/100,(c&255)*pct/100); }
// one full-game football frame
static void gameScreen(Frame& fb, const char* fa, const char* fh, RGB ca, RGB ch, int sa, int sh, int toA, int toH,
                       bool ballHome, int yardFromAwayGoal, int toGo, bool goingRight, int winHome, const char* status, const char* dd, const char* play, int scroll){
  fb.clear();
  text(fb,2,1,status,CLOCK,F3); text(fb,W-2-tw(dd,F3),1,dd,GRAY,F3);
  for(int k=0;k<2;k++){ bool home=k; int y=8+k*18; Logo L; load(home?fh:fa,16,16,L); drawLogo(fb,L,1+((16-L.w)>>1),y+((16-L.h)>>1),1);
    char s[6]; snprintf(s,6,"%d",home?sh:sa); text(fb,W-2-tw(s,F5,2),y+1,s,WHITE,F5,2);
    if(ballHome==home) sprite(fb,22,y+5,BALL,5,BROWN);
    int to=home?toH:toA; RGB tc=home?ch:ca; for(int i=0;i<3;i++) for(int xx=0;xx<3;xx++) fb.put(21+i*4+xx,y+14, i<to?rgb(255,200,0):rgb(50,50,50)); }
  // field: x 2..61, end zones 4 px each in team colours, 100 yards over 52 px
  int fy=45; for(int x=2;x<=61;x++){ for(int yy=fy;yy<fy+5;yy++) fb.put(x,yy,rgb(10,60,20)); }
  for(int x=2;x<6;x++) for(int yy=fy;yy<fy+5;yy++) fb.put(x,yy,dim(ca,70));
  for(int x=58;x<=61;x++) for(int yy=fy;yy<fy+5;yy++) fb.put(x,yy,dim(ch,70));
  for(int yd=10;yd<100;yd+=10){ int x=6+yd*52/100; fb.put(x,fy,rgb(40,110,50)); fb.put(x,fy+4,rgb(40,110,50)); }
  int bx=6+yardFromAwayGoal*52/100; int fdx=6+(yardFromAwayGoal+(goingRight?toGo:-toGo))*52/100;
  for(int yy=fy;yy<fy+5;yy++) fb.put(fdx,yy,rgb(255,215,0));
  fb.put(bx,fy+1,BROWN); fb.put(bx,fy+2,BROWN); fb.put(bx,fy+3,BROWN); fb.put(bx+(goingRight?1:-1),fy+2,WHITE); fb.put(bx+(goingRight?2:-2),fy+2,WHITE);
  // win probability bar
  int split=2+(100-winHome)*60/100; for(int x=2;x<=61;x++) fb.put(x,52, x<split?ca:ch);
  // last play scrolling
  int x=W-scroll; text(fb,x,57,play,DATEC,F3);
}
int main(){ static Frame fb;
  gameScreen(fb,"nfl_500-dark_scoreboard_ari.png","nfl_500-dark_scoreboard_nyg.png",ledColor(true,0x97233F),ledColor(true,0x0B2265),14,24,2,3,true,62,6,true,78,"3RD 8:42","2ND&6","J.DART PASS SHORT RIGHT TO M.NABERS FOR 12 YARDS",58);
  ppm("/tmp/g1.ppm",fb);
  gameScreen(fb,"nfl_500-dark_scoreboard_dal.png","nfl_500-dark_scoreboard_nyg.png",ledColor(true,0x002A5C),ledColor(true,0x0B2265),17,13,1,2,false,85,10,true,34,"4TH 2:11","1ST&10","PRESCOTT PASS DEEP LEFT TO LAMB FOR 31 YARDS",64);
  ppm("/tmp/g2.ppm",fb);
}
