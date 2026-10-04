// Mockup only: current baseball status line vs proposed diamond
#include "preview_json.h"
#include <fstream>
void* sbAlloc(size_t n){return malloc(n);} void sbFree(void*p){free(p);} void sbBreathe(){}
static void ppm(const char* path, const Frame& fb){ FILE* f=fopen(path,"wb"); fprintf(f,"P6 %d %d 255\n",W*10,H*10);
 for(int y=0;y<H*10;y++)for(int x=0;x<W*10;x++){RGB c=fb.px[(y/10)*W+x/10];bool d=(x%10)&&(y%10)&&(x%10<9)&&(y%10<9);
 unsigned char p[3]={(unsigned char)(d?c>>16:0),(unsigned char)(d?c>>8:0),(unsigned char)(d?c:0)}; if(d&&!c)p[0]=p[1]=p[2]=22; fwrite(p,1,3,f);} fclose(f);}
// proposed: no grass/dirt; runner = solid bright 3x3 diamond (gold), empty = hollow dim outline; home plate dim
static void base(Frame& fb,int cx,int cy,bool on){
  RGB c = on ? rgb(255,200,0) : rgb(70,70,70);
  fb.put(cx,cy-1,c); fb.put(cx-1,cy,c); fb.put(cx+1,cy,c); fb.put(cx,cy+1,c);
  if(on) fb.put(cx,cy,rgb(255,255,255));
}
static void newDiamond(Frame& fb,int x,int y,const bool* b){
  // clear old area (rows 0..8, cols 29..41) except the frame
  for(int yy=1;yy<=8;yy++) for(int xx=29;xx<=41;xx++){ fb.px[yy*W+xx]=0; fb.lit[yy*W+xx]=false; }
  base(fb,x+5,y+1,b[1]); base(fb,x+1,y+4,b[2]); base(fb,x+9,y+4,b[0]);
  fb.put(x+5,y+6,rgb(90,90,90));
}
int main(){
  std::ifstream in("/tmp/js_frames.json"); JsonDocument d; deserializeJson(d,in);
  static Game g; static Frame a,b;
  for (int k=0;k<2;k++){
    fromPreview(d["baseball@0"]["game"],g);
    g.po.on=true; scopy(g.po.round,"ALDS GAME 2");
    g.bases[0]=k==0; g.bases[1]=true; g.bases[2]=k==1;
    renderGame(a,g,0,nullptr,nullptr);
    b=a; newDiamond(b,30,1,g.bases);
    char n[64]; snprintf(n,64,"/tmp/dia_old_%d.ppm",k); ppm(n,a); snprintf(n,64,"/tmp/dia_new_%d.ppm",k); ppm(n,b);
  }
}
