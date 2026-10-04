// Runs the preview page's renderer headless and dumps games + pixels.
global.performance = { now: () => 0 };
global.requestAnimationFrame = () => {};
const el = () => ({ addEventListener(){}, appendChild(){}, style:{}, dataset:{}, children:[],
  classList:{add(){},remove(){}}, setAttribute(){}, innerHTML:"", textContent:"",
  getContext: () => ({fillRect(){},beginPath(){},arc(){},fill(){}}), clientWidth:512 });
global.document = { getElementById: el, createElement: el };
global.window = { devicePixelRatio: 1, addEventListener(){} };
global.setInterval = () => 0; global.setTimeout = () => 0;
global.fetch = async () => ({ json: async () => ({ ok:false }) });
global.Image = function(){};
const fs = require('fs');
let js = fs.readFileSync('/tmp/page.js','utf8').replace(/\nload\(\);\s*$/m, '\n');
js += `
const cases = {};
const clone = o => JSON.parse(JSON.stringify(o));
for (const k of Object.keys(DEMOS)) cases[k] = clone(DEMOS[k].game);
let g = clone(DEMOS.college.game);
g.state='pre'; g.kickoff_local='10/4 3:30P'; g.home.score=null; g.away.score=null; g.home.record='10-2'; g.home.rank=12;
cases.college_pre = g;
g = clone(DEMOS.hockey.game); g.pp={side:'away',time:'0:47'}; g.clock='12:07'; g.period_label='1ST'; cases.hockey_pk = g;
g = clone(DEMOS.hockey.game); g.intermission=true; g.intermission_left='14:32'; g.pp=null; cases.hockey_int = g;
g = clone(DEMOS.college.game); g.down_distance='1st & Goal at TENN 4'; g.redzone=true; g.possession='away'; cases.college_rz = g;
g = clone(DEMOS.baseball.game); g.state='post'; g.final_label='FINAL 10/1'; g.home.score=6; g.away.score=12;
  g.playoff={round:'ALDS GAME 3',summary:'NYY LEADS 2-1',wins:{home:2,away:1}}; cases.mlb_post_po = g;
g = clone(DEMOS.playoffs.game); g.playoff.wins={home:0,away:0}; cases.playoffs_g1 = g;
g = clone(DEMOS.college.game); g.state='pre'; g.kickoff_local='8/30 7:00P'; g.home.score=null; g.away.score=null;
  g.preseason=true; cases.preseason_pre = g;
g = clone(DEMOS.baseball.game); g.half='MID'; g.inning_text='MID 7TH'; cases.mlb_mid = g;
g = clone(DEMOS.basketball.game); g.home.score=99; g.bonus={home:false,away:true}; cases.nba_nobonus = g;
const out = {};
for (const [name, gg] of Object.entries(cases)) for (const pair of [0,1,2]) {
  renderPixels(clone(gg), pair);
  out[name+'@'+pair] = { game: gg, pair, px: px.map(c => c ? c : null) };
}
fs.writeFileSync(process.argv[2] || '/tmp/js_frames.json', JSON.stringify(out));
console.log('cases', Object.keys(out).length);
process.exit(0);
`;
eval(js);
