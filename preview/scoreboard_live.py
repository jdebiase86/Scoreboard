#!/usr/bin/env python3
"""
Opens the scoreboard in your browser, with live scores.

    python3 scoreboard_live.py

That's it - your browser opens by itself. Leave it running; the board
refreshes on its own. Press Control-C in Terminal when you're done.

Why this exists as well as the web page: a browser isn't allowed to call
ESPN directly from a published page, but this little server runs on your
own Mac, so it fetches the scores and hands them to the page. Same board,
same pixels, real data.

Needs scoreboard_sim.py sitting in the same folder - that's where the
ESPN fetching and parsing lives.
"""
import json
import os
import sys
import signal
import subprocess
import threading
import time
import urllib.request
import webbrowser
from http.server import BaseHTTPRequestHandler, HTTPServer
from urllib.parse import urlparse, parse_qs

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
try:
    import scoreboard_sim as SB
except ImportError:
    print("Couldn't find scoreboard_sim.py next to this file.")
    print("Both files need to be in the same folder (your Documents folder).")
    sys.exit(1)

PORT = 8733
VERSION = "v27"

PAGE = r"""<!doctype html>
<html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Gameday Panel</title>
<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Oswald:wght@400;500;600&family=IBM+Plex+Mono:wght@400;500&family=IBM+Plex+Sans:wght@400;500;600&display=swap">
<style>
  :root {
    color-scheme: light;
    --bg:#f0eeea; --surface:#fff; --border:#d9d4cb; --ink:#16181c;
    --ink-soft:#5d6067; --ink-faint:#8b8e95; --accent:#c06a12;
    --accent-soft:#f4e3cd; --chassis:#0a0b0d; --chassis-edge:#26292f;
    --good:#1f7a3d; --bad:#a3301b;
  }
  @media (prefers-color-scheme: dark) {
    :root {
      color-scheme: dark;
      --bg:#101216; --surface:#191c22; --border:#2b2f37; --ink:#e9ebef;
      --ink-soft:#a3a7b0; --ink-faint:#767b85; --accent:#e8a33d;
      --accent-soft:#332614; --chassis:#050607; --chassis-edge:#1d2026;
      --good:#4ecb75; --bad:#e2765c;
    }
  }
  * { box-sizing: border-box; }
  body {
    margin:0; background:var(--bg); color:var(--ink);
    font-family:"IBM Plex Sans",system-ui,-apple-system,sans-serif;
  }
  .wrap {
    max-width:640px; margin:0 auto;
    padding-left:16px; padding-right:16px;
    padding-block:28px 44px;
    display:flex; flex-direction:column; gap:20px;
  }
  h1 {
    font-family:Oswald,"Arial Narrow",sans-serif; font-weight:600;
    font-size:clamp(27px,7vw,36px); letter-spacing:.012em;
    line-height:1.04; margin:0;
  }
  .sub { color:var(--ink-soft); font-size:14px; line-height:1.5; margin:4px 0 0; }
  .chassis {
    background:var(--chassis); border:1px solid var(--chassis-edge);
    border-radius:10px; padding:14px; display:flex; justify-content:center;
  }
  canvas { width:100%; height:auto; display:block; image-rendering:pixelated; }
  .group { display:flex; flex-direction:column; gap:7px; }
  .grouplabel {
    font-family:"IBM Plex Mono",ui-monospace,monospace; font-size:10.5px;
    letter-spacing:.1em; text-transform:uppercase; color:var(--ink-faint);
  }
  .chips { display:flex; flex-wrap:wrap; gap:7px; }
  button.chip {
    font:500 13px/1.2 "IBM Plex Sans",system-ui,sans-serif;
    color:var(--ink-soft); background:var(--surface);
    border:1px solid var(--border); border-radius:6px;
    padding:8px 13px; cursor:pointer;
  }
  button.chip:hover { border-color:var(--ink-faint); color:var(--ink); }
  button.chip[aria-pressed="true"] {
    background:var(--accent-soft); border-color:var(--accent);
    color:var(--accent); font-weight:600;
  }
  button.chip:focus-visible { outline:2px solid var(--accent); outline-offset:2px; }
  .status {
    font-family:"IBM Plex Mono",ui-monospace,monospace; font-size:12px;
    line-height:1.6; color:var(--ink-faint); display:flex;
    align-items:center; gap:8px; min-height:20px;
  }
  .dot { width:8px; height:8px; border-radius:50%; background:var(--good); flex:none; }
  .dot.bad { background:var(--bad); }
  .status b { color:var(--ink-soft); font-weight:500; }
  .note {
    background:var(--surface); border:1px solid var(--border);
    border-radius:8px; padding:13px 15px; font-size:13.5px;
    line-height:1.5; color:var(--ink-soft);
  }
  .spec {
    font-family:"IBM Plex Mono",ui-monospace,monospace; font-size:12px;
    line-height:1.7; color:var(--ink-faint);
    border-top:1px solid var(--border); padding-top:14px;
  }
  .spec b { color:var(--ink-soft); font-weight:500; }
  .spec:empty { display:none; }
</style></head><body>
<div class="wrap">
  <div>
    <h1>Gameday Panel</h1>
    <p class="sub">Live from ESPN, drawn at 64&times;64 &mdash; exactly what the LEDs will show.</p>
  </div>

  <div class="chassis">
    <canvas id="panel" width="640" height="640"></canvas>
  </div>

  <div class="status"><span class="dot" id="dot"></span><span id="statusText">Loading&hellip;</span></div>

  <div class="group">
    <div class="grouplabel">Board</div>
    <div class="chips" id="chips"></div>
  </div>

  <div class="group">
    <div class="grouplabel">Layout preview (sample games, for when nothing's live)</div>
    <div class="chips" id="demoChips"></div>
  </div>

  <div class="group">
    <div class="grouplabel">Football</div>
    <div class="chips">
      <button class="chip" id="fxBtn" type="button">Touchdown</button>
      <button class="chip" id="fgBtn" type="button">Field goal</button>
      <button class="chip" id="kickBtn" type="button">Kickoff</button>
      <button class="chip" id="qtrBtn" type="button">End of quarter</button>
      <button class="chip" id="halfBtn" type="button">Halftime</button>
      <button class="chip" id="flagBtn" type="button">Flag</button>
      <button class="chip" id="firstDownBtn" type="button">1st down</button>
    </div>
  </div>

  <div class="group">
    <div class="grouplabel">Hockey, baseball &amp; basketball</div>
    <div class="chips">
      <button class="chip" id="goalBtn" type="button">Goal light</button>
      <button class="chip" id="intBtn" type="button">Intermission</button>
      <button class="chip" id="homerunBtn" type="button">Home run</button>
      <button class="chip" id="runBtn" type="button">Run scores</button>
      <button class="chip" id="threeBtn" type="button">3-pointer</button>
    </div>
  </div>

  <div class="note" id="note" style="display:none"></div>
  <div class="spec" id="diag"></div>
  <div class="spec" id="phone"></div>
</div>

<script>
const F5={"0":["01110","10001","10011","10101","11001","10001","01110"],
"1":["00100","01100","00100","00100","00100","00100","01110"],
"2":["01110","10001","00001","00010","00100","01000","11111"],
"3":["11111","00010","00100","00010","00001","10001","01110"],
"4":["00010","00110","01010","10010","11111","00010","00010"],
"5":["11111","10000","11110","00001","00001","10001","01110"],
"6":["00110","01000","10000","11110","10001","10001","01110"],
"7":["11111","00001","00010","00100","01000","01000","01000"],
"8":["01110","10001","10001","01110","10001","10001","01110"],
"9":["01110","10001","10001","01111","00001","00010","01100"],
"A":["01110","10001","10001","11111","10001","10001","10001"],
"B":["11110","10001","10001","11110","10001","10001","11110"],
"C":["01110","10001","10000","10000","10000","10001","01110"],
"D":["11100","10010","10001","10001","10001","10010","11100"],
"E":["11111","10000","10000","11110","10000","10000","11111"],
"F":["11111","10000","10000","11110","10000","10000","10000"],
"G":["01110","10001","10000","10111","10001","10001","01111"],
"H":["10001","10001","10001","11111","10001","10001","10001"],
"I":["01110","00100","00100","00100","00100","00100","01110"],
"J":["00111","00010","00010","00010","00010","10010","01100"],
"K":["10001","10010","10100","11000","10100","10010","10001"],
"L":["10000","10000","10000","10000","10000","10000","11111"],
"M":["10001","11011","10101","10101","10001","10001","10001"],
"N":["10001","10001","11001","10101","10011","10001","10001"],
"O":["01110","10001","10001","10001","10001","10001","01110"],
"P":["11110","10001","10001","11110","10000","10000","10000"],
"Q":["01110","10001","10001","10001","10101","10010","01101"],
"R":["11110","10001","10001","11110","10100","10010","10001"],
"S":["01111","10000","10000","01110","00001","00001","11110"],
"T":["11111","00100","00100","00100","00100","00100","00100"],
"U":["10001","10001","10001","10001","10001","10001","01110"],
"V":["10001","10001","10001","10001","10001","01010","00100"],
"W":["10001","10001","10001","10101","10101","11011","10001"],
"X":["10001","10001","01010","00100","01010","10001","10001"],
"Y":["10001","10001","01010","00100","00100","00100","00100"],
"Z":["11111","00001","00010","00100","01000","10000","11111"],
" ":["00000","00000","00000","00000","00000","00000","00000"],
"/":["00001","00010","00010","00100","01000","01000","10000"],
"-":["00000","00000","00000","11111","00000","00000","00000"],
"&":["01100","10010","10100","01000","10101","10010","01101"],
".":["00000","00000","00000","00000","00000","00100","00100"],
":":["00000","00100","00100","00000","00100","00100","00000"]};
const F3={"0":["111","101","101","101","111"],"1":["010","110","010","010","111"],
"2":["111","001","111","100","111"],"3":["111","001","111","001","111"],
"4":["101","101","111","001","001"],"5":["111","100","111","001","111"],
"6":["111","100","111","101","111"],"7":["111","001","001","001","001"],
"8":["111","101","111","101","111"],"9":["111","101","111","001","111"],
"A":["111","101","111","101","101"],"B":["110","101","110","101","110"],
"C":["111","100","100","100","111"],"D":["110","101","101","101","110"],
"E":["111","100","111","100","111"],"F":["111","100","111","100","100"],
"G":["111","100","101","101","111"],"H":["101","101","111","101","101"],
"I":["111","010","010","010","111"],"J":["001","001","001","101","111"],
"K":["101","101","110","101","101"],"L":["100","100","100","100","111"],
"M":["101","111","111","101","101"],"N":["101","111","111","111","101"],
"O":["111","101","101","101","111"],"P":["111","101","111","100","100"],
"Q":["111","101","101","111","001"],"R":["111","101","111","110","101"],
"S":["111","100","111","001","111"],"T":["111","010","010","010","010"],
"U":["101","101","101","101","111"],"V":["101","101","101","101","010"],
"W":["101","101","111","111","101"],"X":["101","101","010","101","101"],
"Y":["101","101","010","010","010"],"Z":["111","001","010","100","111"],
" ":["000","000","000","000","000"],"/":["001","001","010","100","100"],
"-":["000","000","111","000","000"],":":["000","010","000","010","000"],
"&":["010","101","010","101","011"],"#":["101","111","101","111","101"],
".":["000","000","000","000","010"],"'":["010","010","000","000","000"],
"+":["000","010","111","010","000"]};
const BALL=["0011100","0111110","1112111","0111110","0011100"];

const W=64,H=64;
const WHITE="rgb(255,255,255)",DIM="rgb(120,120,120)",GRAY="rgb(90,90,90)",
      GOLD="rgb(255,190,0)",RED="rgb(255,40,40)",GREEN="rgb(0,230,80)",
      BROWN="rgb(190,95,30)",LINE="rgb(105,105,105)",OFF="rgb(26,26,26)",
      CLOCK="rgb(235,235,235)",   // the time itself - reads first
      DATEC="rgb(140,140,140)";   // the date / period label behind it

function hexRgb(h){
  h=(h||"").replace("#","");
  if(h.length!==6) return null;
  const n=parseInt(h,16);
  if(isNaN(n)) return null;
  return [(n>>16)&255,(n>>8)&255,n&255];
}
// Navy and forest green read as black on a panel. Lift them until they light.
function ledColor(hex){
  const c=hexRgb(hex);
  if(!c) return WHITE;
  const m=Math.max(c[0],c[1],c[2]);
  if(m===0) return WHITE;
  if(m<140){const f=190/m;
    return `rgb(${Math.min(255,c[0]*f|0)},${Math.min(255,c[1]*f|0)},${Math.min(255,c[2]*f|0)})`;}
  return `rgb(${c[0]},${c[1]},${c[2]})`;
}

let px=new Array(W*H).fill(null);
const put=(x,y,c)=>{if(x>=0&&x<W&&y>=0&&y<H)px[y*W+x]=c;};
function tw(s,f,sc){sc=sc||1;if(!s)return 0;const gw=f===F5?5:3;
  return String(s).length*(gw*sc+1)-1;}
function text(x,y,s,color,f,sc){sc=sc||1;const gw=f===F5?5:3;
  for(const ch of String(s).toUpperCase()){const g=f[ch];
    if(g){for(let ry=0;ry<g.length;ry++)for(let rx=0;rx<g[ry].length;rx++)
      if(g[ry][rx]==="1")for(let sy=0;sy<sc;sy++)for(let sx=0;sx<sc;sx++)
        put(x+rx*sc+sx,y+ry*sc+sy,color);}
    x+=gw*sc+1;}}
function sprite(x,y,rows,c1,c2){for(let ry=0;ry<rows.length;ry++)
  for(let rx=0;rx<rows[ry].length;rx++){const b=rows[ry][rx];
    if(b!=="0")put(x+rx,y+ry,b==="1"?c1:(c2||WHITE));}}

const LAY={status:1,row1:7,row2:22,divider:37,tickA:39,tickDiv:51,tickB:53};

// Abbreviations keep all their letters now - TENN stays TENN. Three
// letters use the big 5x7 font doubled. A four-letter one won't fit beside
// a score that way, so any game with one switches BOTH rows to the
// narrower 3x5 font doubled: same weight, all four letters, and the two
// rows still match each other instead of one looking shrunken.
// (Quarter-break card and kickoff still use shortAbbr - they have room
// for three and nothing else to fit beside them.)
function shortAbbr(a){ return (a||"").slice(0,3); }
function fullAbbr(a){ return (a||"").slice(0,4); }

// 1 when the playoff frame is up, so names and scores don't touch the gold
let ROW_INSET=0;
function teamRow(y,abbr,score,color,hasBall,record,rank,condensed){
  abbr = fullAbbr(abbr);
  const rowH=14;
  const af = condensed ? {f:F3,h:10} : {f:F5,h:14};
  const ax=1+ROW_INSET, aw=tw(abbr,af.f,2), RX=W-1-ROW_INSET;
  text(ax,y+((rowH-af.h)>>1),abbr,color,af.f,2);

  // Before kickoff the slot carries the team's record, not a dash
  const pre=(score===null||score===undefined);
  let txt,f,sc,h;
  if(pre){ txt=record?String(record):""; f=F5; sc=1; h=7; }
  else {
    txt=String(score); f=F5; sc=2; h=14;
    // a three-digit basketball score won't fit in the big font beside the
    // abbreviation - same height class, narrower font
    if(ax+aw+2+tw(txt,F5,2) > RX){ f=F3; h=10; }
  }
  // AP / CFP rank: small soft-white number tucked low after the abbreviation,
  // so it stays clear of the football (which sits mid-row) and stays up
  // for the whole game rather than vanishing at kickoff
  const rs = rank ? String(rank) : "";
  const rx = ax+aw+2, rEnd = rs ? rx+tw(rs,F3)+2 : rx;
  if(pre && txt && RX-tw(txt,f,sc) < rEnd){ f=F3; h=5; }   // 10-2 beside #12
  const sw=tw(txt,f,sc);
  const sx=RX-sw;
  if(txt){ if(pre) text(sx,y+((rowH-h)>>1),txt,WHITE,f,sc); else scoreText(sx,y+((rowH-h)>>1),txt,WHITE,f,sc); }
  if(rs) text(rx,y+rowH-5,rs,RANKC,F3);
  // hasBall: true/"ball" = football possession, "bat" = baseball team at bat (drawn as a baseball)
  if(hasBall==="bat") spriteMap(sx-12,y+((rowH-9)>>1),AT_BAT,AT_BAT_COLORS);
  else if(hasBall) sprite(sx-9,y+((rowH-5)>>1),BALL,BROWN);
}

// A solid status chip (PP / PK / BONUS): lit block, letters cut out of it,
// right edge at xr. Reads as a badge rather than more text on the line.
function chip(xr,label,c){
  const w=tw(label,F3)+2, xl=xr-w+1;
  for(let y=LAY.status-1;y<=LAY.status+5;y++)for(let x=xl;x<=xr;x++)put(x,y,c);
  text(xl+1,LAY.status,label,"rgb(0,0,0)",F3);
}

// AP / CFP rank colour: a soft white - reads as secondary next to the
// pure-white score, and doesn't fight the team colours the way gold did
const RANKC="rgb(190,190,190)";

// Baseball's situation lives on the top line, read left to right the way
// a TV score bug does:  ▼7  2-1  ◇  1 OUT
// The diamond is three bases (2nd top, 3rd left, 1st right) with home
// plate under them. A runner is a solid gold base with a white centre; an
// empty base is a faint grey outline - readable at a glance (Joe,
// 2026-10-03: the old grass-and-dirt diamond was hard to read, and its top
// row was hidden under the playoff frame, so it now starts a row lower).
const ARROW_UP=["00100","01110","11111"], ARROW_DN=["11111","01110","00100"];
function drawBaseDiamond(x,y,bases){
  const base=(cx,cy,on)=>{
    const c=on?"rgb(255,200,0)":"rgb(70,70,70)";
    put(cx,cy-1,c);put(cx-1,cy,c);put(cx+1,cy,c);put(cx,cy+1,c);
    if(on) put(cx,cy,"rgb(255,255,255)");
  };
  base(x+5,y+1,bases&&bases[1]);   // 2nd
  base(x+1,y+4,bases&&bases[2]);   // 3rd
  base(x+9,y+4,bases&&bases[0]);   // 1st
  put(x+5,y+6,"rgb(90,90,90)");    // home plate
}

// The team at bat gets a baseball beside its score - the same spot
// and the same idea as football's possession ball: the ball marks the
// team on offence. (A bat icon was tried and read as an arrow at this
// size; Joe picked the ball, 2026-09-30.)
const AT_BAT=["000333000",
              "003111300",
              "042111240",
              "312111213",
              "342111243",
              "312111213",
              "042111240",
              "003111300",
              "000333000"];
// 9x9: off-white ball, a soft grey edge so it reads round, red seams
// bowing in from each side the way a real ball's do, darker stitch marks
const AT_BAT_COLORS={"1":"rgb(250,250,245)","2":"rgb(220,30,30)",
                     "3":"rgb(150,150,150)","4":"rgb(150,20,20)"};
function spriteMap(x,y,rows,map){
  for(let ry=0;ry<rows.length;ry++)for(let rx=0;rx<rows[ry].length;rx++){
    const k=rows[ry][rx]; if(k!=="0") put(x+rx,y+ry,map[k]);
  }
}
function drawInning(x,y,half,num,color){
  // arrow up = top of the inning, arrow down = bottom
  sprite(x,y+1,half==="TOP"?ARROW_UP:ARROW_DN,color);
  text(x+6,y,String(num),color,F3);
  return x+6+tw(String(num),F3);
}

// "1st & Goal" spelled out collides with the clock on the left, so
// Goal becomes G and anything still too long gets trimmed to fit.
function shortDown(dd,leftW){
  if(!dd) return "";
  let s=dd.split(" at ")[0].toUpperCase().replace(/ /g,"").replace("GOAL","G");
  // keep a clear gap after the clock: "1ST&10", then "1&10", else nothing
  const fits=t=>leftW+tw(t,F3)+2+4+2<=W;
  if(fits(s)) return s;
  const c=s.replace(/(\d)[A-Z]{2}/,"$1");
  return fits(c)?c:"";
}
// score digits: a chunkier, stadium-style 1 (the font's 1 looks thin)
const BOLD_ONE=["00110","01110","00110","00110","00110","00110","01111"];
function scoreText(x,y,s,color,f,sc){sc=sc||1;
  if(f!==F5){text(x,y,s,color,f,sc);return;}
  for(const ch of String(s)){
    if(ch==="1"){for(let ry=0;ry<7;ry++)for(let rx=0;rx<5;rx++)if(BOLD_ONE[ry][rx]==="1")
      for(let sy=0;sy<sc;sy++)for(let sx=0;sx<sc;sx++)put(x+rx*sc+sx,y+ry*sc+sy,color);}
    else text(x,y,ch,color,f,sc);
    x+=5*sc+1;}}

// Postseason gold: the frame round your game, and the line under it
const PLAYOFF_GOLD="rgb(230,170,0)";
// one of several status-line messages, changing every 4s with the ticker
function cycle(lines,pair){ const l=lines.filter(Boolean); return l.length?l[pair%l.length]:""; }
// Preseason: a dotted silver frame - "these don't count" - instead of gold
const PRESEASON_SILVER="rgb(165,170,185)";

// Pregame matchup: small logos (22px box) for both teams
// 26 wide x 24 tall: the most that fits two logos, "AT" between them and
// the records underneath (22 was too small for detailed logos like the
// Gators' and the Knicks')
const MATCHUP_W=26, MATCHUP_H=24;
const matchupKey=url=>url+"@"+MATCHUP_W+"x"+MATCHUP_H;
function matchupLogo(side){ return side&&side.logo ? (logoCache[matchupKey(side.logo)]||null) : null; }
function loadMatchupLogos(g){
  if(!g || g.state!=="pre") return;
  for(const side of [g.home,g.away])
    if(side && side.logo && !(matchupKey(side.logo) in logoCache))
      loadLogo(side.logo, ()=>{ if(!fx) draw(); }, MATCHUP_W, MATCHUP_H);
}
function drawMatchup(la, lh, away, home, recA, recH){
  const place=(lg,cx)=>drawLogo(lg, cx-(lg.w>>1), 7+((MATCHUP_H-lg.h)>>1), 1);
  place(la, 14); place(lh, 50);
  text((W-tw("AT",F3))>>1, 17, "AT", DATEC, F3);
  // under each logo: rank (college) and record, centred
  const under=(side,rec,cx)=>{
    const rk=side.rank?String(side.rank):"", r=rec?String(rec):"";
    const wr=tw(r,F3), wk=rk?tw(rk,F3)+3:0, x0=cx-((wk+wr)>>1);
    if(rk) text(x0,32,rk,RANKC,F3);
    if(r) text(x0+wk,32,r,WHITE,F3);
  };
  under(away,recA,14); under(home,recH,50);
}

function drawMain(g,pair){
  pair=pair||0;
  const sport=g.sport||"football";
  const po=g.playoff;
  ROW_INSET=(po||g.preseason)?1:0;
  const pinnedHome=g.pinned_side==="home";
  const top=pinnedHome?g.home:g.away, bot=pinnedHome?g.away:g.home;
  let topBall=sport==="football"&&g.possession!=null&&((g.possession==="home")===pinnedHome);
  let botBall=sport==="football"&&g.possession!=null&&!topBall;
  // baseball: the ball goes beside whoever's hitting - away team in the
  // top of the inning, home team in the bottom. The other team is fielding.
  if(sport==="baseball" && g.state==="in" && (g.half==="TOP"||g.half==="BOT")){
    const homeBats=g.half==="BOT";
    topBall=(homeBats===pinnedHome)?"bat":false;
    botBall=topBall?false:"bat";
  }
  // The narrow 3x5 font doubled for every team name, every sport - Joe's
  // call (2026-09-30): it looked better than the round 5x7 font, and it
  // means the board never switches style depending on the opponent.
  const condensed=true;
  const live=g.state==="in";

  const extras=[po&&po.round, po&&po.summary, g.preseason&&"PRESEASON"];
  const extraColor=po?PLAYOFF_GOLD:PRESEASON_SILVER;
  if(g.state==="post"){
    // "FINAL 10/1" when the result shown isn't from today; in the playoffs
    // it takes turns with the series result ("TB WINS 3-1"), in the
    // preseason with "PRESEASON"
    const fl=cycle([g.final_label||"FINAL", po&&po.summary, g.preseason&&"PRESEASON"],pair);
    text((W-tw(fl,F3))>>1,LAY.status,fl,GOLD,F3);
  } else if(g.state==="pre"){
    // Date top-left, first pitch / puck drop / kickoff top-right. In the
    // playoffs or preseason that line takes turns with the round, the
    // series standing, or "PRESEASON", centred.
    const k=String(g.kickoff_local||"").trim().split(/\s+/);
    const date=k.length>1?k[0]:"", time=k.length>1?k[k.length-1]:(k[0]||"");
    const msgs=["__time__", ...extras.filter(Boolean)];
    const msg=msgs[pair%msgs.length];
    if(msg==="__time__"){
      if(date) text(2,LAY.status,date,DATEC,F3);
      text(W-2-tw(time,F3),LAY.status,time,CLOCK,F3);
    } else {
      text((W-tw(msg,F3))>>1,LAY.status,msg,extraColor,F3);
    }
  } else if(sport==="baseball"){
    // No clock in baseball. While someone's batting:  ▼7  2-1  ◇  1 OUT
    // (the diamond sits between the count and the outs so the two numbers
    // can't run together). Between half-innings just "MID 7TH"/"END 7TH".
    const atBat=g.half==="TOP"||g.half==="BOT";
    if(atBat){
      const num=(g.inning_short||"").replace(/^[A-Z]/,"")||g.period||"";
      drawInning(1,LAY.status,g.half,num,CLOCK);
      if(g.balls!=null && g.strikes!=null)
        text(17,LAY.status,g.balls+"-"+g.strikes,DATEC,F3);
      drawBaseDiamond(30,LAY.status,g.bases);
      if(g.outs!=null){
        const o=g.outs+" OUT";
        text(W-2-tw(o,F3),LAY.status,o,CLOCK,F3);
      }
    } else {
      text(2,LAY.status,g.inning_text||g.period_label||"",CLOCK,F3);
    }
  } else if(sport==="hockey" && g.intermission){
    // between periods: "1ST INT" and the break's own countdown
    const left=(g.period_label||"")+" INT";
    text(2,LAY.status,left,CLOCK,F3);
    if(g.intermission_left) text(W-2-tw(g.intermission_left,F3),LAY.status,g.intermission_left,DATEC,F3);
  } else {
    const q=g.period_label||"OT";
    const left=q+" "+(g.clock||"");
    text(2,LAY.status,left,CLOCK,F3);
    let r="", rc=GRAY;
    if(sport==="football"){
      r=shortDown(g.down_distance,tw(left,F3)); rc=g.redzone?RED:GRAY;
    } else if(sport==="hockey" && g.pp){
      // PP when your team has the extra skater, PK when you're killing one.
      // Drawn as a solid chip so it can't run together with the game clock,
      // with the power play's own countdown beside it if there's room.
      const mine=g.pp.side===g.pinned_side, c=mine?GREEN:RED, tag=mine?"PP":"PK";
      const leftEnd=2+tw(left,F3);
      const tt=g.pp.time||"";
      const timeX=W-2-tw(tt,F3);
      const chipW=tw(tag,F3)+2;
      let xr=W-3;
      if(tt && (timeX-3)-chipW+1 >= leftEnd+3){ text(timeX,LAY.status,tt,c,F3); xr=timeX-3; }
      chip(xr,tag,c);
    } else if(sport==="basketball" && g.bonus && g.bonus[g.pinned_side]){
      // your team's in the bonus: the other side's next foul is free throws
      chip(W-3,"BONUS",GREEN);
    }
    if(r)text(W-2-tw(r,F3),LAY.status,r,rc,F3);
  }
  // Before a playoff game the record slots show series wins instead -
  // the regular-season record doesn't mean much in October
  let topRec=top.record, botRec=bot.record;
  // (until a game's been played, 0 WINS vs 0 WINS says nothing - keep the
  // records up for game 1)
  if(po && po.wins && g.state==="pre" && ((po.wins.home||0)+(po.wins.away||0))>0){
    const w=n=>(n==null?"":n+(n===1?" WIN":" WINS"));
    topRec=w(po.wins[pinnedHome?"home":"away"]);
    botRec=w(po.wins[pinnedHome?"away":"home"]);
  }
  // Before the game, if both logos are in: a matchup card - away logo on
  // the left, home on the right, records under them. Otherwise the rows.
  const la=matchupLogo(g.away), lh=matchupLogo(g.home);
  if(g.state==="pre" && la && lh){
    const recA=pinnedHome?botRec:topRec, recH=pinnedHome?topRec:botRec;
    drawMatchup(la, lh, g.away, g.home, recA, recH);
    return;
  }
  teamRow(LAY.row1,top.abbr,top.score,ledColor(top.color),topBall,topRec,top.rank,condensed);
  teamRow(LAY.row2,bot.abbr,bot.score,ledColor(bot.color),botBall,botRec,bot.rank,condensed);
}

function tickerBlock(y,g,ranked){
  let aS=null,hS=null;
  if(g.score){const p=String(g.score).split("-");aS=p[0];hS=p[1];}
  let rt,rb;
  if(g.status==="F"){rt="FINAL";rb="";}
  else if(!g.score){rt=g.kick_time||"";rb=g.kick_date||"";}
  else {rt=g.status||"";rb=g.clock||"";}

  // College ticker: a narrow rank column on the left, four-letter
  // abbreviations, scores right-aligned so the columns line up.
  [[g.away,aS,false,g.away_color,g.away_rank],[g.home,hS,true,g.home_color,g.home_rank]]
  .forEach(([abbr,sc,isHome,hex,rk],i)=>{
    const ry=y+i*6;
    abbr=fullAbbr(abbr);
    if(ranked){
      if(rk){const rs=String(rk); text(8-tw(rs,F3),ry,rs,DATEC,F3);}  // dimmer than the score so it reads as a label
      text(10,ry,abbr,ledColor(hex),F3);
      if(sc!==null)text(34-tw(sc,F3),ry,sc,WHITE,F3);
      if(g.possession&&(g.possession==="home")===isHome)sprite(35,ry,BALL,BROWN);
    } else {
      text(3,ry,abbr,ledColor(hex),F3);
      if(sc!==null)text(21,ry,sc,WHITE,F3);
      if(g.possession&&(g.possession==="home")===isHome)sprite(31,ry,BALL,BROWN);
    }
  });
  // A time reads bright; a date or period label sits behind it.
  const sched=!g.score&&g.status!=="F";
  const inn=/^([TB])(\d+)$/.exec(rt||"");
  if(inn){
    // baseball: same ▲/▼ + inning as the main line
    const n=inn[2], w=6+tw(n,F3);
    drawInning(W-3-w,y,inn[1]==="T"?"TOP":"BOT",n,DATEC);
  } else if(rt)text(W-3-tw(rt,F3),y,rt,rt==="FINAL"?GREEN:(sched?CLOCK:DATEC),F3);
  if(rb)text(W-3-tw(rb,F3),y+6,rb,sched?DATEC:CLOCK,F3);

  const bar=g.redzone?RED:(g.fantasy?GREEN:null);
  if(bar)for(let yy=y;yy<y+11;yy++){put(0,yy,bar);put(W-1,yy,bar);}
}

function renderPixels(g,pair){
  px=new Array(W*H).fill(null);
  if(!g)return;
  drawMain(g,pair);
  // playoffs: a gold frame round your game - top edge, both sides, and the
  // dividing line underneath - so it reads as the postseason at a glance
  const frame=g.playoff?PLAYOFF_GOLD:(g.preseason?PRESEASON_SILVER:null);
  const dotted=!g.playoff && g.preseason;      // preseason frame is dotted
  const on=i=>!dotted || i%2===0;
  for(let x=0;x<W;x++)put(x,LAY.divider,frame&&on(x)?frame:LINE);
  if(frame){
    for(let x=0;x<W;x++) if(on(x)) put(x,0,frame);
    for(let y=0;y<LAY.divider;y++) if(on(y)){put(0,y,frame);put(W-1,y,frame);}
  }
  const games=g.ticker_games||[];
  if(games.length){
    const i=(pair*2)%games.length;
    tickerBlock(LAY.tickA,games[i%games.length],g.ranked);
    for(let x=2;x<39;x++)put(x,LAY.tickDiv,LINE);  // stops short of the clock
    tickerBlock(LAY.tickB,games[(i+1)%games.length],g.ranked);
  }
}

const cv=document.getElementById("panel"),ctx=cv.getContext("2d");
function paint(){
  const dpr=Math.min(window.devicePixelRatio||1,2);
  const cell=Math.max(4,Math.floor((cv.clientWidth*dpr)/W));
  const size=cell*W;
  if(cv.width!==size){cv.width=size;cv.height=size;}
  ctx.fillStyle="#0a0b0d";ctx.fillRect(0,0,size,size);
  const r=cell*0.39;
  for(let y=0;y<H;y++)for(let x=0;x<W;x++){
    ctx.fillStyle=px[y*W+x]||OFF;
    ctx.beginPath();ctx.arc(x*cell+cell/2,y*cell+cell/2,r,0,6.2832);ctx.fill();
  }
}


// ------------------------------------------------------ touchdown explosion
// A blast, not a firework: the panel flashes white, a shockwave ring tears
// outward in the team's colors, debris flies with trails, and secondary
// blasts keep going off around it. All of it drawn on the same 64x64 grid
// as the scoreboard, so the panel firmware can do exactly this.
const FX_MS = 5000;
let fx = null;

function rgbStr(c, b) {
  return "rgb(" + Math.round(c[0]*b) + "," + Math.round(c[1]*b) + "," + Math.round(c[2]*b) + ")";
}

// Dark team colors would vanish against black, so lift them the same way
// the scoreboard does before using them in the blast.
function liftFx(c) {
  if (!c) return [255,255,255];
  const m = Math.max(c[0], c[1], c[2]);
  if (m === 0) return [255,255,255];
  if (m < 150) { const f = 200/m;
    return [Math.min(255, c[0]*f|0), Math.min(255, c[1]*f|0), Math.min(255, c[2]*f|0)]; }
  return c;
}

function makeBlast(cx, cy, palette, power) {
  const parts = [];
  // three shells at different speeds - reads as one violent burst rather
  // than a tidy ring
  const rings = [{n: 30*power, spd: 1.9}, {n: 22*power, spd: 1.15}, {n: 14*power, spd: 0.55}];
  for (const ring of rings) {
    for (let i = 0; i < ring.n; i++) {
      const ang = Math.random() * Math.PI * 2;
      const spd = ring.spd * (0.6 + Math.random()*0.8);
      parts.push({
        x: cx, y: cy,
        vx: Math.cos(ang)*spd, vy: Math.sin(ang)*spd,
        life: 1, decay: 0.008 + Math.random()*0.007,
        color: palette[(Math.random()*palette.length)|0]
      });
    }
  }
  return parts;
}

// Team logos, redrawn as LED pixels.
// The image is scaled down to the panel's scale on a scratch canvas, then
// each pixel is read back and lit. Anything near-transparent or almost
// black is skipped so the logo sits on the panel instead of in a box.
const logoCache = {};

// Shrink a logo's pixels (RGBA, dw*SS x dh*SS, SS samples per LED) to one
// colour per LED. Two ways, picked by size (tested on the real ESPN files
// for the Gators, Knicks, Giants, Mizzou, 76ers and Cardinals):
//
//  "snap" (big - celebrations): average the patch, then snap that average
//    to the nearest of the logo's OWN main colours. Shapes stay smooth,
//    fine detail that's big enough survives (the Knicks ball's seams, the
//    gator's teeth and eye), and no in-between colours get invented
//    (averaging alone turned orange-beside-blue into green).
//  "dominant" (small - pregame matchup): the colour that fills most of the
//    patch. At ~25px there's no room for detail; this keeps the bold shapes
//    and drops the fine shading that would otherwise turn into stripes.
function logoPalette(bd) {
  const keyOf = i => ((bd[i] >> 5) << 10) | ((bd[i+1] >> 5) << 5) | (bd[i+2] >> 5);
  const cnt = new Map(); let total = 0;
  for (let i = 0; i < bd.length; i += 4) {
    if (bd[i+3] < 140) continue;
    total++;
    const k = keyOf(i); let e = cnt.get(k);
    if (!e) { e = {n:0, r:0, g:0, b:0}; cnt.set(k, e); }
    e.n++; e.r += bd[i]; e.g += bd[i+1]; e.b += bd[i+2];
  }
  const cols = [...cnt.values()].sort((p, q) => q.n - p.n);
  const pal = [];
  for (const e of cols) {
    if (e.n < total * 0.02) break;                // only colours that matter
    const c = [e.r/e.n, e.g/e.n, e.b/e.n];
    const near = pal.find(q => Math.hypot(q.c[0]-c[0], q.c[1]-c[1], q.c[2]-c[2]) < 70);
    if (near) near.n += e.n; else pal.push({n: e.n, c: c});
  }
  return pal.map(q => q.c);
}

function logoPixels(bd, dw, dh, SS, mode) {
  const pix = [];
  const need = Math.ceil(SS*SS*0.34);             // mostly-empty patches stay dark
  const pal = mode === "snap" ? logoPalette(bd) : null;
  for (let y = 0; y < dh; y++) for (let x = 0; x < dw; x++) {
    let opaque = 0, sr = 0, sg = 0, sb = 0;
    const bucket = mode === "snap" ? null : new Map();
    for (let by = 0; by < SS; by++) for (let bx = 0; bx < SS; bx++) {
      const i = (((y*SS + by) * dw*SS) + (x*SS + bx)) * 4;
      if (bd[i+3] < 140) continue;
      opaque++;
      if (pal) { sr += bd[i]; sg += bd[i+1]; sb += bd[i+2]; continue; }
      // coarse key, so near-identical shades count as the same colour
      const key = ((bd[i] >> 5) << 10) | ((bd[i+1] >> 5) << 5) | (bd[i+2] >> 5);
      let e = bucket.get(key);
      if (!e) { e = {n:0, r:0, g:0, b:0}; bucket.set(key, e); }
      e.n++; e.r += bd[i]; e.g += bd[i+1]; e.b += bd[i+2];
    }
    if (opaque < need) continue;                  // edge of the mark
    let r, gg, bb;
    if (pal && pal.length) {
      const avg = [sr/opaque, sg/opaque, sb/opaque];
      let best = pal[0], bd2 = Infinity;
      for (const c of pal) {
        const d = Math.hypot(c[0]-avg[0], c[1]-avg[1], c[2]-avg[2]);
        if (d < bd2) { bd2 = d; best = c; }
      }
      [r, gg, bb] = best;
    } else {
      let best = null;
      bucket.forEach(e => { if (!best || e.n > best.n) best = e; });
      if (!best) continue;
      r = best.r/best.n; gg = best.g/best.n; bb = best.b/best.n;
    }
    if (r + gg + bb < 70) continue;               // near-black, drop it

    const mx = Math.max(r, gg, bb), mn = Math.min(r, gg, bb);
    if (mx - mn > 10) {                           // it has a hue: deepen it
      const avg = (r + gg + bb) / 3;
      r = Math.max(0, Math.min(255, avg + (r - avg) * 1.7));
      gg = Math.max(0, Math.min(255, avg + (gg - avg) * 1.7));
      bb = Math.max(0, Math.min(255, avg + (bb - avg) * 1.7));
    }
    const m2 = Math.max(r, gg, bb);
    if (m2 > 0 && m2 < 175) { const f = 175/m2; r*=f; gg*=f; bb*=f; }
    pix.push([x, y, Math.min(255,r|0), Math.min(255,gg|0), Math.min(255,bb|0)]);
  }
  // A lit dot with no lit neighbours is noise, not part of the mark.
  const lit = new Set(pix.map(q => q[1]*dw + q[0]));
  return pix.filter(q => {
    let n = 0;
    for (let dy = -1; dy <= 1; dy++) for (let dx = -1; dx <= 1; dx++) {
      if (!dx && !dy) continue;
      if (lit.has((q[1]+dy)*dw + (q[0]+dx))) n++;
    }
    return n >= 2;
  });
}

// size: the box the logo is fitted into (54 = nearly the whole panel for
// celebrations; smaller for the pregame matchup). Cached per size.
function loadLogo(url, cb, size, height) {
  const S = size || 54, SH = height || S;      // box: S wide, SH tall
  if (!url) { setDiag("ESPN sent no logo address for this team"); return cb(null); }
  const key = url + "@" + S + "x" + SH;
  if (key in logoCache) return cb(logoCache[key]);
  setDiag("loading … " + url);
  const img = new Image();
  img.onload = function () {
    // ESPN's logo files have transparent margin around the mark. Scaling
    // the whole file wastes pixels on empty space, so find what's actually
    // drawn and crop to it first - that alone makes the mark much bigger.
    const P = 128;
    const pc = document.createElement("canvas");
    pc.width = P; pc.height = P;
    const pg = pc.getContext("2d");
    pg.drawImage(img, 0, 0, P, P);
    let pd;
    try { pd = pg.getImageData(0, 0, P, P).data; }
    catch (e) { logoCache[key] = null; setDiag("browser blocked reading the image"); return cb(null); }
    let x0 = P, y0 = P, x1 = -1, y1 = -1;
    for (let y = 0; y < P; y++) for (let x = 0; x < P; x++) {
      if (pd[(y*P + x)*4 + 3] > 40) {
        if (x < x0) x0 = x; if (x > x1) x1 = x;
        if (y < y0) y0 = y; if (y > y1) y1 = y;
      }
    }
    if (x1 < 0) { x0 = 0; y0 = 0; x1 = P-1; y1 = P-1; }

    const iw = img.width || P, ih = img.height || P;
    const sx = x0/P * iw, sy = y0/P * ih;
    const sw2 = (x1-x0+1)/P * iw, sh2 = (y1-y0+1)/P * ih;
    // keep the mark's shape; fit it inside the S x SH box
    const fit = Math.min(S/(x1-x0+1), SH/(y1-y0+1));
    const dw = Math.max(1, Math.round((x1-x0+1) * fit));
    const dh = Math.max(1, Math.round((y1-y0+1) * fit));

    const SS = 8;                                   // source samples per LED
    const big = document.createElement("canvas");
    big.width = dw*SS; big.height = dh*SS;
    const bg2 = big.getContext("2d");
    bg2.imageSmoothingEnabled = true; bg2.imageSmoothingQuality = "high";
    bg2.drawImage(img, sx, sy, sw2, sh2, 0, 0, dw*SS, dh*SS);
    let bd;
    try { bd = bg2.getImageData(0, 0, dw*SS, dh*SS).data; }
    catch (e) { logoCache[key] = null; setDiag("browser blocked reading the image"); return cb(null); }

    const clean = logoPixels(bd, dw, dh, SS, Math.max(S, SH) >= 40 ? "snap" : "dominant");
    logoCache[key] = clean.length ? {w:dw, h:dh, pix:clean} : null;
    setDiag(clean.length ? ("ok, " + clean.length + " lit pixels")
                         : "image loaded but every pixel was filtered out");
    cb(logoCache[key]);
  };
  img.onerror = function () {
    logoCache[key] = null;
    setDiag("server could not fetch it — see the Terminal window");
    cb(null);
  };
  img.src = "/api/logo?u=" + encodeURIComponent(url);
}

function drawLogo(logo, ox, oy, bright) {
  if (!logo) return;
  // Blank the logo's footprint first, so sparks flying past don't get
  // mistaken for part of the mark.
  for (const q of logo.pix) {
    for (let dy = -1; dy <= 1; dy++) for (let dx = -1; dx <= 1; dx++)
      put(ox + q[0] + dx, oy + q[1] + dy, "rgb(0,0,0)");
  }
  for (const q of logo.pix) {
    put(ox + q[0], oy + q[1],
        "rgb(" + Math.round(q[2]*bright) + "," + Math.round(q[3]*bright) + "," + Math.round(q[4]*bright) + ")");
  }
}

function startFireworks(palette, label, logo, banner) {
  const pal = palette.map(liftFx);
  fx = {
    t0: performance.now(),
    pal: pal,
    label: label,
    banner: banner || "TOUCHDOWN",
    logo: logo || null,
    parts: makeBlast(32, 30, pal, 1.6),      // the main hit, centre panel
    waves: [
      {cx:32, cy:30, t0:0,   color: pal[0], speed:0.055, max:56},
      {cx:32, cy:30, t0:110, color: pal[1] || pal[0], speed:0.042, max:50},
      {cx:32, cy:30, t0:230, color: [255,255,255], speed:0.032, max:44}
    ],
    nextBlast: 620
  };
  requestAnimationFrame(stepFireworks);
}

function drawRing(cx, cy, r, color, bright) {
  if (r < 1) return;
  const steps = Math.max(10, Math.round(r * 7));
  for (let i = 0; i < steps; i++) {
    const a = (Math.PI*2*i)/steps;
    put(Math.round(cx + Math.cos(a)*r), Math.round(cy + Math.sin(a)*r), rgbStr(color, bright));
  }
}

function stepFireworks(now) {
  if (!fx) return;
  if (fx.kind === "kick")      return stepKick(now - fx.t0);
  if (fx.kind === "quarter")   return stepQuarter(now - fx.t0);
  if (fx.kind === "fieldgoal") return stepFieldGoal(now - fx.t0);
  if (fx.kind === "flag")      return stepFlag(now - fx.t0);
  if (fx.kind === "firstdown") return stepFirstDown(now - fx.t0);
  if (fx.kind === "goallight") return stepGoalLight(now - fx.t0);
  if (fx.kind === "run")       return stepRun(now - fx.t0);
  if (fx.kind === "homerun")   return stepHomeRun(now - fx.t0);
  if (fx.kind === "three")     return stepThree(now - fx.t0);
  const el = now - fx.t0;
  px = new Array(W*H).fill(null);

  // 1. the flash - whole panel white, gone in a blink
  if (el < 130) {
    const b = 1 - el/130;
    const c = rgbStr([255,255,255], b);
    for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) put(x, y, c);
  }

  // 2. shockwave rings tearing outward
  for (const w of fx.waves) {
    const t = el - w.t0;
    if (t <= 0) continue;
    const r = t * w.speed;
    if (r > w.max) continue;
    drawRing(w.cx, w.cy, r, w.color, Math.max(0, 1 - r/w.max));
    drawRing(w.cx, w.cy, r - 1, w.color, Math.max(0, 0.45 - r/w.max));
  }

  // 3. secondary blasts, so it keeps erupting instead of petering out
  if (el > fx.nextBlast && el < FX_MS - 800) {
    const cx = 6 + Math.random()*52, cy = 6 + Math.random()*46;
    fx.parts = fx.parts.concat(makeBlast(cx, cy, fx.pal, 1.05));
    fx.waves.push({cx:cx, cy:cy, t0:el, color: fx.pal[(Math.random()*fx.pal.length)|0],
                   speed:0.045, max:30});
    // drop spent debris so the list doesn't grow without bound
    fx.parts = fx.parts.filter(p => p.life > 0);
    fx.nextBlast = el + 240 + Math.random()*200;
  }

  // 4. debris
  for (const pt of fx.parts) {
    if (pt.life <= 0) continue;
    pt.x += pt.vx; pt.y += pt.vy;
    pt.vy += 0.022;      // gravity
    pt.vx *= 0.985;
    pt.life -= pt.decay;
    if (pt.life > 0) {
      const b = Math.max(0, Math.min(1, pt.life));
      put(Math.round(pt.x), Math.round(pt.y), rgbStr(pt.color, b));
      if (b > 0.4) put(Math.round(pt.x - pt.vx), Math.round(pt.y - pt.vy), rgbStr(pt.color, b*0.4));
      if (b > 0.7) put(Math.round(pt.x - pt.vx*2), Math.round(pt.y - pt.vy*2), rgbStr(pt.color, b*0.18));
    }
  }

  // 5. the team punches through once the first blast clears
  if (el > 620) {
    const cyc = fx.pal[Math.floor(el/130) % fx.pal.length];
    if (fx.logo) {
      // fade the logo up over the first third of a second
      const b = Math.min(1, (el - 620) / 320);
      const ly = Math.max(0, (54 - fx.logo.h) >> 1);
      drawLogo(fx.logo, (W - fx.logo.w) >> 1, ly, b);
    } else {
      const lw = tw(fx.label, F5, 2);
      if (lw <= W - 2) text((W-lw)>>1, 20, fx.label, rgbStr(cyc, 1), F5, 2);
      else { const lw1 = tw(fx.label, F5, 1);
             text((W-lw1)>>1, 24, fx.label, rgbStr(cyc, 1), F5, 1); }
    }
    // always bright - cycling this through a navy team colour made it vanish
    const banner = fx.banner;
    const sw = tw(banner, F3, 1);
    const ty = fx.logo ? 58 : 42;
    for (let bx = -1; bx <= sw; bx++) for (let by = -1; by <= 5; by++)
      put(((W-sw)>>1) + bx, ty + by, "rgb(0,0,0)");
    text((W-sw)>>1, ty, banner,
         (Math.floor(el/130) % 2) ? "rgb(255,255,255)" : "rgb(255,190,0)", F3, 1);
  }

  paint();
  if (el < FX_MS) requestAnimationFrame(stepFireworks);
  else { fx = null; draw(); }
}


// ------------------------------------------------------------- kickoff
// Three beats: the teams arrive, the ball is struck, the word lands.
// A touchdown is chaos; this is anticipation, so it builds instead of
// exploding, and it gets out of the way in under five seconds.
const KICK_MS = 5000;

function startKickoff(pal, awayAbbr, homeAbbr) {
  fx = { kind:"kick", t0: performance.now(), pal: pal.map(liftFx),
         away: awayAbbr || "", home: homeAbbr || "" };
  requestAnimationFrame(stepFireworks);
}

function stepKick(el) {
  px = new Array(W*H).fill(null);
  const A = fx.pal[0], B = fx.pal[1] || fx.pal[0];

  if (el < 1000) {
    // BEAT ONE - the teams take the field. Two colour walls close on the
    // middle, each carrying its own abbreviation on the leading edge.
    const t = el / 1000;
    const reach = Math.round(33 * t);
    for (let x = 0; x < reach; x++) {
      const fade = 0.30 + 0.70 * (1 - x / 33);
      for (let y = 0; y < H; y++) {
        put(x, y, rgbStr(A, fade * 0.55));
        put(W-1-x, y, rgbStr(B, fade * 0.55));
      }
    }
    const aA = shortAbbr(fx.away), hA = shortAbbr(fx.home);
    const aw = tw(aA, F5, 1), hw = tw(hA, F5, 1);
    text(Math.max(1, reach - aw - 2), 28, aA, "rgb(255,255,255)", F5, 1);
    text(Math.min(W - hw - 1, W - reach + 2), 28, hA, "rgb(255,255,255)", F5, 1);

  } else if (el < 1350) {
    // the walls drop away, leaving an empty field
    const t = (el - 1000) / 350;
    const top = Math.round(H * t);
    for (let x = 0; x < W; x++) for (let y = top; y < H; y++) {
      put(x, y, rgbStr(x < 32 ? A : B, 0.55 * (1 - t)));
    }
    for (let gx = 0; gx < W; gx++) put(gx, 58, "rgb(20,70,25)");

  } else if (el < 3100) {
    // BEAT TWO - the kick. A real kickoff hangs; so does this one.
    const t = (el - 1350) / 1750;
    for (let gx = 0; gx < W; gx++) put(gx, 58, "rgb(20,70,25)");
    for (let k = 7; k >= 1; k--) {          // trail behind the ball
      const tt = t - k * 0.030;
      if (tt < 0) continue;
      const bx = 4 + tt * 55, by = 54 - Math.sin(Math.PI * tt) * 44;
      put(Math.round(bx), Math.round(by), rgbStr([190,95,30], 0.5 - k * 0.055));
    }
    const x = 4 + t * 55, y = 54 - Math.sin(Math.PI * t) * 44;
    sprite(Math.round(x) - 3, Math.round(y) - 2, BALL, BROWN);

  } else {
    // BEAT THREE - the word, over a pulse in the two team colours
    const t = (el - 3100) / (KICK_MS - 3100);
    const pulse = 0.25 + 0.20 * Math.sin(el / 90);
    for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) {
      put(x, y, rgbStr(x < 32 ? A : B, pulse * 0.35 * (1 - t*0.5)));
    }
    const w1 = tw("KICK", F5, 2), w2 = tw("OFF", F5, 2);
    const show = Math.min(1, t * 3);
    // second line takes the team's own accent colour, not a generic gold
    const accent = fx.pal[1] || [255,190,0];
    for (let bx = 0; bx < W; bx++) for (let by = 16; by < 48; by++) put(bx, by, "rgb(0,0,0)");
    text((W - w1) >> 1, 18, "KICK", rgbStr([255,255,255], show), F5, 2);
    text((W - w2) >> 1, 34, "OFF", rgbStr(accent, show), F5, 2);
  }

  paint();
  if (el < KICK_MS) requestAnimationFrame(stepFireworks);
  else { fx = null; draw(); }
}

// -------------------------------------------------------- quarter break
// Punctuation, not celebration: wipe the board away, state what just
// ended and where the score stands, wipe back. Halftime holds longer
// because it is the break people actually get up for.
// lines: optional two-line title, e.g. ["1ST","INTERMISSION"] for hockey.
// Football's halftime is the same layout with ["HALF","TIME"].
function startQuarter(label, away, home, pal, lines) {
  fx = { kind:"quarter", t0: performance.now(), label: label,
         lines: lines || (label === "HALFTIME" ? ["HALF","TIME"] : null),
         away: away || {}, home: home || {},
         pal: pal.map(liftFx), dur: 5000 };
  requestAnimationFrame(stepFireworks);
}

function stepQuarter(el) {
  px = new Array(W*H).fill(null);
  const C = fx.pal[0];
  const IN = 420, OUT = fx.dur - 420;

  if (el < IN) {                    // wipe in, from the top
    const edge = Math.round(H * (el / IN));
    for (let y = 0; y < edge; y++) for (let x = 0; x < W; x++)
      put(x, y, rgbStr(C, y > edge - 3 ? 0.9 : 0.16));
  } else if (el > OUT) {            // wipe back out, downward
    const t = (el - OUT) / 420;
    const edge = Math.round(H * t);
    for (let y = edge; y < H; y++) for (let x = 0; x < W; x++)
      put(x, y, rgbStr(C, y < edge + 3 ? 0.9 : 0.16));
  } else {
    for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) put(x, y, rgbStr(C, 0.16));
    // a rule above and below the label, so it reads as a title card
    for (let x = 6; x < W-6; x++) { put(x, 14, rgbStr(C, 0.95)); put(x, 30, rgbStr(C, 0.95)); }

    const big = !!fx.lines;
    if (big) {
      // two-line title: first line big and white, second in the team's
      // accent colour at the biggest size that fits ("TIME", "REGULATION",
      // "INTERMISSION")
      const [l1, l2] = fx.lines;
      const accent = rgbStr(fx.pal[1] || [255,190,0], 1);
      // first line big; too wide ("END OF") -> the board's narrow font, doubled
      if (tw(l1, F5, 2) <= W - 4) text((W - tw(l1, F5, 2)) >> 1, 16, l1, "rgb(255,255,255)", F5, 2);
      else                        text((W - tw(l1, F3, 2)) >> 1, 18, l1, "rgb(255,255,255)", F3, 2);
      if (tw(l2, F5, 2) <= W - 4)      text((W - tw(l2, F5, 2)) >> 1, 33, l2, accent, F5, 2);
      else if (tw(l2, F5, 1) <= W - 4) text((W - tw(l2, F5, 1)) >> 1, 36, l2, accent, F5, 1);
      else                             text((W - tw(l2, F3, 1)) >> 1, 37, l2, accent, F3, 1);
      for (let x = 6; x < W-6; x++) put(x, 30, null);
    } else {
      const lw = tw(fx.label, F5, 2);
      if (lw <= W - 4) text((W - lw) >> 1, 17, fx.label, "rgb(255,255,255)", F5, 2);
      else text((W - tw(fx.label, F5, 1)) >> 1, 20, fx.label, "rgb(255,255,255)", F5, 1);
    }

    // The score, laid out exactly as the scoreboard does it: team on the
    // left in its own colour, points on the right in white. Same reading
    // order, same colour coding - the card should feel like the board.
    if (!big) {
      const rows = [fx.away, fx.home];
      for (let i = 0; i < 2; i++) {
        const side = rows[i], y = 36 + i*11;
        text(2, y + 2, shortAbbr(side.abbr), ledColor(side.color), F3, 1);
        const sc = String(side.score == null ? "-" : side.score);
        text(W - 1 - tw(sc, F5, 1), y, sc, "rgb(255,255,255)", F5, 1);
      }
    } else {
      for (let bx = 0; bx < W; bx++) for (let by = 48; by < 58; by++) put(bx, by, "rgb(0,0,0)");
      const aA = shortAbbr(fx.away.abbr), hA = shortAbbr(fx.home.abbr);
      const aS = String(fx.away.score == null ? "-" : fx.away.score);
      const hS = String(fx.home.score == null ? "-" : fx.home.score);
      const wA = tw(aA, F3, 1), wAS = tw(" " + aS + "   ", F3, 1),
            wH = tw(hA, F3, 1), wHS = tw(" " + hS, F3, 1);
      let x = (W - (wA + wAS + wH + wHS)) >> 1;
      text(x, 50, aA, ledColor(fx.away.color), F3, 1);            x += wA;
      text(x, 50, " " + aS + "   ", "rgb(255,255,255)", F3, 1);   x += wAS;
      text(x, 50, hA, ledColor(fx.home.color), F3, 1);            x += wH;
      text(x, 50, " " + hS, "rgb(255,255,255)", F3, 1);
    }
  }

  paint();
  if (el < fx.dur) requestAnimationFrame(stepFireworks);
  else { fx = null; draw(); }
}

// ------------------------------------------------------------ field goal
// Quieter than a touchdown - the ball splits the uprights, one flash
// confirms it, then the total's up and it's gone before the next snap.
const FG_MS = 5000;

function startFieldGoal(pal, label) {
  fx = { kind:"fieldgoal", t0: performance.now(), pal: pal.map(liftFx), label: label || "" };
  requestAnimationFrame(stepFireworks);
}

// A real upright: one pole rising off the ground to the crossbar, then
// splitting into two tall arms above it - a slingshot shape, not a
// soccer-goal rectangle. Drawn in the same golden-yellow real posts are
// painted, standing on the right the whole time so the kick has
// something to aim at.
const POST_X = 54, CROSS_Y = 30, ARM_X = 6, TOP_Y = 9, GROUND_Y = 52;
function drawGoalpost(b) {
  const c = rgbStr([255,205,40], b);
  for (let y = CROSS_Y; y <= GROUND_Y; y++) put(POST_X, y, c);           // base pole
  for (let x = POST_X - ARM_X; x <= POST_X + ARM_X; x++) put(x, CROSS_Y, c); // crossbar
  for (let y = TOP_Y; y <= CROSS_Y; y++) {                              // the two arms
    put(POST_X - ARM_X, y, c);
    put(POST_X + ARM_X, y, c);
  }
}

function stepFieldGoal(el) {
  px = new Array(W*H).fill(null);
  const A = fx.pal[0];
  drawGoalpost(1);

  if (el < 1200) {
    // the kick - rises off the left, arcs up, and splits the uprights
    // right where the two arms leave room for it
    const t = el / 1200;
    const arc = Math.sin(Math.PI * t) * 10;
    for (let k = 6; k >= 1; k--) {
      const tt = t - k * 0.035;
      if (tt < 0) continue;
      const bx = 6 + tt * 48, by = GROUND_Y - tt * 34 - Math.sin(Math.PI*tt)*10;
      put(Math.round(bx), Math.round(by), rgbStr([190,95,30], 0.5 - k * 0.06));
    }
    const bx = 6 + t * 48, by = GROUND_Y - t * 34 - arc;
    sprite(Math.round(bx) - 3, Math.round(by) - 2, BALL, BROWN);
  } else if (el < 1450) {
    // the flash that says it's good, right between the arms
    const b = 1 - (el - 1200) / 250;
    for (let y = TOP_Y; y < CROSS_Y; y++)
      for (let x = POST_X - ARM_X; x < POST_X + ARM_X; x++) put(x, y, rgbStr([255,255,255], b));
  } else {
    const t = (el - 1450) / (FG_MS - 1450);
    const show = Math.min(1, t * 3);
    const w1 = tw("FIELD", F5, 1), w2 = tw("GOAL", F5, 1);
    text((W-w1)>>1, 20, "FIELD", rgbStr(A, show), F5, 1);
    text((W-w2)>>1, 29, "GOAL", rgbStr(A, show), F5, 1);
    if (fx.label) {
      const lw = tw(fx.label, F3, 1);
      text((W-lw)>>1, 42, fx.label, rgbStr([255,255,255], show), F3, 1);
    }
  }

  paint();
  if (el < FG_MS) requestAnimationFrame(stepFireworks);
  else { fx = null; draw(); }
}

// ------------------------------------------------------------------ flag
// A penalty doesn't earn the whole panel, but it's easy to miss on a
// glance - so it gets its own beat, held long enough to actually read
// even if you weren't looking right at it when the flag came out.
const FLAG_MS = 5000;
const FLIGHT_MS = 1000;  // the toss itself, before it hits the turf
// A real penalty flag is two parts: a small knot where the weight is tied
// in, and a square of yellow cloth whose loose corners stream and flop
// around it. Drawn here as a compact darker knot ("2") with longer tails
// of cloth ("1") trailing off it. In flight the heavy end leads and the
// cloth streams behind; the frames below are the tails at different
// points of a flutter, and every other one is mirrored so the knot swaps
// ends - that's the end-over-end tumble of a thrown flag.
const FLAG_FLY = [
  ["110000000000",
   "011100000000",
   "001111100220",
   "000011112222",
   "001111112222",
   "011100000220",
   "110000000000"],
  ["000000000000",
   "111110000000",
   "011111110220",
   "000111112222",
   "000011112222",
   "111111100220",
   "000000000000"],
  ["011100000000",
   "001111000000",
   "000011110220",
   "111111112222",
   "000001112222",
   "000011100220",
   "000111000000"],
  ["000111000000",
   "000011100000",
   "000001110220",
   "111111112222",
   "000011112222",
   "001111000220",
   "011100000000"],
];
const mirrorRows = rows => rows.map(r => r.split("").reverse().join(""));
// Knot leads, flop, knot flips to the other end, flop again
const FLAG_TUMBLE = [FLAG_FLY[0], FLAG_FLY[1], mirrorRows(FLAG_FLY[2]),
                     mirrorRows(FLAG_FLY[0]), mirrorRows(FLAG_FLY[1]), FLAG_FLY[3]];
// On the turf: the knot in the middle, the four loose corners splayed out
const FLAG_LANDED = ["0110000000110",
                     "0111100011110",
                     "0011112111100",
                     "0001122211000",
                     "0011112111100",
                     "0111100011110",
                     "0110000000110"];
const FLAG_CLOTH = "rgb(255,215,0)", FLAG_KNOT = "rgb(200,120,0)";
const TURF_Y = 53;

// team / teamColor: who the flag is on, shown as "ON NYG" in their colour
function startFlag(label, team, teamColor) {
  fx = { kind:"flag", t0: performance.now(), label: label || "",
         team: team || "", teamColor: teamColor || "rgb(255,255,255)" };
  requestAnimationFrame(stepFireworks);
}

// ESPN's play text reads like "... PENALTY on NYG-J.Smith, Offensive
// Holding, 10 yards, enforced at ...". Pull out who and what, and squeeze
// the foul into the 15 characters the bottom line holds.
const FOUL_SHORT = {
  "PASS INTERFERENCE":"PASS INTERF", "UNNECESSARY ROUGHNESS":"UNNEC ROUGH",
  "ROUGHING THE PASSER":"RUFF PASSER", "ROUGHING THE KICKER":"RUFF KICKER",
  "UNSPORTSMANLIKE CONDUCT":"UNSPORTSMANLIKE", "NEUTRAL ZONE INFRACTION":"NEUTRAL ZONE",
  "ILLEGAL BLOCK IN THE BACK":"BLOCK IN BACK", "INTENTIONAL GROUNDING":"GROUNDING",
  "TOO MANY MEN ON THE FIELD":"TOO MANY MEN", "TOO MANY MEN ON FIELD":"TOO MANY MEN",
  "ILLEGAL FORMATION":"ILLEGAL FORM", "ILLEGAL USE OF HANDS":"ILLEGAL HANDS",
  "HORSE COLLAR TACKLE":"HORSE COLLAR", "DELAY OF GAME":"DELAY OF GAME",
  "ILLEGAL CONTACT":"ILLEGAL CONTACT", "LOWERING THE HEAD TO INITIATE CONTACT":"LOWER HEAD",
};
function parsePenalty(txt) {
  const t = String(txt || "");
  const who = /PENALTY on ([A-Z]{2,4})[-\s]/i.exec(t);
  const what = /PENALTY on [^,]*,\s*([^,]+)/i.exec(t);
  let foul = (what ? what[1] : t.replace(/^.*PENALTY[, ]*/i, "").split(",")[0])
               .toUpperCase().replace(/\s+/g, " ").trim();
  // the team's on screen already, so offensive/defensive is redundant
  foul = foul.replace(/^(OFFENSIVE|DEFENSIVE) /, "");
  foul = FOUL_SHORT[foul] || foul;
  while (foul.length > 15 && foul.includes(" ")) foul = foul.slice(0, foul.lastIndexOf(" "));
  if (foul.length > 15) foul = foul.slice(0, 15);
  return { team: who ? who[1].toUpperCase() : "", foul: foul };
}

function stepFlag(el) {
  px = new Array(W*H).fill(null);

  // background pulses gold behind the text so it reads at a glance
  const pulse = 0.08 + 0.05 * Math.sin(el / 140);
  for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) put(x, y, rgbStr([255,190,0], pulse));
  // a strip of turf for it to land on
  for (let x = 0; x < W; x++) put(x, TURF_Y, "rgb(20,90,30)");

  // lob stays below the FLAG lettering (which ends at row 19)
  // lob stays below the "ON NYG" line (which ends at row 25)
  const path = t => [4 + t * 22, 46 - Math.sin(Math.PI * t) * 18];
  if (el < FLIGHT_MS) {
    // thrown in a lob, like a ref's toss - up, over, down to the turf
    const t = el / FLIGHT_MS;
    const [x0, y0] = path(t);
    const fi = Math.floor(el / 75);
    // a fading copy one beat behind, so the flutter reads as motion
    if (t > 0.1) {
      const [px0, py0] = path(t - 0.1);
      const prev = FLAG_TUMBLE[(fi + FLAG_TUMBLE.length - 1) % FLAG_TUMBLE.length];
      sprite(Math.round(px0), Math.round(py0), prev, "rgb(110,90,0)", "rgb(80,50,0)");
    }
    sprite(Math.round(x0), Math.round(y0), FLAG_TUMBLE[fi % FLAG_TUMBLE.length],
           FLAG_CLOTH, FLAG_KNOT);
  } else {
    // down and settled, corners out, lying on the turf. A short bounce
    // on landing, then still.
    const s = el - FLIGHT_MS;
    const hop = s < 180 ? Math.round(Math.sin(Math.PI * s / 180) * 2) : 0;
    sprite(25, TURF_Y - 7 - hop, FLAG_LANDED, FLAG_CLOTH, FLAG_KNOT);
  }

  const blink = Math.floor(el / 260) % 2 === 0;
  const w = tw("FLAG", F5, 2);
  text((W-w)>>1, 2, "FLAG", blink ? "rgb(255,255,255)" : GOLD, F5, 2);
  if (fx.team) {
    // who it's on - "ON" in white, the team in its own colour
    const ab = fullAbbr(fx.team), onW = tw("ON ", F5, 1), abW = tw(ab, F5, 1);
    const x0 = (W - onW - abW) >> 1;
    text(x0, 19, "ON", "rgb(255,255,255)", F5, 1);
    text(x0 + onW, 19, ab, fx.teamColor, F5, 1);
  }
  if (fx.label) {
    const lw = tw(fx.label, F3, 1);
    if (lw <= W - 4) text((W-lw)>>1, 56, fx.label, "rgb(255,255,255)", F3, 1);
  }

  paint();
  if (el < FLAG_MS) requestAnimationFrame(stepFireworks);
  else { fx = null; draw(); }
}

// ------------------------------------------------------------ first down
// Lighter than a score - a quick highlight, not a reason to look away
// from the game, but held a beat longer than a blink so a glance at the
// panel actually catches it instead of just missing it.
const FIRSTDOWN_MS = 5000;

function startFirstDown(pal, label) {
  fx = { kind:"firstdown", t0: performance.now(), pal: pal.map(liftFx), label: label || "" };
  requestAnimationFrame(stepFireworks);
}

function stepFirstDown(el) {
  px = new Array(W*H).fill(null);
  const A = fx.pal[0];
  const IN = 200, OUT = FIRSTDOWN_MS - 300;
  let b = 1;
  if (el < IN) b = el / IN;
  else if (el > OUT) b = Math.max(0, 1 - (el - OUT) / 300);

  for (let x = 4; x < W-4; x++) {
    put(x, 22, rgbStr([0,230,80], 0.9*b));
    put(x, 46, rgbStr([0,230,80], 0.9*b));
  }
  const txt = "1ST DOWN";
  const w2 = tw(txt, F3, 2);
  if (w2 <= W - 4) text((W-w2)>>1, 28, txt, rgbStr(A, b), F3, 2);
  else text((W-tw(txt,F3,1))>>1, 30, txt, rgbStr(A, b), F3, 1);
  if (fx.label) {
    const lw = tw(fx.label, F3, 1);
    text((W-lw)>>1, 44, fx.label, rgbStr([255,255,255], b), F3, 1);
  }

  paint();
  if (el < FIRSTDOWN_MS) requestAnimationFrame(stepFireworks);
  else { fx = null; draw(); }
}

// ------------------------------------------------------- hockey goal light
// The real thing: the red lamp behind the net spins up, the arena goes
// red, then the team logo. Two beats - the lamp (2s), then the logo with
// GOAL under it - and a short white cut between them.
const GOAL_MS = 5000;
const LAMP = ["0001111000",
              "0011111100",
              "0111111110",
              "0111111110",
              "2222222222"];

function startGoalLight(pal, label, logo) {
  fx = { kind:"goallight", t0: performance.now(), pal: pal.map(liftFx),
         label: label || "", logo: logo || null };
  requestAnimationFrame(stepFireworks);
}

function stepGoalLight(el) {
  px = new Array(W*H).fill(null);
  const STROBE = 2000, CUT = 2200;
  const RED_ = [255, 20, 20];

  if (el < STROBE) {
    // a rotating beacon: each sweep washes the panel red, four a second
    const ph = (el % 250) / 250;
    const wash = Math.max(0, 1 - Math.abs(ph - 0.5) * 2.4);
    for (let y = 0; y < H; y++) for (let x = 0; x < W; x++)
      put(x, y, rgbStr(RED_, 0.08 + 0.45 * wash));
    // two beams turning around the lamp
    const cx = 32, cy = 5, a0 = (el / 250) * Math.PI * 2;
    for (let k = 0; k < 2; k++) {
      const a = a0 + k * Math.PI;
      for (let r = 6; r < 64; r++) {
        const bx = Math.round(cx + Math.cos(a) * r);
        const by = Math.round(cy + Math.abs(Math.sin(a)) * r * 0.9);
        put(bx, by, rgbStr([255, 90, 60], Math.max(0, 0.9 - r / 70)));
      }
    }
    sprite(27, 1, LAMP, rgbStr(RED_, 0.55 + 0.45 * wash), "rgb(110,110,110)");
    // GOAL, flashing white on the sweep
    const w = tw("GOAL", F5, 2);
    for (let bx = -2; bx < w + 2; bx++) for (let by = -2; by < 16; by++)
      put(((W - w) >> 1) + bx, 30 + by, "rgb(0,0,0)");
    text((W - w) >> 1, 30, "GOAL", wash > 0.35 ? "rgb(255,255,255)" : rgbStr(RED_, 1), F5, 2);
  } else if (el < CUT) {
    const b = 1 - (el - STROBE) / (CUT - STROBE);
    for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) put(x, y, rgbStr([255,255,255], b));
  } else {
    // the team: logo up, GOAL underneath, red edges still pulsing like
    // the lamp's still going
    const b = Math.min(1, (el - CUT) / 300);
    const pulse = 0.4 + 0.6 * (Math.floor(el / 250) % 2);
    for (let y = 0; y < H; y++) { put(0, y, rgbStr(RED_, pulse)); put(W-1, y, rgbStr(RED_, pulse)); }
    const gc = (Math.floor(el / 250) % 2) ? "rgb(255,255,255)" : rgbStr(RED_, 1);
    if (fx.logo) {
      const ly = Math.max(1, (54 - fx.logo.h) >> 1);
      drawLogo(fx.logo, (W - fx.logo.w) >> 1, ly, b);
      const sw = tw("GOAL", F3, 1);
      for (let bx = -1; bx <= sw; bx++) for (let by = -1; by <= 5; by++)
        put(((W - sw) >> 1) + bx, 58 + by, "rgb(0,0,0)");
      text((W - sw) >> 1, 58, "GOAL", gc, F3, 1);
    } else {
      // no logo to show: the team big in its colour, GOAL just as big
      const ab = shortAbbr(fx.label);
      const lw = tw(ab, F5, 2);
      text((W - lw) >> 1, 12, ab, rgbStr(fx.pal[0], b), F5, 2);
      const gw = tw("GOAL", F5, 2);
      text((W - gw) >> 1, 36, "GOAL", gc, F5, 2);
    }
  }

  paint();
  if (el < GOAL_MS) requestAnimationFrame(stepFireworks);
  else { fx = null; draw(); }
}

// ------------------------------------------------------------ run scores
// Baseball's version of a first down: most runs aren't home runs, and a
// sac fly or a bases-loaded walk still deserves a beat. A ball rolls in,
// RUN lands in the team's colour, then it gets out of the way.
const RUN_MS = 5000;
const BASEBALL = ["0011100",
                  "0111110",
                  "1211121",
                  "1111111",
                  "1211121",
                  "0111110",
                  "0011100"];

function startRun(pal, label, n) {
  fx = { kind:"run", t0: performance.now(), pal: pal.map(liftFx),
         label: label || "", n: Math.max(1, n || 1) };
  requestAnimationFrame(stepFireworks);
}

function stepRun(el) {
  px = new Array(W*H).fill(null);
  const A = fx.pal[0];
  const OUT = RUN_MS - 300;
  let b = 1;
  if (el < 200) b = el / 200;
  else if (el > OUT) b = Math.max(0, 1 - (el - OUT) / 300);

  for (let x = 4; x < W-4; x++) { put(x, 14, rgbStr(A, 0.9*b)); put(x, 48, rgbStr(A, 0.9*b)); }
  // the ball rolls in from the left and settles above the word
  const t = Math.min(1, el / 600);
  const bx = Math.round(-7 + t * 35);
  sprite(bx, 4, BASEBALL, rgbStr([255,255,255], b), rgbStr([220,30,30], b));

  const word = fx.n > 1 ? "RUNS" : "RUN";
  const ww = tw(word, F5, 2);
  text((W - ww) >> 1, 20, word, rgbStr(A, b), F5, 2);
  const sub = fx.label + (fx.n > 1 ? " +" + fx.n : "");
  const sw = tw(sub, F3, 1);
  text((W - sw) >> 1, 39, sub, rgbStr([255,255,255], b), F3, 1);

  paint();
  if (el < RUN_MS) requestAnimationFrame(stepFireworks);
  else { fx = null; draw(); }
}

// --------------------------------------------------------------- home run
// A ball leaving the yard. The park: night sky, a crowd in the stands,
// the outfield wall with its yellow home-run line, yellow foul poles,
// striped grass, the infield dirt and home plate at the bottom.
//   0-300ms     crack of the bat - a white burst at the plate
//   300-1800    the ball climbs over the wall and out, shrinking as it
//               gets further away, trail behind it
//   1800-3500   it's gone: crowd lights up, fireworks over the stands,
//               HOME RUN (or GRAND SLAM)
//   3500-5000   the team: logo, or the abbreviation big, banner below
const HR_MS = 5000;
const HR_BALL = ["010","111","010"];

function startHomeRun(pal, label, logo, grand) {
  fx = { kind:"homerun", t0: performance.now(), pal: pal.map(liftFx),
         label: label || "", logo: logo || null, grand: !!grand,
         parts: [], nextBurst: 1850 };
  requestAnimationFrame(stepFireworks);
}

// stable per-pixel "random" so the crowd doesn't reshuffle every frame
function hash2(x, y) { let h = (x * 374761393 + y * 668265263) | 0;
  h = (h ^ (h >>> 13)) * 1274126177 | 0; return ((h ^ (h >>> 16)) >>> 0) / 4294967295; }

function drawPark(el, b, cheer) {
  // sky
  for (let y = 0; y < 18; y++) for (let x = 0; x < W; x++) put(x, y, rgbStr([8, 14, 40], b));
  // light towers glowing at the top corners
  for (const lx of [6, 57]) for (let dx = -1; dx <= 1; dx++) put(lx + dx, 2, rgbStr([255,250,220], b));
  // stands: a crowd of dim dots; when they cheer, some flash bright
  const crowd = [[200,60,60],[60,90,200],[220,220,220],[200,160,60],[60,160,90]];
  for (let y = 18; y < 31; y++) for (let x = 0; x < W; x++) {
    const r = hash2(x, y);
    if (r < 0.45) continue;
    const c = crowd[(r * 997 | 0) % crowd.length];
    const flash = cheer > 0 && hash2(x + (el / 120 | 0), y) > 1 - 0.35 * cheer;
    put(x, y, rgbStr(c, b * (flash ? 1 : 0.28)));
  }
  // outfield wall, yellow home-run line along the top
  for (let x = 0; x < W; x++) {
    put(x, 31, rgbStr([255, 205, 40], b));
    for (let y = 32; y < 36; y++) put(x, y, rgbStr([0, 70, 35], b));
  }
  // foul poles
  for (let y = 12; y < 36; y++) { put(3, y, rgbStr([255,205,40], b)); put(60, y, rgbStr([255,205,40], b)); }
  // grass, mowed in stripes
  for (let y = 36; y < H; y++) for (let x = 0; x < W; x++)
    put(x, y, rgbStr(((y - 36) >> 2) % 2 ? [20, 105, 40] : [14, 84, 30], b));
  // The infield, seen from behind home plate: dirt base paths running
  // home -> 1st -> 2nd -> 3rd -> home, the mound as a small dirt circle in
  // the middle, white bags, and home plate down at the bottom where the
  // batter stands - so the ball clearly comes off the bat, not the mound.
  const DIRT = rgbStr([150, 92, 48], b), WHITE_ = rgbStr([255, 255, 255], b);
  const HOME = [32, 61], FIRST = [43, 51], SECOND = [32, 41], THIRD = [21, 51];
  const line = (a, c) => {
    for (let i = 0; i <= 40; i++) {
      const x = Math.round(a[0] + (c[0] - a[0]) * i / 40), y = Math.round(a[1] + (c[1] - a[1]) * i / 40);
      put(x, y, DIRT); put(x, y + 1, DIRT);
    }
  };
  line(HOME, FIRST); line(FIRST, SECOND); line(SECOND, THIRD); line(THIRD, HOME);
  // dirt circle around home plate, and the mound
  for (let y = 57; y < H; y++) for (let x = 27; x <= 37; x++)
    if ((x - 32) * (x - 32) + (y - 61) * (y - 61) * 1.6 <= 26) put(x, y, DIRT);
  for (const [dx, dy] of [[0,-1],[-1,0],[0,0],[1,0],[0,1]]) put(32 + dx, 51 + dy, DIRT);
  // chalk foul lines: from home plate, along the first- and third-base
  // lines, all the way out to the bottom of each foul pole
  const CHALK = rgbStr([235, 235, 235], b * 0.9);
  for (const pole of [[3, 36], [60, 36]]) {
    for (let i = 0; i <= 60; i++) {
      put(Math.round(HOME[0] + (pole[0] - HOME[0]) * i / 60),
          Math.round(HOME[1] + (pole[1] - HOME[1]) * i / 60), CHALK);
    }
  }
  // bases
  for (const [bx, by] of [FIRST, SECOND, THIRD]) {
    put(bx, by, WHITE_); put(bx + 1, by, WHITE_); put(bx, by - 1, WHITE_); put(bx + 1, by - 1, WHITE_);
  }
  // home plate
  for (let x = 31; x <= 33; x++) put(x, 61, WHITE_);
  put(32, 62, WHITE_);
}

// The batter beside the plate, in the team's colour, and the bat: cocked
// back, then through the zone at the crack, then the follow-through.
function drawBatter(el, color) {
  const c = rgbStr(color, 1), wood = "rgb(215,165,105)";
  put(28, 54, "rgb(230,190,160)");                    // head
  for (let y = 55; y <= 58; y++) put(28, y, c);        // body
  put(27, 59, c); put(29, 59, c); put(27, 60, c); put(29, 60, c);   // legs
  let bat;
  if (el < 120)      bat = [[29,55],[28,53],[27,52],[26,51]];          // loaded
  else if (el < 320) bat = [[29,56],[30,56],[31,56],[32,56],[33,56]];  // contact
  else               bat = [[29,55],[30,54],[31,53],[32,52]];          // follow-through
  for (const [x, y] of bat) put(x, y, wood);
}

function stepHomeRun(el) {
  px = new Array(W*H).fill(null);
  const LOGO_AT = 3500;
  // off the bat at the plate, out to right-centre and over the wall
  const ballPos = t => [33 + 11 * t, 56 - 61 * t + 10 * t * t];

  if (el < LOGO_AT) {
    const gone = el > 1800;
    const cheer = el < 1100 ? 0 : Math.min(1, (el - 1100) / 500);
    drawPark(el, gone ? 0.55 : 1, cheer);
    if (!gone) drawBatter(el, fx.pal[0]);
    if (el < 120) {
      // the pitch, coming in from the mound
      const t = el / 120;
      put(32 + Math.round(t), 51 + Math.round(5 * t), "rgb(255,255,255)");
    }

    if (el >= 120 && el < 420) {
      // crack of the bat, right where it meets the ball
      const k = 1 - (el - 120) / 300;
      for (let r = 1; r <= 4; r++) {
        const c = rgbStr([255, 255, 255], k * (1 - r / 5));
        put(33 + r, 56, c); put(33 - r, 56, c); put(33, 56 - r, c);
        put(33 + r, 56 - r, c); put(33 - r, 56 - r, c);
      }
    }
    if (el >= 120 && el < 1800) {
      const t = Math.min(1, (el - 120) / 1680);
      for (let k = 6; k >= 1; k--) {                       // trail
        const tt = t - k * 0.03; if (tt < 0) continue;
        const [tx, ty] = ballPos(tt);
        put(Math.round(tx), Math.round(ty), rgbStr([255,255,255], 0.85 - k * 0.11));
      }
      const [bx, by] = ballPos(t);
      const rx = Math.round(bx), ry = Math.round(by);
      // a dark ring around the ball once it's up against the crowd, so it
      // never gets lost in the stands (over the grass it doesn't need one)
      if (ry < 33)
        for (let dy = -2; dy <= 2; dy++) for (let dx = -2; dx <= 2; dx++)
          if (Math.abs(dx) + Math.abs(dy) <= 3) put(rx + dx, ry + dy, "rgb(0,0,0)");
      // further away = smaller
      if (t < 0.5) sprite(rx - 1, ry - 1, HR_BALL, "rgb(255,255,255)");
      else { put(rx, ry, "rgb(255,255,255)"); put(rx + 1, ry, "rgb(255,255,255)");
             put(rx, ry + 1, "rgb(255,255,255)"); put(rx + 1, ry + 1, "rgb(255,255,255)"); }
    }

    if (gone) {
      // fireworks over the stands in the team's colours
      if (el > fx.nextBurst && el < LOGO_AT - 500) {
        fx.parts = fx.parts.filter(p => p.life > 0).concat(
          makeBlast(10 + Math.random() * 44, 6 + Math.random() * 10, fx.pal, 0.55));
        fx.nextBurst = el + 260 + Math.random() * 180;
      }
      for (const pt of fx.parts) {
        if (pt.life <= 0) continue;
        pt.x += pt.vx * 0.7; pt.y += pt.vy * 0.7; pt.vy += 0.02; pt.life -= pt.decay * 1.4;
        if (pt.life > 0) put(Math.round(pt.x), Math.round(pt.y), rgbStr(pt.color, Math.min(1, pt.life)));
      }
      // the words, on a black plate so they read over the park
      const l1 = fx.grand ? "GRAND" : "HOME", l2 = fx.grand ? "SLAM" : "RUN";
      const show = Math.min(1, (el - 1800) / 250);
      const c = (Math.floor(el / 200) % 2) ? "rgb(255,255,255)" : rgbStr(fx.pal[0], 1);
      const w1 = tw(l1, F5, 2), w2 = tw(l2, F5, 2);
      for (let y = 21; y < 57; y++) for (let x = 2; x < W - 2; x++) put(x, y, rgbStr([0,0,0], 1));
      text((W - w1) >> 1, 23, l1, show >= 1 ? c : rgbStr([255,255,255], show), F5, 2);
      text((W - w2) >> 1, 40, l2, show >= 1 ? c : rgbStr([255,255,255], show), F5, 2);
    }
  } else {
    // the team
    const b = Math.min(1, (el - LOGO_AT) / 300);
    const banner = fx.grand ? "GRAND SLAM" : "HOME RUN";
    const bc = (Math.floor(el / 200) % 2) ? "rgb(255,255,255)" : rgbStr(fx.pal[0], 1);
    if (fx.logo) {
      drawLogo(fx.logo, (W - fx.logo.w) >> 1, Math.max(1, (54 - fx.logo.h) >> 1), b);
      const sw = tw(banner, F3, 1);
      for (let bx = -1; bx <= sw; bx++) for (let by = -1; by <= 5; by++)
        put(((W - sw) >> 1) + bx, 58 + by, "rgb(0,0,0)");
      text((W - sw) >> 1, 58, banner, bc, F3, 1);
    } else {
      const ab = shortAbbr(fx.label), lw = tw(ab, F5, 2);
      text((W - lw) >> 1, 14, ab, rgbStr(fx.pal[0], b), F5, 2);
      const sw = tw(banner, F3, 1);
      text((W - sw) >> 1, 38, banner, bc, F3, 1);
    }
  }

  paint();
  if (el < HR_MS) requestAnimationFrame(stepFireworks);
  else { fx = null; draw(); }
}

// ------------------------------------------------------------ 3-pointer
// From downtown. A shooter on the left, the hoop on the right (backboard,
// orange rim, white net, pole); the ball arcs high and drops through
// clean - the net kicks - then THREE / POINTER and the team.
//   0-1300ms    the shot, with a trail
//   1300-1650   through the net, the net ripples
//   1650-1850   white flash
//   1850-3500   THREE / POINTER, sparks in the team's colours
//   3500-5000   logo (or abbreviation) with the banner
const THREE_MS = 5000;
const HOOP_BALL = ["01110","12121","11211","12121","01110"];
function startThree(pal, label, logo) {
  fx = { kind:"three", t0: performance.now(), pal: pal.map(liftFx),
         label: label || "", logo: logo || null, parts: [], nextBurst: 1900 };
  requestAnimationFrame(stepFireworks);
}
function drawCourt(el, color) {
  // floor
  for (let y = 58; y < H; y++) for (let x = 0; x < W; x++) put(x, y, "rgb(120,72,32)");
  for (let x = 0; x < W; x++) put(x, 58, "rgb(170,110,55)");
  // pole and arm
  for (let y = 9; y < 58; y++) { put(61, y, "rgb(110,110,110)"); put(62, y, "rgb(110,110,110)"); }
  for (let x = 58; x < 61; x++) put(x, 12, "rgb(110,110,110)");
  // backboard (edge-on) and the rim
  for (let y = 4; y < 22; y++) put(57, y, "rgb(255,255,255)");
  for (let x = 47; x <= 56; x++) put(x, 18, "rgb(255,110,20)");
  // net: hangs from the rim, kicks when the ball goes through
  const kick = el > 1300 && el < 1900 ? Math.sin((el - 1300) / 60) * 1.5 : 0;
  for (let i = 0; i <= 3; i++) {
    for (let y = 19; y <= 26; y++) {
      const t = (y - 19) / 7;
      const xl = Math.round(47 + i * 1 + t * 2 + kick * t);
      const xr = Math.round(56 - i * 1 - t * 2 + kick * t);
      if ((y + i) % 2 === 0) { put(xl, y, "rgb(230,230,230)"); put(xr, y, "rgb(230,230,230)"); }
    }
  }
  // the shooter, arms up through the release
  const c = rgbStr(color, 1);
  put(6, 43, "rgb(230,190,160)");
  for (let y = 44; y <= 50; y++) put(6, y, c);
  put(5, 51, c); put(7, 51, c); put(5, 52, c); put(7, 52, c); put(5, 53, c); put(7, 53, c);
  for (let y = 54; y < 58; y++) { put(5, y, c); put(7, y, c); }
  if (el < 400) { put(7, 42, c); put(8, 41, c); put(5, 42, c); }       // release
  else { put(7, 43, c); put(8, 44, c); put(5, 44, c); }                 // follow-through
}
function stepThree(el) {
  px = new Array(W*H).fill(null);
  const pos = t => [8 + 43.5 * t, (1 - t) * 40 + t * 16 - 23 * Math.sin(Math.PI * t)];   // peaks near the top, stays on the panel
  if (el < 1850) {
    drawCourt(el, fx.pal[0]);
    if (el < 1300) {
      const t = el / 1300;
      for (let k = 6; k >= 1; k--) {
        const tt = t - k * 0.03; if (tt < 0) continue;
        const [tx, ty] = pos(tt);
        put(Math.round(tx), Math.round(ty), rgbStr([255,140,40], 0.55 - k * 0.07));
      }
      const [bx, by] = pos(t);
      sprite(Math.round(bx) - 2, Math.round(by) - 2, HOOP_BALL, "rgb(235,120,30)", "rgb(70,35,10)");
    } else if (el < 1650) {
      // straight down through the net
      const t = (el - 1300) / 350;
      sprite(50, Math.round(14 + t * 14), HOOP_BALL, "rgb(235,120,30)", "rgb(70,35,10)");
      for (let x = 46; x <= 56; x++) put(x, 18, "rgb(255,110,20)");   // rim in front
    } else {
      const b = 1 - (el - 1650) / 200;
      for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) put(x, y, rgbStr([255,255,255], b));
    }
  } else if (el < 3500) {
    // sparks in team colours behind the words
    if (el > fx.nextBurst && el < 3200) {
      fx.parts = fx.parts.filter(p => p.life > 0).concat(
        makeBlast(8 + Math.random() * 48, 8 + Math.random() * 44, fx.pal, 0.5));
      fx.nextBurst = el + 220 + Math.random() * 160;
    }
    for (const pt of fx.parts) {
      if (pt.life <= 0) continue;
      pt.x += pt.vx * 0.7; pt.y += pt.vy * 0.7; pt.vy += 0.02; pt.life -= pt.decay * 1.4;
      if (pt.life > 0) put(Math.round(pt.x), Math.round(pt.y), rgbStr(pt.color, Math.min(1, pt.life) * 0.7));
    }
    const c1 = (Math.floor(el / 200) % 2) ? "rgb(255,255,255)" : rgbStr(fx.pal[0], 1);
    const c2 = rgbStr(fx.pal[1] || [255,255,255], 1);
    const w1 = tw("THREE", F5, 2), w2 = tw("POINTER", F5, 1);
    for (let y = 10; y < 42; y++) for (let x = 3; x < W - 3; x++) put(x, y, "rgb(0,0,0)");
    text((W - w1) >> 1, 12, "THREE", c1, F5, 2);
    text((W - w2) >> 1, 32, "POINTER", c2, F5, 1);
  } else {
    const b = Math.min(1, (el - 3500) / 300);
    const banner = "3 POINTER";
    const bc = (Math.floor(el / 200) % 2) ? "rgb(255,255,255)" : rgbStr(fx.pal[0], 1);
    if (fx.logo) {
      drawLogo(fx.logo, (W - fx.logo.w) >> 1, Math.max(1, (54 - fx.logo.h) >> 1), b);
      const sw = tw(banner, F3, 1);
      for (let bx = -1; bx <= sw; bx++) for (let by = -1; by <= 5; by++)
        put(((W - sw) >> 1) + bx, 58 + by, "rgb(0,0,0)");
      text((W - sw) >> 1, 58, banner, bc, F3, 1);
    } else {
      const ab = shortAbbr(fx.label), lw = tw(ab, F5, 2);
      text((W - lw) >> 1, 14, ab, rgbStr(fx.pal[0], b), F5, 2);
      const sw = tw(banner, F3, 1);
      text((W - sw) >> 1, 38, banner, bc, F3, 1);
    }
  }
  paint();
  if (el < THREE_MS) requestAnimationFrame(stepFireworks);
  else { fx = null; draw(); }
}

function firstDownNum(dd) {
  if (!dd) return null;
  const m = /^(\d+)(?:st|nd|rd|th)/i.exec(String(dd).trim());
  return m ? parseInt(m[1], 10) : null;
}

const BOARDS=[
  {key:"AUTO",label:"Auto",auto:true},
  {key:"NYG",label:"Giants",college:false,sport:"football"},
  {key:"DAL",label:"Cowboys",college:false,sport:"football"},
  {key:"UF",label:"Gators",college:true,sport:"football"},
  {key:"LSU",label:"LSU",college:true,sport:"football"},
  {key:"NYY",label:"Yankees",sport:"baseball"},
  {key:"NYR",label:"Rangers",sport:"hockey"},
  {key:"NYK",label:"Knicks",sport:"basketball"}
];
let board=BOARDS[0], game=null, pair=0;
const dot=document.getElementById("dot"),
      statusText=document.getElementById("statusText"),
      note=document.getElementById("note");

const chips=document.getElementById("chips");
BOARDS.forEach(b=>{
  const el=document.createElement("button");
  el.className="chip";el.type="button";el.textContent=b.label;
  el.setAttribute("aria-pressed",String(b.key===board.key));
  el.addEventListener("click",()=>{
    board=b;pair=0;game=null;
    setPressed();
    statusText.textContent="Loading "+b.label+"…";
    load();
  });
  chips.appendChild(el);
});

// ------------------------------------------------- layout preview samples
// Made-up games, one per sport, so every layout can be checked even when
// nothing's on. Picking one stops the live feed until a real board is
// picked again.
const DEMOS={
  baseball:{label:"Baseball",game:{
    sport:"baseball",state:"in",pinned_side:"home",period:7,period_label:"7",
    inning_text:"BOT 7TH",half:"BOT",balls:2,strikes:1,outs:1,bases:[true,false,true],
    home:{abbr:"NYY",score:4,color:"132448",color2:"C4CED3",logo:"https://a.espncdn.com/i/teamlogos/mlb/500/nyy.png"},
    away:{abbr:"BOS",score:3,color:"BD3039",color2:"0C2340",logo:"https://a.espncdn.com/i/teamlogos/mlb/500/bos.png"},
    ticker_games:[
      {away:"TOR",home:"BAL",status:"T6",clock:"2 OUT",score:"2-5",away_color:"134A8E",home_color:"DF4601"},
      {away:"LAD",home:"SD",status:"F",score:"6-1",away_color:"005A9C",home_color:"2F241D"},
      {away:"HOU",home:"SEA",status:"B3",clock:"0 OUT",score:"1-1",away_color:"EB6E1F",home_color:"005C5C"},
      {away:"NYM",home:"ATL",kick_time:"7:20P",kick_date:"10/1",away_color:"FF5910",home_color:"CE1141"}]}},
  hockey:{label:"Hockey",game:{
    sport:"hockey",state:"in",pinned_side:"home",period:2,period_label:"2ND",clock:"8:14",
    pp:{side:"home",time:"1:23"},
    home:{abbr:"NYR",score:2,color:"0038A8",color2:"CE1126",logo:"https://a.espncdn.com/i/teamlogos/nhl/500/nyr.png"},
    away:{abbr:"NJ",score:1,color:"CE1126",color2:"000000",logo:"https://a.espncdn.com/i/teamlogos/nhl/500/nj.png"},
    ticker_games:[
      {away:"BOS",home:"TOR",status:"3RD",clock:"4:51",score:"3-2",away_color:"FFB81C",home_color:"00205B"},
      {away:"PIT",home:"PHI",status:"F",score:"5-0",away_color:"FCB514",home_color:"F74902"},
      {away:"NYI",home:"WSH",status:"1ST",clock:"12:07",score:"0-0",away_color:"F47D30",home_color:"C8102E"},
      {away:"TB",home:"FLA",kick_time:"7:00P",kick_date:"10/2",away_color:"002868",home_color:"C8102E"}]}},
  basketball:{label:"Basketball",game:{
    sport:"basketball",state:"in",pinned_side:"home",period:4,period_label:"4TH",clock:"2:31",
    bonus:{home:true,away:false},
    home:{abbr:"NY",score:104,color:"1D428A",color2:"F58426",logo:"https://a.espncdn.com/i/teamlogos/nba/500/ny.png"},
    away:{abbr:"BOS",score:101,color:"007A33",color2:"BA9653",logo:"https://a.espncdn.com/i/teamlogos/nba/500/bos.png"},
    ticker_games:[
      {away:"MIA",home:"PHI",status:"3RD",clock:"6:10",score:"77-81",away_color:"98002E",home_color:"006BB6"},
      {away:"LAL",home:"GS",status:"F",score:"118-112",away_color:"552583",home_color:"1D428A"},
      {away:"MIL",home:"CHI",status:"2ND",clock:"0:42",score:"55-49",away_color:"00471B",home_color:"CE1141"},
      {away:"DEN",home:"PHX",kick_time:"10:00P",kick_date:"10/1",away_color:"0E2240",home_color:"E56020"}]}},
  playoffs:{label:"Playoffs",game:{
    sport:"baseball",state:"pre",pinned_side:"home",kickoff_local:"10/3 6:30P",
    playoff:{round:"ALDS GAME 2",summary:"TB LEADS 1-0",wins:{home:0,away:1}},
    home:{abbr:"NYY",score:null,color:"132448",color2:"C4CED3",record:"93-69",logo:"https://a.espncdn.com/i/teamlogos/mlb/500/nyy.png"},
    away:{abbr:"TB",score:null,color:"092C5C",color2:"8FBCE6",record:"98-64",logo:"https://a.espncdn.com/i/teamlogos/mlb/500/tb.png"},
    ticker_games:[
      {away:"CHW",home:"CLE",kick_time:"1:00P",kick_date:"10/3",away_color:"27251F",home_color:"00385D"},
      {away:"ATL",home:"LAD",kick_time:"4:00P",kick_date:"10/3",away_color:"13274F",home_color:"005A9C"}]}},
  college:{label:"College (ranked)",game:{
    sport:"football",state:"in",pinned_side:"home",ranked:true,period:3,period_label:"3RD",clock:"4:12",
    down_distance:"3rd & 7 at TENN 34",redzone:false,possession:"home",
    home:{abbr:"FLA",score:21,color:"0021A5",color2:"FA4616",rank:12,record:"4-1",logo:"https://a.espncdn.com/i/teamlogos/ncaa/500/57.png"},
    away:{abbr:"TENN",score:17,color:"FF8200",color2:"FFFFFF",rank:8,record:"5-0",logo:"https://a.espncdn.com/i/teamlogos/ncaa/500/2633.png"},
    ticker_games:[
      {away:"MISS",home:"UGA",status:"2ND",clock:"9:40",score:"14-10",away_rank:14,home_rank:3,
       away_color:"CE1126",home_color:"BA0C2F",possession:"away"},
      {away:"OSU",home:"MICH",status:"F",score:"31-24",away_rank:2,home_rank:11,
       away_color:"BB0000",home_color:"00274C"},
      {away:"TEX",home:"OKLA",status:"4TH",clock:"1:58",score:"20-23",away_rank:5,home_rank:null,
       away_color:"BF5700",home_color:"841617",redzone:true,possession:"away"},
      {away:"ALA",home:"LSU",kick_time:"7:30P",kick_date:"10/4",away_rank:7,home_rank:19,
       away_color:"9E1B32",home_color:"461D7C"}]}}
};

const demoChips=document.getElementById("demoChips");
function setPressed(){
  [...chips.children].forEach((c,i)=>c.setAttribute("aria-pressed",String(!board.demo&&BOARDS[i].key===board.key)));
  [...demoChips.children].forEach(c=>c.setAttribute("aria-pressed",String(board.demo===c.dataset.demo)));
}
Object.keys(DEMOS).forEach(k=>{
  const el=document.createElement("button");
  el.className="chip";el.type="button";el.textContent=DEMOS[k].label;el.dataset.demo=k;
  el.setAttribute("aria-pressed","false");
  el.addEventListener("click",()=>{ board={key:"DEMO_"+k,demo:k,label:DEMOS[k].label}; pair=0; game=null; setPressed(); load(); });
  demoChips.appendChild(el);
});

// How often to refresh: every 5s while the game on the board is live, so
// a touchdown or goal shows up within seconds; every 20s otherwise.
// In Auto during a live game, follow that game directly and only re-check
// all five teams once a minute (a higher-priority game might have started).
const LIVE_MS=5000, IDLE_MS=20000, AUTO_RECHECK_MS=60000;
let pollTimer=null, autoPick=null, lastAutoAt=0;
function scheduleNext(){
  clearTimeout(pollTimer);
  pollTimer=setTimeout(load, game && game.state==="in" && !board.demo ? LIVE_MS : IDLE_MS);
}

async function load(){
  if(board.demo){
    game=JSON.parse(JSON.stringify(DEMOS[board.demo].game));
    loadMatchupLogos(game);
    dot.classList.remove("bad"); note.style.display="none";
    statusText.innerHTML="<b>Preview</b> &middot; sample "+DEMOS[board.demo].label.toLowerCase()+" game, not live";
    draw();
    scheduleNext();
    return;
  }
  try{
    let url;
    if(board.auto){
      const following = game && game.state==="in" && autoPick &&
                        Date.now()-lastAutoAt < AUTO_RECHECK_MS;
      if(following){
        url=`/api/game?team=${autoPick.team}&college=${autoPick.college}&sport=${autoPick.sport}`;
      } else { url=`/api/game?auto=1`; lastAutoAt=Date.now(); }
    } else {
      url=`/api/game?team=${board.key}&college=${board.college?1:0}&sport=${board.sport||"football"}`;
    }
    const r=await fetch(url);
    const d=await r.json();
    if(board.auto && d.pick) autoPick=d.pick;
    loadMatchupLogos(d.game);
    if(d.ok&&d.game){
      const prev=game;
      game=d.game;
      const mine = game.pinned_side==="home" ? game.home : game.away;
      const was  = prev && prev.pinned_side===game.pinned_side
                 ? (prev.pinned_side==="home" ? prev.home : prev.away) : null;
      // "Auto" can hand back a different sport (even a different team)
      // than the last poll did - a same-team check alone isn't enough.
      const sameGame = was && was.abbr===mine.abbr && (!prev.sport || prev.sport===game.sport);
      const oth = game.pinned_side==="home" ? game.away : game.home;
      const sport = game.sport || "football";
      // 6 or more points at once is a touchdown; exactly 3 is a field goal
      // (its own, quieter animation); a lone extra point (1) or a 2-point
      // conversion on its own doesn't earn an animation. These five are
      // football-only - hockey/baseball/basketball get their own checks
      // further down so a flurry of hockey goals can't get mistaken for
      // a touchdown just because the score jumped by 6 between polls.
      if(sameGame && sport==="football" && typeof was.score==="number" && typeof mine.score==="number" &&
         mine.score-was.score>=6){
        loadLogo(mine.logo, lg => startFireworks(teamPalette(mine), mine.abbr, lg));
      } else if(sameGame && sport==="football" && typeof was.score==="number" && typeof mine.score==="number" &&
                mine.score-was.score===3){
        startFieldGoal(teamPalette(mine), mine.abbr);
      } else if(sameGame && sport==="football" && prev.state==="pre" && game.state==="in"){
        // the game just started
        startKickoff(teamPalette(mine), game.away.abbr, game.home.abbr);
      } else if(sameGame && (sport==="football" || sport==="basketball") &&
                game.state==="in" && prev.state==="in" &&
                prev.period && game.period && game.period > prev.period){
        // a quarter just ended - name the one that finished, not the new one
        // (football and basketball both have quarters and a halftime)
        const done = prev.period;
        const label = done===1 ? "END 1ST" : done===2 ? "HALFTIME"
                    : done===3 ? "END 3RD" : done===4 ? "END REG" : "END OT";
        startQuarter(label, game.away, game.home, teamPalette(mine));
      } else if(sameGame && sport==="football" && game.state==="in" && prev.state==="in" &&
                game.last_play_text && game.last_play_text!==prev.last_play_text &&
                /penalty/i.test(game.last_play_text)){
        // who it's on (in their colour) and the foul, shortened to fit
        const p = parsePenalty(game.last_play_text);
        const side = [game.home, game.away].find(s => s.abbr && p.team &&
                       s.abbr.toUpperCase() === p.team);
        startFlag(p.foul, p.team, side ? ledColor(side.color) : "rgb(255,255,255)");
      } else if(sameGame && sport==="football" && game.state==="in" && prev.state==="in" &&
                game.possession && game.possession===prev.possession &&
                game.possession===game.pinned_side &&
                firstDownNum(game.down_distance)===1 &&
                firstDownNum(prev.down_distance)>1){
        // my team drove it far enough to reset the down, not a kickoff reset
        startFirstDown(teamPalette(mine), mine.abbr);
      } else if(sameGame && sport==="hockey" && game.state==="in" &&
                game.intermission && !prev.intermission){
        // a period just ended: hockey's own words - 1ST / INTERMISSION,
        // 2ND / INTERMISSION, END OF / REGULATION, END OF / OT
        startQuarter("INTERMISSION", game.away, game.home, teamPalette(mine),
                     game.intermission_lines || ["END OF", game.period_label||""]);
      } else if(sameGame && sport==="hockey" && game.state==="in" &&
                typeof was.score==="number" && typeof mine.score==="number" &&
                mine.score>was.score){
        // every goal is one point, so a score bump of any size is a goal -
        // no ambiguity the way touchdown-vs-field-goal needs one
        loadLogo(mine.logo, lg => startGoalLight(teamPalette(mine), mine.abbr, lg));
      } else if(sameGame && sport==="baseball" && game.state==="in" &&
                typeof was.score==="number" && typeof mine.score==="number" &&
                mine.score>was.score){
        // Your team scored. If ESPN's last-play text says home run, the
        // full blast; otherwise (or if that text never shows up on this
        // feed) the lighter run notice - so a run never goes unmarked.
        const hr = game.last_play_text && game.last_play_text!==prev.last_play_text &&
                   /home run|homers|grand slam/i.test(game.last_play_text);
        const gs = hr && /grand slam/i.test(game.last_play_text);
        if(hr) loadLogo(mine.logo, lg => startHomeRun(teamPalette(mine), mine.abbr, lg, gs));
        else startRun(teamPalette(mine), shortAbbr(mine.abbr), mine.score-was.score);
      }
      if(sameGame && sport==="basketball" && game.state==="in" && !fx &&
         typeof was.score==="number" && typeof mine.score==="number" &&
         mine.score>was.score){
        // A three. ESPN's last-play text says so when it's there ("makes
        // 26-foot three point jumper"); if that text isn't on the feed,
        // exactly +3 between refreshes - with 5-second refreshes while
        // live, that's almost always a three.
        const txt=game.last_play_text||"", fresh=txt && txt!==prev.last_play_text;
        const three = fresh ? (/three point|three-point|3-pt|3pt/i.test(txt) && !/miss/i.test(txt))
                            : (mine.score-was.score===3);
        if(three) loadLogo(mine.logo, lg => startThree(teamPalette(mine), mine.abbr, lg));
      }
      // Ordinary baskets get no animation on purpose - a made basket
      // happens every 30-45 seconds and a touchdown-style blast every
      // time would be exhausting, not exciting.
      note.style.display="none";
      dot.classList.remove("bad");
      const m0 = game.pinned_side==="home" ? game.home : game.away;
      const other = game.pinned_side==="home" ? game.away : game.home;
      setDiag("pinned <b>" + (m0&&m0.abbr) + "</b> vs " + (other&&other.abbr)
              + " &middot; " + (m0 && m0.logo ? m0.logo : "no logo address"));
      const when=new Date().toLocaleTimeString([], {hour:"numeric",minute:"2-digit",second:"2-digit"});
      const n=(game.ticker_games||[]).length;
      statusText.innerHTML=`<b>${board.label}</b> &middot; ${n} other game${n===1?"":"s"} in the ticker &middot; updated ${when}`;
    } else {
      game=null;
      dot.classList.add("bad");
      statusText.textContent="Nothing to show for "+board.label;
      note.style.display="block";
      note.textContent=d.error||"No game found this week.";
    }
  }catch(e){
    dot.classList.add("bad");
    statusText.textContent="Can't reach the server";
    note.style.display="block";
    note.textContent="Is the Terminal window still running? Closing it stops the page updating.";
  }
  draw();
  scheduleNext();
}

function draw(){
  if(fx) return;            // fireworks own the panel while they run
  renderPixels(game,pair);
  paint();
}

setInterval(()=>{           // page through the ticker
  if(fx) return;
  pair++;draw();
},4000);
function teamPalette(side){
  // team's own two colours first, then gold / white / ember so the blast
  // stays colourful even for a team with two dark colours
  const pal=[];
  const a=hexRgb(side&&side.color), b=hexRgb(side&&side.color2);
  if(a) pal.push(a);
  if(b) pal.push(b);
  pal.push([255,215,0],[255,255,255],[255,110,30]);
  return pal;
}

document.getElementById("fxBtn").addEventListener("click",()=>{
  const mine = game && (game.pinned_side==="home" ? game.home : game.away);
  if(!mine || !mine.logo){
    note.style.display="block";
    note.textContent = "No logo URL came back from ESPN for this team, so the "
      + "blast shows the abbreviation instead.";
  }
  loadLogo(mine && mine.logo, lg => {
    if(mine && mine.logo && !lg){
      note.style.display="block";
      note.textContent = "Couldn't load the logo image ("+mine.logo+"). The Terminal "
        + "window will say which addresses it tried.";
    } else if(lg){
      note.style.display="none";
    }
    startFireworks(teamPalette(mine), mine ? mine.abbr : board.key, lg);
  });
});

function pinnedSide(){ return game && (game.pinned_side==="home" ? game.home : game.away); }

document.getElementById("kickBtn").addEventListener("click",()=>{
  const m = pinnedSide();
  startKickoff(teamPalette(m),
               game ? game.away.abbr : "AWAY", game ? game.home.abbr : "HOME");
});
document.getElementById("qtrBtn").addEventListener("click",()=>{
  const m = pinnedSide();
  startQuarter("END 1ST", game?game.away:{abbr:"AWAY",score:0},
               game?game.home:{abbr:"HOME",score:0}, teamPalette(m));
});
document.getElementById("halfBtn").addEventListener("click",()=>{
  const m = pinnedSide();
  startQuarter("HALFTIME", game?game.away:{abbr:"AWAY",score:0},
               game?game.home:{abbr:"HOME",score:0}, teamPalette(m));
});
document.getElementById("fgBtn").addEventListener("click",()=>{
  const m = pinnedSide();
  startFieldGoal(teamPalette(m), m ? m.abbr : "");
});
document.getElementById("flagBtn").addEventListener("click",()=>{
  // demo: a flag on the other team, run through the same parser as live
  const opp = game && (game.pinned_side==="home" ? game.away : game.home);
  const ab = opp && opp.abbr ? opp.abbr : "DAL";
  const p = parsePenalty(`PENALTY on ${ab}-J.Smith, Defensive Pass Interference, 15 yards, enforced at NYG 40.`);
  startFlag(p.foul, p.team, opp ? ledColor(opp.color) : "rgb(255,255,255)");
});
document.getElementById("firstDownBtn").addEventListener("click",()=>{
  const m = pinnedSide();
  startFirstDown(teamPalette(m), m ? m.abbr : "");
});
document.getElementById("goalBtn").addEventListener("click",()=>{
  const m = pinnedSide();
  loadLogo(m && m.logo, lg => startGoalLight(teamPalette(m), m ? m.abbr : "", lg));
});
document.getElementById("intBtn").addEventListener("click",()=>{
  const m = pinnedSide();
  startQuarter("INTERMISSION", game?game.away:{abbr:"NJ",score:1},
               game?game.home:{abbr:"NYR",score:2}, teamPalette(m), ["1ST","INTERMISSION"]);
});
document.getElementById("threeBtn").addEventListener("click",()=>{
  const m = pinnedSide();
  loadLogo(m && m.logo, lg => startThree(teamPalette(m), m ? m.abbr : "", lg));
});
document.getElementById("runBtn").addEventListener("click",()=>{
  const m = pinnedSide();
  startRun(teamPalette(m), m ? shortAbbr(m.abbr) : "", 2);
});
document.getElementById("homerunBtn").addEventListener("click",()=>{
  const m = pinnedSide();
  loadLogo(m && m.logo, lg => startHomeRun(teamPalette(m), m ? m.abbr : "", lg, false));
});

// (refreshing is scheduled by load() itself - see scheduleNext)
window.addEventListener("resize",paint);
const VERSION="__VERSION__";
function setDiag(msg){
  document.getElementById("diag").innerHTML =
    "<b>build</b> " + VERSION + " &nbsp;&middot;&nbsp; <b>logo</b> " + msg;
}
setDiag("waiting for a game\u2026");

const PHONE_URL="__PHONE_URL__";
if(PHONE_URL && !PHONE_URL.startsWith("__")){
  document.getElementById("phone").innerHTML =
    "<b>On your phone</b> \u2014 same wifi, open " + PHONE_URL;
}
load();
</script>
</body></html>
"""


