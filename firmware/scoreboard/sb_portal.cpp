#include "sb_portal.h"
#include "sb_settings.h"
#include "sb_net.h"
#include "sb_panel.h"
#include <WiFi.h>
#include <WebServer.h>
#include "sb_dns.h"
#include <ESPmDNS.h>
#include "sb_log.h"
#include "sb_version.h"

static WebServer server(80);
static CaptiveDns dns;
static bool apOn = false, started = false;
static String scanned;   // <option>s of nearby networks
volatile bool portalWifiSaved = false;
volatile uint32_t portalSavedAt = 0;
volatile uint32_t portalUsedAt = 0;

static String esc(const String& s) {
  String o;
  for (unsigned i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '&') o += "&amp;"; else if (c == '<') o += "&lt;"; else if (c == '>') o += "&gt;";
    else if (c == '"') o += "&quot;"; else o += c;
  }
  return o;
}

static void scanNetworks() {
  scanned = "";
  int n = WiFi.scanNetworks();
  for (int i = 0; i < n && i < 25; i++) {
    String s = WiFi.SSID(i);
    if (!s.length() || scanned.indexOf("\"" + esc(s) + "\"") >= 0) continue;
    scanned += "<option value=\"" + esc(s) + "\">";
  }
  WiFi.scanDelete();
}

