#include "shim/Arduino.h"
#include "../scoreboard/sb_teams.h"
#include <stdio.h>
struct TzDef { const char* label; const char* posix; };
static const TzDef TZS[] = {{"Eastern",""},{"Central",""},{"Mountain",""},{"Arizona",""},{"Pacific",""}};
static const int NTZ=5;
struct BrightDef { const char* label; uint8_t level; };
static const BrightDef BRIGHTS[] = {{"Low", 35}, {"Medium", 70}, {"High", 120}, {"Max", 180}};
static const int NBRIGHT=4;
static const int ROTATE_SECS[] = {30, 60, 120};
static const char* const ROTATE_LABELS[] = {"30 seconds", "1 minute", "2 minutes"};
static const int NROTATE = 3;
#define FW_VERSION "1.3"
static String otaStatus(){ return "8:41 PM: up to date (1.3)"; }
struct { int rot; String ssid; int picks[8]; int npicks; int pin; int tz; int bright; bool clockA; } settings;
static const char* const LKEY[L_COUNT] = {"NFL", "CFB", "MLB", "NHL", "NBA"};
String teamKey(int i){ return String(LKEY[TEAMS[i].league]) + ":" + TEAMS[i].abbr; }
static volatile unsigned long portalUsedAt; static unsigned long millis(){return 1;}
struct IP { String toString(){ return "192.168.4.1"; } };
struct { IP softAPIP(){ return IP(); } } WiFi;
struct Cl { IP remoteIP(){ return IP(); } };
struct Server { int code=0; String body, last; void send(int c,const char*,const String& b){code=c;body=b;} void sendHeader(const char*,const String&,bool=false){} Cl client(){return Cl();} String uri(){return "/hotspot-detect.html";} } server;
static void sbLog(const char* f, ...) {}
static bool apOn=true; static String scanned;
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
input[type=checkbox]{flex:none;width:20px;height:20px;accent-color:var(--acc)}
.picked{font-size:14px;color:var(--soft);min-height:20px}
button{width:100%;font-family:inherit;font-weight:600;font-size:17px;padding:15px;border:0;border-radius:10px;background:var(--acc);color:#fff;margin-top:6px}
.row{display:flex;gap:8px;align-items:center;margin-top:8px;font-size:14px;color:var(--soft)}
</style></head><body><main>
)HTML";

// The team lists are built in the browser from a compact list, which keeps
// the page about a third the size (a big page is what a busy board struggles
// to send to a phone).
static String jsq(const char* t) {   // a JavaScript string literal
  String o = "\"";
  for (; *t; t++) { if (*t == '"' || *t == '\\') o += '\\'; o += *t; }
  return o + "\"";
}

static String teamSection() {
  String h = "<div id=leagues></div><script>const LN=[";
  for (int lg = 0; lg < L_COUNT; lg++) { if (lg) h += ","; h += jsq(LEAGUE_NAMES[lg]); }
  h += "],LK=[\"NFL\",\"CFB\",\"MLB\",\"NHL\",\"NBA\"],T=[";
  for (int i = 0; i < NTEAMS; i++) {
    if (i) h += ",";
    h += "[" + String((int)TEAMS[i].league) + "," + jsq(TEAMS[i].abbr) + "," + jsq(TEAMS[i].name) + "]";
  }
  h += "],P=[";
  for (int k = 0; k < settings.npicks; k++) { if (k) h += ","; h += jsq(teamKey(settings.picks[k]).c_str()); }
  h += R"JS(];
(()=>{const box=document.getElementById('leagues');
LN.forEach((ln,lg)=>{const d=document.createElement('details'),items=T.filter(t=>t[0]===lg);
 d.innerHTML='<summary>'+ln+'<span class=n></span></summary><div class=teams></div>';
 const g=d.querySelector('.teams');
 items.forEach(t=>{const k=LK[lg]+':'+t[1],l=document.createElement('label'),i=document.createElement('input');
  i.type='checkbox';i.name='t';i.value=k;i.dataset.n=t[2];i.checked=P.includes(k);if(i.checked)d.open=true;
  l.append(i,t[2]);g.append(l);});
 box.append(d);});})();
</script>)JS";
  return h;
}