LOGO_CACHE = {}


def save_logo_copy(url, data):
    """Keep a copy of each team logo the board fetches, in a
    scoreboard_logos folder next to this script, named after ESPN's path
    (nba_500-dark_ny.png). Lets the logo shrinking be tuned on the real
    files. Never sent anywhere."""
    try:
        folder = os.path.join(os.path.dirname(os.path.abspath(__file__)), "scoreboard_logos")
        os.makedirs(folder, exist_ok=True)
        tail = urlparse(url).path.split("/teamlogos/")[-1].replace("/", "_")
        with open(os.path.join(folder, tail or "logo.png"), "wb") as f:
            f.write(data)
    except Exception:
        pass


def save_last_event(data, team):
    """Keep a copy of the raw ESPN data for the game on the board, next to
    this script (scoreboard_last_event.json). It's what lets new features
    be checked against ESPN's real field names - playoff series, last-play
    text, baseball counts - instead of guesses. Overwritten every refresh,
    a few KB, never sent anywhere."""
    try:
        found = SB.find_event_state(data, team)
        if not found:
            return
        path = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                            "scoreboard_last_event.json")
        with open(path, "w") as f:
            json.dump(found[0], f, indent=1)
    except Exception:
        pass


def fetch_logo(url):
    """Pull a team logo from ESPN and hand it to the page.

    The browser fetches it from this server rather than from ESPN, which
    sidesteps cross-origin rules - the page needs to read the actual
    pixels to redraw them on the LED grid, and a canvas won't allow that
    for an image loaded straight off another site."""
    if url in LOGO_CACHE:
        return LOGO_CACHE[url]
    host = urlparse(url).hostname or ""
    if not host.endswith("espncdn.com"):
        return None                      # only ESPN's image host
    # ESPN's scoreboard logo looks like
    #   .../teamlogos/nfl/500/scoreboard/gb.png
    # and there are dark-background variants at related paths. Try the
    # dark ones first (better on a black panel), then the plain one.
    candidates = []
    if "/500/" in url:
        dark = url.replace("/500/", "/500-dark/")
        candidates.append(dark)
        if "/scoreboard/" in dark:
            candidates.append(dark.replace("/scoreboard/", "/"))
    candidates.append(url)
    if "/scoreboard/" in url:
        candidates.append(url.replace("/scoreboard/", "/"))

    for cand in candidates:
        try:
            req = urllib.request.Request(cand, headers={"User-Agent": "curl/8.0"})
            with urllib.request.urlopen(req, timeout=10) as r:
                data = r.read()
            if data:
                print(f"  logo ok: {cand} ({len(data)} bytes)")
                LOGO_CACHE[url] = data
                save_logo_copy(cand, data)
                return data
        except Exception as exc:
            print(f"  logo miss: {cand} -> {exc}")
            continue
    print(f"  logo FAILED for {url}")
    LOGO_CACHE[url] = None
    return None


