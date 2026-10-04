// Hand-drawn 26x24 matchup logos for the teams whose real logo doesn't
// survive the shrink, plus the auto "badge" for others. Prints C arrays and
// a preview image.
#include "../scoreboard/sb_gfx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
void* sbAlloc(size_t n){return malloc(n);} void sbFree(void*p){free(p);} void sbBreathe(){}
static Frame fb;
static void rect(int x0,int y0,int x1,int y1,RGB c){ for(int y=y0;y<=y1;y++) for(int x=x0;x<=x1;x++) fb.put(x,y,c);}
static void disc(double cx,double cy,double r,RGB c){ for(int y=0;y<24;y++) for(int x=0;x<26;x++){ double dx=x+.5-cx,dy=y+.5-cy; if(dx*dx+dy*dy<=r*r) fb.put(x,y,c);} }
static const RGB RB=rgb(0,70,200), RR=rgb(220,20,45), WH=rgb(235,235,235), OR=rgb(245,120,30), KB=rgb(0,90,220), DK=rgb(90,40,10);
static void outlineFill(bool (*in)(int,int), RGB fill, RGB edge){
  for(int y=0;y<24;y++) for(int x=0;x<26;x++) if(in(x,y)){
    bool e=!in(x-1,y)||!in(x+1,y)||!in(x,y-1)||!in(x,y+1); fb.put(x,y,e?edge:fill);} }
static bool shield(int x,int y){ if(y<0||y>23) return false; int in= y<14?0:(y-13)*6/5; return x>=1+in && x<=24-in; }
static void rangers(){
  fb.clear(); outlineFill(shield,WH,RB);
  rect(1,0,24,8,RB);
  text(fb,(26-tw("NY",F5))/2,1,"NY",WH,F5);
  for(int y=9;y<24;y++) for(int x=1;x<25;x++){ double d=(x-1)-(y-9)*1.6; if(d>=3 && d<=9 && shield(x,y) && fb.px[y*W+x]==WH) fb.put(x,y,RR);} 
}
static bool banner(int x,int y){ if(y<0||y>9) return false; int in=y/2; return x>=in && x<=25-in; }
static void knicks(){
  fb.clear();
  disc(13,16,7.6,OR);
  for(int y=8;y<24;y++) for(int x=4;x<23;x++){ double dx=x+.5-13,dy=y+.5-16; double r=sqrt(dx*dx+dy*dy); if(r<=7.2 && (fabs(dx)<0.55 || fabs(dy)<0.55)) fb.put(x,y,rgb(150,55,0));} 
  for(int y=0;y<10;y++) for(int x=0;x<26;x++) if(banner(x,y)) fb.put(x,y,KB);
  text(fb,(26-tw("NYK",F5))/2,1,"NYK",WH,F5);
}
static bool arrow(int x,int y){ if(y<0||y>23||x<0||x>25) return false; double t=fabs(y-11.5);
  int x1 = y<=11 ? 13+ (int)((11.5-t)*1.0) : 13+(int)((11.5-t)*1.0); x1=13+(int)(11.5-t);
  if(x>x1) return false; if(x<1) return false; if(x<4 && y>=9 && y<=14) return false; return true; }
static void chiefs(){
  fb.clear(); outlineFill(arrow,WH,RR);
  text(fb,5,8,"KC",RR,F5);
}
static void badge(const char* ab, RGB c1, RGB c2){
  fb.clear(); c1 = ((c1>>16)+((c1>>8)&255)+(c1&255))<90 ? rgb(60,60,66) : ledColor(true,c1); c2=ledColor(true,c2);
  for(int y=1;y<23;y++) for(int x=1;x<25;x++){ bool corner=(x<3&&y<3)||(x>22&&y<3)||(x<3&&y>20)||(x>22&&y>20); if(corner) continue; bool edge=x==1||x==24||y==1||y==22|| (x==2&&(y==2||y==21))||(x==23&&(y==2||y==21)); fb.put(x,y, edge?c2:c1);} 
  int w=tw(ab,F3,2); if(w>22) { text(fb,(26-tw(ab,F3))/2,10,ab,WH,F3); } else text(fb,(26-w)/2,7,ab,WH,F3,2);
}
static void dump(const char* name, FILE* img, int slot){
  printf("static const LogoPix ART_%s[] = {", name); int n=0;
  for(int y=0;y<24;y++) for(int x=0;x<26;x++) if(fb.lit[y*W+x]){ RGB c=fb.px[y*W+x]; if(!c) continue; printf("%s{%d,%d,%d,%d,%d}", n?",":"", x,y,(int)(c>>16),(int)((c>>8)&255),(int)(c&255)); n++; }
  printf("};\n");
}
int main(){
  static RGB sheet[4][26*24];
  void (*fns[])()={rangers,knicks,chiefs};
  const char* names[]={"NYR","NYK","KC"};
  FILE* f=fopen("/tmp/art.ppm","wb"); int S=8, cols=5; fprintf(f,"P6 %d %d 255\n",cols*28*S,26*S);
  static RGB all[5][26*24];
  for(int i=0;i<3;i++){ fns[i](); dump(names[i],0,i); for(int k=0;k<26*24;k++){ int x=k%26,y=k/26; all[i][k]=fb.lit[y*W+x]?fb.px[y*W+x]:0xFFFFFFFF; } }
  badge("BKN",0x000000,0xFFFFFF); for(int k=0;k<26*24;k++){int x=k%26,y=k/26; all[3][k]=fb.lit[y*W+x]?fb.px[y*W+x]:0xFFFFFFFF;}
  badge("LAL",0x552583,0xFDB927); for(int k=0;k<26*24;k++){int x=k%26,y=k/26; all[4][k]=fb.lit[y*W+x]?fb.px[y*W+x]:0xFFFFFFFF;}
  for(int y=0;y<26*S;y++) for(int x=0;x<cols*28*S;x++){ int t=x/(28*S), lx=(x%(28*S))/S-1, ly=y/S-1; RGB c=0xFFFFFFFF; if(lx>=0&&lx<26&&ly>=0&&ly<24) c=all[t][ly*26+lx];
    bool d=(x%S)&&(y%S)&&(x%S<S-1)&&(y%S<S-1); unsigned char p[3]={0,0,0}; if(d){ if(c==0xFFFFFFFF){p[0]=p[1]=p[2]=20;} else {p[0]=c>>16;p[1]=(c>>8)&255;p[2]=c&255;} } fwrite(p,1,3,f);} fclose(f);
}
