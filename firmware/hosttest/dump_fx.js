// Runs the preview's animations headless with a seeded random and a fixed
// 60 fps clock, and dumps frames for comparison with the firmware's port.
let NOW = 1000;
global.performance = { now: () => NOW };
global.requestAnimationFrame = () => {};
const el = () => ({ addEventListener(){}, appendChild(){}, style:{}, dataset:{}, children:[],
  classList:{add(){},remove(){}}, setAttribute(){}, innerHTML:"", textContent:"",
  getContext: () => ({fillRect(){},beginPath(){},arc(){},fill(){}}), clientWidth:512 });
global.document = { getElementById: el, createElement: el };
global.window = { devicePixelRatio: 1, addEventListener(){} };
global.setInterval = () => 0; global.setTimeout = () => 0;
global.fetch = async () => ({ json: async () => ({ ok:false }) });
global.Image = function(){};
function mulberry32(a){return function(){a|=0;a=a+0x6D2B79F5|0;var t=Math.imul(a^a>>>15,1|a);t=t+Math.imul(t^t>>>7,61|t)^t;return((t^t>>>14)>>>0)/4294967296;}}
const fs = require('fs');
let js = fs.readFileSync('/tmp/page.js','utf8').replace(/\nload\(\);\s*$/m, '\n');
js += `
const PAL = [[0,51,160],[200,16,46],[255,215,0],[255,255,255],[255,110,30]];
const PAL2 = [[255,130,0],[255,255,255],[255,215,0],[255,255,255],[255,110,30]];
// a fake 40x30 logo: ring of colours, deterministic
const LOGO = {w:40, h:30, pix:[]};
for (let y=0;y<30;y++) for (let x=0;x<40;x++) { const d=(x-20)*(x-20)/400+(y-15)*(y-15)/225;
  if (d<1 && d>0.3) LOGO.pix.push([x,y,(x*7)%256,(y*9)%256,200]); }
const cases = {
  td_logo:   () => startFireworks(PAL, "NYG", LOGO),
  td_text:   () => startFireworks(PAL2, "TENN", null),
  kick:      () => startKickoff(PAL, "DAL", "NYG"),
  qtr:       () => startQuarter("END 1ST", {abbr:"DAL",score:7,color:"002a5c"}, {abbr:"NYG",score:10,color:"003c7f"}, PAL),
  half:      () => startQuarter("HALFTIME", {abbr:"DAL",score:7,color:"002a5c"}, {abbr:"NYG",score:10,color:"003c7f"}, PAL),
  int1:      () => startQuarter("INTERMISSION", {abbr:"NJ",score:1,color:"ce1126"}, {abbr:"NYR",score:2,color:"0038a8"}, PAL, ["1ST","INTERMISSION"]),
  intreg:    () => startQuarter("INTERMISSION", {abbr:"NJ",score:2,color:"ce1126"}, {abbr:"NYR",score:2,color:"0038a8"}, PAL, ["END OF","REGULATION"]),
  fg:        () => startFieldGoal(PAL, "NYG"),
  flag:      () => { const p = parsePenalty("PENALTY on DAL-J.Smith, Defensive Pass Interference, 15 yards, enforced at NYG 40."); startFlag(p.foul, p.team, ledColor("002a5c")); },
  first:     () => startFirstDown(PAL, "NYG"),
  goal_logo: () => startGoalLight(PAL, "NYR", LOGO),
  goal_text: () => startGoalLight(PAL, "NYR", null),
  run1:      () => startRun(PAL, "NYY", 1),
  run3:      () => startRun(PAL, "NYY", 3),
  hr_logo:   () => startHomeRun(PAL, "NYY", LOGO, false),
  gs_text:   () => startHomeRun(PAL, "NYY", null, true),
  three_logo:() => startThree(PAL, "NY", LOGO),
  three_text:() => startThree(PAL2, "NY", null),
};
const out = {logo: LOGO, pal: PAL, pal2: PAL2, cases: {}};
const pens = ["PENALTY on DAL-J.Smith, Defensive Pass Interference, 15 yards, enforced at NYG 40.",
  "J.Doe pass incomplete. PENALTY on NYG-T.Lee, Offensive Holding, 10 yards",
  "PENALTY on PHI, Delay of Game, 5 yards", "Penalty on KC-X.Y, Unnecessary Roughness, 15 yards",
  "PENALTY on GB-A.B, Roughing the Passer, 15 yards", "Too many flags PENALTY, False Start",
  "PENALTY on NE-C.D, Lowering the Head to Initiate Contact, 15 yards"];
out.penalties = pens.map(t => [t, parsePenalty(t)]);
for (const [name, go] of Object.entries(cases)) {
  Math.random = mulberry32(12345);
  NOW = 1000; fx = null;
  go();
  const frames = [];
  for (let k = 1; k <= 300; k++) {
    NOW = 1000 + k * 1000 / 60;
    const elv = NOW - 1000;
    if (elv >= 5000) break;
    stepFireworks(NOW);
    if (k % 3 === 0 || k < 12) frames.push({el: elv, px: px.map(c => c ? c : null)});
  }
  out.cases[name] = frames;
}
fs.writeFileSync('/tmp/fx_frames.json', JSON.stringify(out));
console.log('cases', Object.keys(out.cases).length, 'frames', Object.values(out.cases).reduce((a,f)=>a+f.length,0));
process.exit(0);
`;
eval(js);