class Handler(BaseHTTPRequestHandler):
    def log_message(self, *args):
        pass  # keep the Terminal window quiet

    def _send(self, body, ctype):
        self.send_response(200)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        u = urlparse(self.path)
        if u.path in ("/", "/index.html"):
            self._send(PAGE.encode("utf-8"), "text/html; charset=utf-8")
            return
        if u.path == "/api/version":
            self._send(json.dumps({"version": VERSION}).encode(), "application/json")
            return
        if u.path == "/api/logo":
            q = parse_qs(u.query)
            src = (q.get("u") or [""])[0]
            data = fetch_logo(src) if src else None
            if not data:
                self.send_response(404)
                self.end_headers()
                return
            self._send(data, "image/png")
            return
        if u.path == "/api/game":
            q = parse_qs(u.query)
            auto = (q.get("auto") or ["0"])[0] == "1"
            try:
                if auto:
                    sport, data, team, college = SB.auto_pick(SB.fetch)
                else:
                    sport = (q.get("sport") or ["football"])[0]
                    team = (q.get("team") or ["NYG"])[0]
                    college = (q.get("college") or ["0"])[0] == "1"
                    url = {
                        "football": SB.CFB_URL if college else SB.NFL_URL,
                        "baseball": SB.MLB_URL,
                        "hockey": SB.NHL_URL,
                        "basketball": SB.NBA_URL,
                    }[sport]
                    # off day? the next game; season over? the last one
                    data, _when = SB.find_team_feed(SB.fetch, url, team, sport)
                game = SB.parse_live(data, team, sport=sport, top25=college)
                save_last_event(data, team)
                if game:
                    SB.enrich(game, SB.fetch, team)
                    for side in ("home", "away"):
                        print(f"  {game[side]['abbr']}: logo={game[side].get('logo')}")
                    # which team/sport this is, so in Auto the page can
                    # follow a live game quickly without re-checking all five
                    payload = {"ok": True, "game": game,
                               "pick": {"team": team, "sport": sport,
                                        "college": 1 if college else 0}}
                else:
                    payload = {"ok": False,
                               "error": f"No {team} game today, in the next "
                                        f"{SB.AHEAD_DAYS} days, or in the last "
                                        f"{SB.BACK_DAYS}. Could be a bye week, "
                                        "or the season's over."}
            except Exception as exc:
                payload = {"ok": False,
                           "error": f"Couldn't reach ESPN: {exc}"}
            self._send(json.dumps(payload).encode("utf-8"),
                       "application/json; charset=utf-8")
            return
        self.send_response(404)
        self.end_headers()


