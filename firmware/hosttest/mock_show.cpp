#include "../scoreboard/sb_events.h"
#include "../scoreboard/sb_render.h"
#include "../scoreboard/sb_png.h"
#include <stdio.h>
#include <stdlib.h>
void* sbAlloc(size_t n){return malloc(n);} void sbFree(void*p){free(p);} void sbBreathe(){}
static void ppm(const char* path, const Frame& fb){ FILE* f=fopen(path,"wb"); fprintf(f,"P6 %d %d 255\n",W*6,H*6);
 for(int y=0;y<H*6;y++)for(int x=0;x<W*6;x++){RGB c=fb.px[(y/6)*W+x/6];bool d=(x%6)&&(y%6)&&(x%6<5)&&(y%6<5);
 unsigned char p[3]={(unsigned char)(c>>16),(unsigned char)(c>>8),(unsigned char)c}; if(!d)p[0]=p[1]=p[2]=0; else if(!fb.lit[(y/6)*W+x/6])p[0]=p[1]=p[2]=18; fwrite(p,1,3,f);} fclose(f);}
static bool load(const char* name, int bw, int bh, Logo& L){ char path[300]; snprintf(path,300,"/mnt/user-data/uploads/Documents/Scoreboard/scoreboard_logos/%s",name);
 FILE* f=fopen(path,"rb"); if(!f) return false; fseek(f,0,2); long n=ftell(f); fseek(f,0,0); uint8_t* b=(uint8_t*)malloc(n); fread(b,1,n,f); fclose(f);
 int w,h,e; uint8_t* rgba=decodePngRGBA(b,n,w,h,&e); free(b); if(!rgba) return false; bool ok=shrinkLogo(rgba,w,h,bw,bh,L); free(rgba); return ok;}
static void fill(FxSpec& f, const Logo& big, const Logo& a, const Logo& h){
  f.fixPointers(); memcpy(f.logoPix,big.pix,big.n*sizeof(LogoPix)); f.logo.w=big.w; f.logo.h=big.h; f.logo.n=big.n;
  memcpy(f.awayPix,a.pix,a.n*sizeof(LogoPix)); f.awayLogo.w=a.w; f.awayLogo.h=a.h; f.awayLogo.n=a.n;
  memcpy(f.homePix,h.pix,h.n*sizeof(LogoPix)); f.homeLogo.w=h.w; f.homeLogo.h=h.h; f.homeLogo.n=h.n; }
int main(){
  Logo big,a,h; load("nfl_500-dark_scoreboard_nyg.png",54,54,big); load("nfl_500-dark_scoreboard_dal.png",26,24,a); load("nfl_500-dark_scoreboard_nyg.png",26,24,h);
  static FxSpec f; static Frame fb; FxPlayer p;
  const char* names[]={"fieldgoal","kickoff","firstdown","run","win","touchdown"}; uint32_t at[]={4000,6500,4000,4000,1000,7000};
  for(int i=0;i<6;i++){ testEvent(names[i],nullptr,f); if(f.kind==FX_WIN) scopy(f.winText,"GIANTS WIN"); fill(f,big,a,h);
    p.start(&f,1); bool ok=true; for(uint32_t t=0;t<=at[i];t+=16) ok=p.show(fb,t); char path[64]; snprintf(path,64,"/tmp/s%d.ppm",i); ppm(path,fb);
    printf("%s total %d active %d\n",names[i],p.showMs(),ok); }
}