static const char PAGE_HEAD[] PROGMEM = R"HTML(<!doctype html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Scoreboard setup</title><style>
:root{color-scheme:light dark;--bg:#f2f1ee;--card:#fff;--ink:#17181b;--soft:#5f636b;--line:#d8d4cc;--acc:#c0650f}
@media(prefers-color-scheme:dark){:root{--bg:#111316;--card:#1a1d22;--ink:#eceef1;--soft:#a2a7b0;--line:#2d3139;--acc:#eda040}}
main>form{margin-bottom:14px}
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
.fx{display:grid;grid-template-columns:1fr 1fr;gap:8px}.fx button{font-size:15px;padding:12px 6px;margin:0}
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
    h += "<section><h2>Test the animations</h2><p class=hint>Plays one on the board now, with the game "
         "that's on it (or made-up teams). <span id=fxr></span></p><div class=fx>";
    static const char* const FX[][2] = {
        {"touchdown", "Touchdown"}, {"fieldgoal", "Field goal"}, {"kickoff", "Kickoff"}, {"quarter", "End of quarter"},
        {"halftime", "Halftime"}, {"flag", "Flag"}, {"firstdown", "1st down"}, {"goal", "Goal (hockey)"},
        {"intermission", "Intermission"}, {"run", "Run scores"}, {"homerun", "Home run"}, {"grandslam", "Grand slam"},
        {"three", "3-pointer"}, {"win", "Win card"}};
    for (auto& f : FX) h += String("<button type=button data-k=") + f[0] + ">" + f[1] + "</button>";
    h += "</div></section>";
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
document.querySelectorAll('[data-k]').forEach(b=>b.onclick=()=>{const r=document.getElementById('fxr');
 fetch('/fxtest?k='+b.dataset.k,{method:'POST'}).then(x=>{r.textContent=x.ok?b.textContent+' sent.':'Not sent.';}).catch(()=>{r.textContent='Not sent.';});});
document.querySelector('form').addEventListener('submit',e=>{if(!boxes.some(b=>b.checked)){e.preventDefault();alert('Pick at least one team.');}});
</script></main></body></html>)JS";
  server.send(200, "text/html; charset=utf-8", h);
  sbLog("page / sent to %s: %u bytes, %lu ms", server.client().remoteIP().toString().c_str(), h.length(),
        (unsigned long)(millis() - t0));
}

static void handleSave() {
  portalUsedAt = millis();
  String teams;
  for (int i = 0; i < server.args(); i++)
    if (server.argName(i) == "t") { if (teams.length()) teams += ","; teams += server.arg(i); }
  settings.setPicksFromString(teams);
  String pk = server.arg("pin");
  settings.pin = pk.length() ? findTeam(pk.c_str()) : -1;
  settings.tz = constrain(server.arg("tz").toInt(), 0, NTZ - 1);
  settings.bright = constrain(server.arg("bright").toInt(), 0, NBRIGHT - 1);
  if (server.hasArg("rot")) settings.rot = constrain(server.arg("rot").toInt(), 0, NROTATE - 1);
  bool clockWas = settings.clockA;
  if (server.hasArg("clk")) settings.clockA = server.arg("clk") != "0";
  bool clockChanged = clockWas != settings.clockA;
  String ssid = server.arg("ssid");
  ssid.trim();
  bool wifiChanged = false;
  if (ssid.length()) {
    String pass = server.arg("pass");
    if (!apOn && ssid == settings.ssid && !pass.length()) pass = settings.pass;   // left blank: keep it
    wifiChanged = ssid != settings.ssid || pass != settings.pass;
    settings.ssid = ssid;
    settings.pass = pass;
  }
  settings.save();
  setenv("TZ", TZS[settings.tz].posix, 1);
  tzset();

  String h = FPSTR(PAGE_HEAD);
  if (clockChanged && !wifiChanged && !apOn) {
    h += "<h1>Saved</h1><section><p>The board is restarting to change the panel timing. Give it 20 seconds.</p>"
         "<p><a href=/>Back to settings</a></p></section></main></body></html>";
    server.send(200, "text/html; charset=utf-8", h);
    portalWifiSaved = true;   // same path: restart shortly
    portalSavedAt = millis();
  } else if (wifiChanged || apOn) {
    h += "<h1>Saved</h1><section><p>The scoreboard is joining <b>" + esc(settings.ssid) +
         "</b> now. Watch the board: it says <b>CONNECTED</b> when it's on, and scores follow a few seconds later.</p>"
         "<p class=hint>You can close this page. Your phone will go back to your normal Wi-Fi on its own.</p>"
         "<p class=hint>If the board says it can't join, the network name or password was off. Join "
         "the Scoreboard network again and re-enter them.</p></section></main></body></html>";
    server.send(200, "text/html; charset=utf-8", h);
    portalWifiSaved = true;
    portalSavedAt = millis();
  } else {
    h += "<h1>Saved</h1><section><p>The board is updating now.</p><p><a href=/>Back to settings</a></p></section></main></body></html>";
    server.send(200, "text/html; charset=utf-8", h);
    panelBrightness(BRIGHTS[settings.bright].level);
    netKick();
  }
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


static void routes() {
  if (started) return;
  started = true;
  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/hotspot-detect.html", HTTP_GET, appleProbe);
  server.on("/library/test/success.html", HTTP_GET, appleProbe);
  for (const char* p : {"/generate_204", "/gen_204", "/connecttest.txt", "/ncsi.txt", "/redirect", "/canonical.html",
                        "/success.txt"})
    server.on(p, HTTP_GET, [] { if (apOn) captive(); else server.send(404, "text/plain", ""); });
  server.on("/update", HTTP_POST, [] {
    otaRequest();
    String h = FPSTR(PAGE_HEAD);
    h += "<h1>Checking</h1><section><p>The board is checking GitHub now. If there's a new version, the board "
         "shows UPDATING and restarts by itself in a minute or two. If not, nothing changes.</p>"
         "<p><a href=/>Back to settings</a> (the result shows under Software)</p></section></main></body></html>";
    server.send(200, "text/html; charset=utf-8", h);
  });
  server.on("/fxtest", HTTP_POST, [] {
    bool ok = !apOn && fxTest(server.arg("k").c_str());
    server.send(ok ? 200 : 400, "text/plain", ok ? "ok" : "no");
  });
  server.on("/log", HTTP_GET, [] { server.send(200, "text/plain; charset=utf-8", sbLogText()); });
  server.on("/favicon.ico", HTTP_GET, [] { server.send(404, "text/plain", ""); });
  server.onNotFound([] {
    if (apOn) captive();
    else server.send(404, "text/plain", "Not found");
  });
  server.begin();
}

void portalStartAP(const String& apName) {
  WiFi.mode(WIFI_AP_STA);
  scanNetworks();
  WiFi.softAP(apName.c_str());
  delay(200);
  sbLog("setup network %s up at %s", apName.c_str(), WiFi.softAPIP().toString().c_str());
  dns.begin((uint32_t)WiFi.softAPIP());
  apOn = true;
  routes();
}

void portalStopAP() {
  if (!apOn) return;
  dns.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  apOn = false;
}

void portalStartHome() {
  if (MDNS.begin("scoreboard")) MDNS.addService("http", "tcp", 80);
  routes();
}

void portalLoop() {
  if (apOn) dns.process();
  if (started) server.handleClient();
}

bool portalAPRunning() { return apOn; }
