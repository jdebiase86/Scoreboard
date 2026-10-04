#include "shim/Arduino.h"
#include "../scoreboard/sb_teams.h"
#include <stdio.h>
static const int MAX_PICKS=8;
struct TzDef { const char* label; const char* posix; };
static const TzDef TZS[] = {{"Eastern",""},{"Central",""},{"Mountain",""},{"Arizona",""},{"Pacific",""}};
static const int NTZ=5;
struct BrightDef { const char* label; uint8_t level; };
static const BrightDef BRIGHTS[] = {{"Low", 35}, {"Medium", 70}, {"High", 120}, {"Max", 180}};
static const int NBRIGHT=4;
struct { String ssid; int picks[8]; int npicks; int pin; int tz; int bright; } settings;
static const char* const LKEY[L_COUNT] = {"NFL", "CFB", "MLB", "NHL", "NBA"};
String teamKey(int i){ return String(LKEY[TEAMS[i].league]) + ":" + TEAMS[i].abbr; }
static bool apOn=true; static String scanned; static String SENT;
static String esc(const String& s) {
  String o;
  for (unsigned i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '&') o += "&amp;"; else if (c == '<') o += "&lt;"; else if (c == '>') o += "&gt;";
    else if (c == '"') o += "&quot;"; else o += c;
  }
  return o;
}

