#include "../scoreboard/sb_render.h"
#include "../scoreboard/sb_png.h"
#include <stdio.h>
#include <stdlib.h>
void* sbAlloc(size_t n){return malloc(n);} void sbFree(void*p){free(p);} void sbBreathe(){}
int main(int argc,char**argv){ int N=argc-2; int cols=8, rows=(N+cols-1)/cols, cw=30, chh=28, S=6;
 static uint32_t img[8*30*20*28]; int WW=cols*cw, HH=rows*chh;
 for(int i=0;i<N;i++){ FILE* f=fopen(argv[i+2],"rb"); if(!f) continue; fseek(f,0,2); long n=ftell(f); fseek(f,0,0); uint8_t* b=(uint8_t*)malloc(n); fread(b,1,n,f); fclose(f);
  int w,h,e; uint8_t* rgba=decodePngRGBA(b,n,w,h,&e); free(b); if(!rgba) continue; Logo L; shrinkLogo(rgba,w,h,26,24,L); free(rgba);
  int ox=(i%cols)*cw+2+((26-L.w)>>1), oy=(i/cols)*chh+2+((24-L.h)>>1);
  for(int k=0;k<L.n;k++) img[(oy+L.pix[k].y)*WW+ox+L.pix[k].x]=(L.pix[k].r<<16)|(L.pix[k].g<<8)|L.pix[k].b|0x1000000; }
 FILE* o=fopen(argv[1],"wb"); fprintf(o,"P6 %d %d 255\n",WW*S,HH*S);
 for(int y=0;y<HH*S;y++) for(int x=0;x<WW*S;x++){ uint32_t c=img[(y/S)*WW+x/S]; bool d=(x%S)&&(y%S)&&(x%S<5)&&(y%S<5); unsigned char p[3]={(unsigned char)(c>>16),(unsigned char)(c>>8),(unsigned char)c}; if(!d)p[0]=p[1]=p[2]=0; else if(!(c&0x1000000))p[0]=p[1]=p[2]=18; else {p[0]=(c>>16)&255;} fwrite(p,1,3,o);} fclose(o);
}
