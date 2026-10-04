#include "../scoreboard/sb_render.h"
#include "../scoreboard/sb_png.h"
#include <stdio.h>
#include <stdlib.h>
void* sbAlloc(size_t n){return malloc(n);} void sbFree(void*p){free(p);} void sbBreathe(){}
static bool load(const char* name, int bw, int bh, Logo& L){ char path[300]; snprintf(path,300,"/mnt/user-data/uploads/Documents/Scoreboard/scoreboard_logos/%s",name);
 FILE* f=fopen(path,"rb"); if(!f) return false; fseek(f,0,2); long n=ftell(f); fseek(f,0,0); uint8_t* b=(uint8_t*)malloc(n); fread(b,1,n,f); fclose(f);
 int w,h,e; uint8_t* rgba=decodePngRGBA(b,n,w,h,&e); free(b); if(!rgba) return false; bool ok=shrinkLogo(rgba,w,h,bw,bh,L); free(rgba); return ok;}
int main(){
 const char* f[]={"nfl_500-dark_scoreboard_nyg.png","nfl_500-dark_scoreboard_dal.png","nfl_500-dark_scoreboard_ari.png","nfl_500-dark_scoreboard_hou.png","ncaa_500-dark_57.png","ncaa_500-dark_99.png","ncaa_500-dark_142.png","ncaa_500-dark_2377.png","mlb_500-dark_scoreboard_nyy.png","nba_500-dark_scoreboard_ny.png"};
 int sizes[]={10,16,26}; const int N=10, cell=30;
 int WW=N*cell, HH=3*cell; static uint32_t img[300*90];
 for(int si=0;si<3;si++) for(int i=0;i<N;i++){ Logo L; if(!load(f[i],sizes[si],sizes[si],L)) continue; Frame fb; fb.clear(); drawLogo(fb,L,0,0,1);
   for(int y=0;y<sizes[si];y++) for(int x=0;x<sizes[si];x++) img[(si*cell+2+y)*WW+i*cell+2+x]=fb.lit[y*W+x]?fb.px[y*W+x]:0; }
 FILE* o=fopen("/tmp/sizes.ppm","wb"); int S=6; fprintf(o,"P6 %d %d 255\n",WW*S,HH*S);
 for(int y=0;y<HH*S;y++) for(int x=0;x<WW*S;x++){ uint32_t c=img[(y/S)*WW+x/S]; bool d=(x%S)&&(y%S)&&(x%S<5)&&(y%S<5); unsigned char p[3]={(unsigned char)(c>>16),(unsigned char)(c>>8),(unsigned char)c}; if(!d)p[0]=p[1]=p[2]=0; else if(!c)p[0]=p[1]=p[2]=18; fwrite(p,1,3,o);} fclose(o);
}