static void handleRoot() {
  portalUsedAt = millis();
  uint32_t t0 = millis();
  bool home = !apOn;
  String h = FPSTR(PAGE_HEAD);
  h += "<h1>Scoreboard</h1><p class=sub>";
  h += home ? "Settings. Changes show up on the board within a few seconds."
            : "Two quick steps and the board starts showing scores.";
  h += "</p><form method=post action=/save>";

  // Wi-Fi
  if (!home) {
    h += "<section><h2>1. Your home Wi-Fi</h2><p class=hint>The network the scoreboard should use every day.</p>"
         "<label class=f>Network name</label><input type=text name=ssid list=nets autocomplete=off autocapitalize=none autocorrect=off spellcheck=false "
         "required value=\"" + esc(settings.ssid) + "\"><datalist id=nets>" + scanned + "</datalist>"
         "<label class=f>Password</label><input type=password name=pass id=pw value=\"\" autocomplete=off>"
         "<div class=row><input type=checkbox id=show onclick=\"pw.type=this.checked?'text':'password'\">"
         "<label for=show>Show password</label></div></section>";
  } else {
    h += "<section><h2>Wi-Fi</h2><p class=hint>On <b>" + esc(settings.ssid) + "</b>.</p>"
         "<details><summary>Change Wi-Fi network</summary>"
         "<label class=f>Network name</label><input type=text name=ssid autocomplete=off autocapitalize=none autocorrect=off spellcheck=false>"
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
  h += "</select><label class=f>Two of your teams playing at once (same sport): take turns every</label><select name=rot>";
  for (int i = 0; i < NROTATE; i++)
    h += "<option value=" + String(i) + (settings.rot == i ? " selected" : "") + ">" + ROTATE_LABELS[i] + "</option>";
  h += "</select><p class=hint>A team that scores jumps straight to the top.</p></section>";

  // time zone + brightness
  h += "<section><h2>Board</h2><label class=f>Time zone (for game times)</label><select name=tz>";
  for (int i = 0; i < NTZ; i++)
    h += "<option value=" + String(i) + (settings.tz == i ? " selected" : "") + ">" + TZS[i].label + "</option>";
  h += "</select><label class=f>Brightness</label><select name=bright>";
  for (int i = 0; i < NBRIGHT; i++)
    h += "<option value=" + String(i) + (settings.bright == i ? " selected" : "") + ">" + BRIGHTS[i].label + "</option>";
  h += "</select><details><summary>Panel timing</summary><p class=hint>Only change this if the picture looks "
       "shifted by one dot (a missing edge on the gold playoff frame). The board restarts to apply it.</p>"
       "<select name=clk><option value=1" + String(settings.clockA ? " selected" : "") + ">A (normal)</option>"
       "<option value=0" + String(settings.clockA ? "" : " selected") + ">B</option></select></details>";
  h += "</section><button type=submit>Save</button></form>";
  if (home) {
    h += "<section><h2>Software</h2><p class=hint>Version " FW_VERSION ". Updates install by themselves overnight. " +
         esc(otaStatus()) + "</p><form method=post action=/update><button type=submit>Check for updates now</button>"
         "</form></section>";
  }

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
  server.send(200, "text/html; charset=utf-8", h);
  sbLog("page / sent to %s: %u bytes, %lu ms", server.client().remoteIP().toString().c_str(), h.length(),
        (unsigned long)(millis() - t0));
}

// Phones probe these to detect a sign-in page; send them to ours
static void captive() {
  String url = String("http://") + WiFi.softAPIP().toString() + "/";
  server.sendHeader("Location", url, true);
  server.sendHeader("Cache-Control", "no-cache, no-store");
  server.send(302, "text/html", "<a href=\"" + url + "\">Scoreboard setup</a>");
}

// iPhones show whatever their probe gets back (anything but "Success" means
// "sign-in page"). Keep the answer TINY: phones repeat these checks in the
// background, and the board serves one request at a time - in v1.1 it sent
// the whole 23 KB setup page to every check and never got round to Safari.
static void appleProbe() {
  sbLog("phone sign-in check %s", server.uri().c_str());
  if (!apOn) {
    server.send(200, "text/html", "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>");
    return;
  }
  String url = String("http://") + WiFi.softAPIP().toString() + "/";
  server.sendHeader("Cache-Control", "no-cache, no-store");
  server.send(200, "text/html",
              "<!doctype html><html><head><meta name=viewport content=\"width=device-width\">"
              "<meta http-equiv=refresh content=\"0;url=" + url + "\"><title>Scoreboard</title></head>"
              "<body><a href=\"" + url + "\">Scoreboard setup</a></body></html>");
}


int main(int argc,char**argv){
  settings.ssid=""; settings.npicks=0; settings.pin=-1; settings.tz=0; settings.bright=1; settings.clockA=true;
  scanned = "<option value=\"HomeWiFi\">";
  handleRoot(); printf("root page: %u bytes\n", server.body.length());
  FILE* f=fopen("/tmp/page_v12.html","w"); fputs(server.body.c_str(),f); fclose(f);
  appleProbe(); printf("iPhone probe answer: %u bytes: %s\n", server.body.length(), server.body.c_str());
  captive(); printf("other probes: %d, %u bytes\n", server.code, server.body.length());
  apOn=false; handleRoot(); { FILE* f=fopen("/tmp/page_v13_home.html","w"); fputs(server.body.c_str(),f); fclose(f); } appleProbe(); printf("on home wifi probe: %s\n", server.body.c_str());
}
