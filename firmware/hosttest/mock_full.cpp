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
static const char* const BALL[] = {"0011100", "0111110", "1112111", "0111110", "0011100"};
struct G { const char *a,*h; uint32_t ca,ch; int sa,sh; const char *st,*cl; int poss; bool rz; const char* la; const char* lh; };
static G games[]={
 {"ARI","NYG",0x97233F,0x0B2265,17,24,"4TH","2:11",1,true,"nfl/ari.png","nfl/nyg.png"},
 {"DAL","HOU",0x002A5C,0x03202F,21,14,"3RD","8:42",2,false,"nfl/dal.png","nfl/hou.png"},
 {"SEA","LAC",0x002244,0x0080C6,10,10,"HALF","",0,false,0,0},
 {"SF","DEN",0xAA0000,0xFB4F14,27,3,"FINAL","",0,false,0,0}};
static RGB lc(uint32_t c){ return ledColor(true,c); }
static void chipOrLogo(Frame& fb,int x,int y,int sz,const char* file,uint32_t col){
  Logo L; if(file && load(file,sz,sz,L)) { drawLogo(fb,L,x+((sz-L.w)>>1),y+((sz-L.h)>>1),1); return; }
  for(int yy=0;yy<sz;yy++) for(int xx=0;xx<sz;xx++) if((xx+yy)%1==0 && (yy>0&&yy<sz-1||xx>0&&xx<sz-1)) fb.put(x+xx,y+yy,lc(col)); }
int main(){
  static Frame fb; char s[8];
  // A: 4 games, text rows
  fb.clear(); text(fb,2,1,"NFL",GOLD,F3); text(fb,W-2-tw("1/3",F3),1,"1/3",GRAY,F3);
  for(int i=0;i<4;i++){ G& g=games[i]; int y=8+i*14;
    for(int k=0;k<2;k++){ int ry=y+k*6; bool home=k; text(fb,3,ry,home?g.h:g.a,lc(home?g.ch:g.ca),F3);
      snprintf(s,8,"%d",home?g.sh:g.sa); text(fb,30-tw(s,F3),ry,s,WHITE,F3);
      if(g.poss && (g.poss==1)==home) sprite(fb,32,ry,BALL,5,BROWN); }
    bool fin=!strcmp(g.st,"FINAL"); text(fb,W-3-tw(g.st,F3),y,g.st,fin?GREEN:DATEC,F3); if(g.cl[0]) text(fb,W-3-tw(g.cl,F3),y+6,g.cl,CLOCK,F3);
    if(g.rz) for(int yy=y;yy<y+11;yy++){fb.put(0,yy,RED);fb.put(W-1,yy,RED);}
    if(i<3) for(int x=2;x<W-2;x++) fb.put(x,y+12,0x303030); }
  ppm("/tmp/fA.ppm",fb);
  // B: 3 games with little logos
  fb.clear(); text(fb,2,1,"NFL",GOLD,F3); text(fb,W-2-tw("1/4",F3),1,"1/4",GRAY,F3);
  for(int i=0;i<3;i++){ G& g=games[i]; int y=8+i*19;
    for(int k=0;k<2;k++){ int ry=y+k*9; bool home=k; chipOrLogo(fb,1,ry,8,home?g.lh:g.la,home?g.ch:g.ca);
      text(fb,11,ry+1,home?g.h:g.a,lc(home?g.ch:g.ca),F3);
      snprintf(s,8,"%d",home?g.sh:g.sa); text(fb,40-tw(s,F5),ry,s,WHITE,F5);
      if(g.poss && (g.poss==1)==home) sprite(fb,24,ry+1,BALL,5,BROWN); }
    bool fin=!strcmp(g.st,"FINAL"); text(fb,W-2-tw(g.st,F3),y+2,g.st,fin?GREEN:DATEC,F3); if(g.cl[0]) text(fb,W-2-tw(g.cl,F3),y+10,g.cl,CLOCK,F3);
    if(g.rz) for(int yy=y;yy<y+17;yy++){fb.put(0,yy,RED);}
    if(i<2) for(int x=2;x<W-2;x++) fb.put(x,y+18,0x303030); }
  ppm("/tmp/fB.ppm",fb);
  // C: 2 big games per page with logos
  fb.clear();
  for(int i=0;i<2;i++){ G& g=games[i]; int y=i*32;
    chipOrLogo(fb,1,y+2,16,g.la,g.ca); chipOrLogo(fb,47,y+2,16,g.lh,g.ch);
    snprintf(s,8,"%d",g.sa); text(fb,31-tw(s,F5,1)-1,y+6,s,WHITE,F5); text(fb,30,y+6,"-",GRAY,F5); snprintf(s,8,"%d",g.sh); text(fb,35,y+6,s,WHITE,F5);
    char st[16]; snprintf(st,16,"%s %s",g.st,g.cl); text(fb,(W-tw(st,F3))>>1,y+21,st,CLOCK,F3);
    text(fb,9-(tw(g.a,F3)>>1),y+21,"",WHITE,F3);
    if(g.poss){ int bx=g.poss==2?18:39; sprite(fb,bx,y+16,BALL,5,BROWN);} 
    if(g.rz) for(int yy=y;yy<y+30;yy++){fb.put(0,yy,RED);fb.put(W-1,yy,RED);}
    if(i==0) for(int x=2;x<W-2;x++) fb.put(x,31,0x303030); }
  ppm("/tmp/fC.ppm",fb);
}