def lan_ip():
    """This Mac's address on your home network. Opening a UDP socket
    doesn't send anything - it just asks the OS which interface it would
    use, which is the address your phone needs."""
    import socket
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect(("8.8.8.8", 80))
        return s.getsockname()[0]
    except Exception:
        return None
    finally:
        s.close()


def main():
    url = f"http://localhost:{PORT}/"
    ip = lan_ip()
    phone_url = f"http://{ip}:{PORT}/" if ip else ""

    global PAGE
    PAGE = PAGE.replace("__PHONE_URL__", phone_url).replace("__VERSION__", VERSION)

    def start(port):
        # 0.0.0.0 so other devices on your wifi can reach it too
        return HTTPServer(("0.0.0.0", port), Handler)

    server = None
    try:
        server = start(PORT)
    except OSError:
        # An earlier copy is still holding the port. That is the thing most
        # likely to make an update look like it did nothing: this process
        # would quit, the OLD one would keep serving, and the browser would
        # show yesterday's code no matter how many times you restarted.
        print(f"Port {PORT} is busy - an older scoreboard is still running.")
        print("Stopping it...")
        try:
            out = subprocess.run(["lsof", "-ti", f"tcp:{PORT}"],
                                 capture_output=True, text=True, timeout=5).stdout
            mine = str(os.getpid())
            for pid in [x for x in out.split() if x and x != mine]:
                try:
                    os.kill(int(pid), signal.SIGTERM)
                    print(f"  stopped process {pid}")
                except Exception as exc:
                    print(f"  couldn't stop {pid}: {exc}")
            time.sleep(1.2)
            server = start(PORT)
            print("  port freed, carrying on.")
        except Exception:
            server = None

        if server is None:
            # Couldn't clear it - use the next free port rather than quitting,
            # so you are never left talking to the stale one.
            for alt in range(PORT + 1, PORT + 12):
                try:
                    server = start(alt)
                    globals()["PORT"] = alt
                    url = f"http://localhost:{alt}/"
                    if ip:
                        phone_url = f"http://{ip}:{alt}/"
                    print(f"  couldn't stop it; using port {alt} instead.")
                    break
                except OSError:
                    continue
        if server is None:
            print("Couldn't find a free port. Quit Terminal entirely and try again.")
            return

    print(f"Scoreboard running  ({VERSION})")
    print(f"  This Mac:   {url}")
    if phone_url:
        print(f"  Your phone: {phone_url}")
        print("  (phone has to be on the same wifi)")
    print()
    print("Your browser should open by itself. Press Control-C here to stop.")
    print("macOS may ask whether to allow incoming connections - say Allow,")
    print("or the phone address won't work.")
    threading.Timer(0.6, lambda: webbrowser.open(url)).start()
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nStopped.")
        server.server_close()


if __name__ == "__main__":
    main()