static const char PAGE_HEAD[] PROGMEM = R"HTML(<!doctype html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Scoreboard setup</title><style>
:root{color-scheme:light dark;--bg:#f2f1ee;--card:#fff;--ink:#17181b;--soft:#5f636b;--line:#d8d4cc;--acc:#c0650f}
@media(prefers-color-scheme:dark){:root{--bg:#111316;--card:#1a1d22;--ink:#eceef1;--soft:#a2a7b0;--line:#2d3139;--acc:#eda040}}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--ink);font:16px/1.45 -apple-system,system-ui,sans-serif}
main{max-width:560px;margin:0 auto;padding:20px 16px 48px}h1{font-size:26px;margin:4px 0 2px}
p.sub{color:var(--soft);margin:0 0 18px}
section{background:var(--card);border:1px solid var(--line);border-radius:12px;padding:16px;margin:0 0 14px}
h2{font-size:17px;margin:0 0 4px}.hint{color:var(--soft);font-size:14px;margin:0 0 12px}
label.f{display:block;font-size:14px;color:var(--soft);margin:10px 0 4px}
input[type=text],input[type=password],select{width:100%;font:inherit;padding:11px 12px;border:1px solid var(--line);border-radius:8px;background:var(--bg);color:var(--ink)}
details{border-top:1px solid var(--line);padding:10px 0}details:first-of-type{border-top:0}
summary{cursor:pointer;font-weight:600}summary .n{color:var(--acc);font-weight:600;margin-left:6px}
.teams{display:grid;grid-template-columns:1fr 1fr;gap:2px 10px;margin-top:8px}
.teams label{display:flex;gap:8px;align-items:center;padding:6px 0;font-size:15px}
input[type=checkbox]{width:20px;height:20px;accent-color:var(--acc)}
.picked{font-size:14px;color:var(--soft);min-height:20px}
button{width:100%;font-family:inherit;font-weight:600;font-size:17px;padding:15px;border:0;border-radius:10px;background:var(--acc);color:#fff;margin-top:6px}
.row{display:flex;gap:8px;align-items:center;margin-top:8px;font-size:14px;color:var(--soft)}
</style></head><body><main>
)HTML";

static String teamSection() {
  String h;
  for (int lg = 0; lg < L_COUNT; lg++) {
    int count = 0;
    String items;
    for (int i = 0; i < NTEAMS; i++) {
      if (TEAMS[i].league != lg) continue;
      bool on = false;
      for (int k = 0; k < settings.npicks; k++) if (settings.picks[k] == i) on = true;
      if (on) count++;
      items += "<label><input type=checkbox name=t value=\"" + esc(teamKey(i)) + "\" data-n=\"" + esc(TEAMS[i].name) +
               "\"" + (on ? " checked" : "") + ">" + esc(TEAMS[i].name) + "</label>";
    }
    h += String("<details") + (count ? " open" : "") + "><summary>" + LEAGUE_NAMES[lg] +
         "<span class=n>" + (count ? String(count) + " picked" : String("")) + "</span></summary><div class=teams>" +
         items + "</div></details>";
  }
  return h;
}

static void handleRoot() {
  bool home = !apOn;
  String h = FPSTR(PAGE_HEAD);
  h += "<h1>Scoreboard</h1><p class=sub>";
  h += home ? "Settings. Changes show up on the board within a few seconds."
            : "Two quick steps and the board starts showing scores.";
  h += "</p><form method=post action=/save>";

  // Wi-Fi
  if (!home) {
    h += "<section><h2>1. Your home Wi-Fi</h2><p class=hint>The network the scoreboard should use every day.</p>"
         "<label class=f>Network name</label><input type=text name=ssid list=nets autocomplete=off autocapitalize=none "
         "required value=\"" + esc(settings.ssid) + "\"><datalist id=nets>" + scanned + "</datalist>"
         "<label class=f>Password</label><input type=password name=pass id=pw value=\"\" autocomplete=off>"
         "<div class=row><input type=checkbox id=show onclick=\"pw.type=this.checked?'text':'password'\">"
         "<label for=show>Show password</label></div></section>";
  } else {
    h += "<section><h2>Wi-Fi</h2><p class=hint>On <b>" + esc(settings.ssid) + "</b>.</p>"
         "<details><summary>Change Wi-Fi network</summary>"
         "<label class=f>Network name</label><input type=text name=ssid autocomplete=off autocapitalize=none>"
         "<label class=f>Password</label><input type=password name=pass autocomplete=off>"
         "<p class=hint>Leave blank to stay on the current network.</p></details></section>";
  }

  // teams
  h += String("<section><h2>") + (home ? "Teams" : "2. Your teams") +
       "</h2><p class=hint>Up to 8. In Auto, a live football game always wins, then baseball, hockey, basketball. "
       "With nothing live, it shows whoever plays next.</p><div class=picked id=picked></div>";
  h += teamSection();
  h += "<label class=f>What the board shows</label><select name=pin id=pin><option value=\"\">Auto (recommended)</option>";
  for (int k = 0; k < settings.npicks; k++) {
    int i = settings.picks[k];
    h += "<option value=\"" + esc(teamKey(i)) + "\"" + (settings.pin == i ? " selected" : "") + ">Always " +
         esc(TEAMS[i].name) + "</option>";
  }
  h += "</select></section>";

  // time zone + brightness
  h += "<section><h2>Board</h2><label class=f>Time zone (for game times)</label><select name=tz>";
  for (int i = 0; i < NTZ; i++)
    h += "<option value=" + String(i) + (settings.tz == i ? " selected" : "") + ">" + TZS[i].label + "</option>";
  h += "</select><label class=f>Brightness</label><select name=bright>";
  for (int i = 0; i < NBRIGHT; i++)
    h += "<option value=" + String(i) + (settings.bright == i ? " selected" : "") + ">" + BRIGHTS[i].label + "</option>";
  h += "</select></section><button type=submit>Save</button></form>";

  h += R"JS(<script>
const boxes=[...document.querySelectorAll('input[name=t]')],pin=document.getElementById('pin'),picked=document.getElementById('picked');
function upd(){const on=boxes.filter(b=>b.checked);
 picked.textContent=on.length?('Picked: '+on.map(b=>b.dataset.n).join(', ')):'Pick at least one team.';
 boxes.forEach(b=>b.disabled=!b.checked&&on.length>=8);
 document.querySelectorAll('details').forEach(d=>{const n=d.querySelector('.n');if(!n)return;
  const c=[...d.querySelectorAll('input[name=t]')].filter(b=>b.checked).length;n.textContent=c?c+' picked':'';});
 const cur=pin.value;pin.length=1;on.forEach(b=>{const o=new Option('Always '+b.dataset.n,b.value);pin.add(o);if(b.value===cur)o.selected=true;});}
boxes.forEach(b=>b.addEventListener('change',upd));upd();
document.querySelector('form').addEventListener('submit',e=>{if(!boxes.some(b=>b.checked)){e.preventDefault();alert('Pick at least one team.');}});
</script></main></body></html>)JS";
  SENT = h;
}


int main(int argc,char**argv){
  apOn = argc>1 && argv[1][0]=='a';
  settings.ssid = apOn ? "" : "DeBiase Home";
  settings.npicks=0; settings.pin=-1; settings.tz=0; settings.bright=1;
  if(!apOn){ const char* k[]={"NYG","FLA","NYY","NYR","NY"}; int lg[]={0,1,2,3,4};
    for(int j=0;j<5;j++) for(int i=0;i<NTEAMS;i++) if(TEAMS[i].league==lg[j]&&!strcmp(TEAMS[i].abbr,k[j])) settings.picks[settings.npicks++]=i; }
  scanned = "<option value=\"DeBiase Home\"><option value=\"Neighbor 5G\">";
  handleRoot(); fputs(SENT.c_str(), stdout);
}
