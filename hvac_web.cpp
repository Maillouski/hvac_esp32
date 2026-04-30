#include "hvac_web.h"
#include "hvac_storage.h"
#include "config.h"
#include <WebServer.h>
#include <ArduinoJson.h>
#include <time.h>

static WebServer server(80);

static HvacConfig* g_cfg;
static HvacState*  g_state;
static Sensors*    g_sensors;
static GpioState*  g_gpio;
static Decisions*  g_decisions;

// ─────────────────────────────────────────────────────────────────
// Dashboard HTML (stocke en flash)
// ─────────────────────────────────────────────────────────────────
static const char HTML[] PROGMEM = R"HVAC(
<!DOCTYPE html>
<html lang="fr">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>HVAC Hangar</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{background:#0d1117;color:#c9d1d9;font-family:monospace;padding:12px;max-width:660px;margin:auto}
.card{background:#161b22;border:1px solid #30363d;border-radius:6px;padding:12px;margin-bottom:10px}
h1{color:#58a6ff;font-size:1.15em;display:inline}
.ts{color:#8b949e;font-size:.8em;float:right;padding-top:2px}
h2{color:#8b949e;font-size:.72em;text-transform:uppercase;letter-spacing:1px;margin-bottom:8px}
.sensors{display:grid;grid-template-columns:1fr 1fr;gap:8px;text-align:center}
.big{font-size:1.4em;font-weight:bold}
.lbl{font-size:.75em;color:#8b949e;margin-bottom:2px}
.devs{display:grid;grid-template-columns:1fr 1fr 1fr;gap:8px}
.dev{border-radius:4px;padding:8px;text-align:center}
.dev.on{background:#1a3626;border:1px solid #2ea043}
.dev.off{background:#2d1117;border:1px solid #6e2a2a}
.devname{font-size:.68em;color:#8b949e}
.devstat{font-size:1.1em;font-weight:bold}
.devstat.on{color:#2ea043}
.devstat.off{color:#f85149}
.devmode{font-size:.68em;color:#e3b341;min-height:14px}
.devreason{font-size:.62em;color:#8b949e;min-height:14px;margin-top:2px}
.acts{margin-top:6px;display:flex;flex-wrap:wrap;gap:3px;justify-content:center}
button{background:#21262d;border:1px solid #30363d;color:#c9d1d9;padding:4px 8px;
       border-radius:4px;cursor:pointer;font-family:monospace;font-size:.72em}
button:hover{background:#30363d}
button.red{background:#2d1117;border-color:#6e2a2a}
button.red:hover{background:#6e2a2a}
input[type=number]{width:44px;background:#0d1117;border:1px solid #30363d;
                   color:#c9d1d9;padding:3px 4px;border-radius:4px;
                   font-family:monospace;font-size:.72em}
.srow{display:grid;grid-template-columns:1fr 60px 28px;gap:6px;
      align-items:center;margin-bottom:5px;font-size:.82em}
.footer{color:#8b949e;font-size:.68em;margin-top:8px;text-align:center}
</style>
</head>
<body>
<div class="card">
  <h1>HVAC HANGAR</h1>
  <span class="ts" id="ts">--:--:--</span>
</div>

<div class="card">
  <h2>Capteurs</h2>
  <div class="sensors">
    <div>
      <div class="lbl">Interieur</div>
      <div class="big"><span id="Ti">--</span> &deg;C</div>
      <div class="big"><span id="Hi">--</span> %</div>
    </div>
    <div>
      <div class="lbl">Exterieur</div>
      <div class="big"><span id="Te">--</span> &deg;C</div>
      <div class="big"><span id="He">--</span> %</div>
    </div>
  </div>
</div>

<div class="card">
  <h2>Equipements</h2>
  <div class="devs">
    <div class="dev off" id="d-fan">
      <div class="devname">VENTILATION</div>
      <div class="devstat off" id="s-fan">OFF</div>
      <div class="devmode" id="m-fan"></div>
      <div class="devreason" id="r-fan"></div>
      <div class="acts">
        <button onclick="toggleFan()">Toggle</button>
      </div>
    </div>
    <div class="dev off" id="d-dehum">
      <div class="devname">DESHUMIDIF.</div>
      <div class="devstat off" id="s-dehum">OFF</div>
      <div class="devmode" id="m-dehum"></div>
      <div class="devreason" id="r-dehum"></div>
      <div class="acts">
        <button onclick="toggleDehum()">Toggle</button>
      </div>
    </div>
    <div class="dev off" id="d-heater">
      <div class="devname">CHAUFFAGE</div>
      <div class="devstat off" id="s-heater">OFF</div>
      <div class="devmode" id="m-heater"></div>
      <div class="devreason" id="r-heater"></div>
      <div class="acts">
        <input type="number" id="hmin" value="30" min="1" max="240">
        <button onclick="setHeater()">ON</button>
        <button class="red" onclick="offHeater()">OFF</button>
      </div>
    </div>
  </div>
</div>

<div class="card">
  <h2>Reglages</h2>
  <div id="sform"></div>
  <button onclick="saveSettings()" style="margin-top:7px">Sauvegarder</button>
</div>

<div class="footer">Actualisation auto toutes les 5s &bull; <span id="lupd">--</span></div>

<script>
const KEYS=[
  ['humidity_on','Seuil humidite ON','%'],
  ['humidity_hysteresis','Hysteresis','%'],
  ['temp_min_dehum','Temp. min deshumidif.','C'],
  ['temp_heater_low','Chauffage seuil bas','C'],
  ['temp_heater_high','Chauffage seuil haut','C'],
  ['humidity_fan_max','Hum. ext max ventil.','%'],
  ['temp_fan_delta','Delta temp ventil.','C'],
  ['k_predictive','Coeff. predictif',''],
  ['rate_min','Taux min prediction','%/h'],
  ['min_run_dehum_min','Cycle min deshumidif.','min'],
  ['humidity_correction','Correction hum. BME280','x'],
];
let cfg={};
async function status(){
  try{
    const d=await(await fetch('/api/status')).json();
    document.getElementById('ts').textContent=d.time||'--';
    set('Ti',d.T_int!=null?d.T_int.toFixed(1):'--');
    set('Hi',d.H_int!=null?d.H_int.toFixed(1):'--');
    set('Te',d.T_ext!=null?d.T_ext.toFixed(1):'--');
    set('He',d.H_ext!=null?d.H_ext.toFixed(1):'--');
    updev('fan',  d.fan,  d.fan_reason,  d.manual_fan);
    updev('dehum',d.dehum,d.dehum_reason,d.manual_dehum);
    upheater(d.heater,d.heater_reason,d.heater_timer);
    document.getElementById('lupd').textContent=new Date().toLocaleTimeString();
  }catch(e){}
}
function set(id,v){document.getElementById(id).textContent=v;}
function updev(n,on,reason,manual){
  document.getElementById('d-'+n).className='dev '+(on?'on':'off');
  const s=document.getElementById('s-'+n);
  s.textContent=on?'ON':'OFF';
  s.className='devstat '+(on?'on':'off');
  document.getElementById('r-'+n).textContent=reason||'';
  const m=document.getElementById('m-'+n);
  m.textContent=manual===1?'MANUEL ON':manual===0?'MANUEL OFF':'AUTO';
}
function upheater(on,reason,timer){
  document.getElementById('d-heater').className='dev '+(on?'on':'off');
  const s=document.getElementById('s-heater');
  s.textContent=on?'ON':'OFF';
  s.className='devstat '+(on?'on':'off');
  document.getElementById('r-heater').textContent=reason||'';
  document.getElementById('m-heater').textContent=timer>0?'MANUEL '+Math.ceil(timer/60)+'min':'AUTO';
}
async function getConfig(){
  cfg=await(await fetch('/api/config')).json();
  document.getElementById('sform').innerHTML=KEYS.map(([k,l,u])=>
    `<div class="srow"><span>${l}</span><input type="number" step="0.5" id="c-${k}" value="${cfg[k]??''}"><span>${u}</span></div>`
  ).join('');
}
async function saveSettings(){
  const upd={};
  KEYS.forEach(([k])=>{const v=document.getElementById('c-'+k)?.value;if(v!=='')upd[k]=parseFloat(v);});
  await fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(upd)});
  getConfig();
}
async function toggleFan()  {await fetch('/api/fan',  {method:'POST'});status();}
async function toggleDehum(){await fetch('/api/dehum',{method:'POST'});status();}
async function setHeater(){
  const m=parseInt(document.getElementById('hmin').value)||30;
  await fetch('/api/heater',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({minutes:m})});
  status();
}
async function offHeater(){
  await fetch('/api/heater',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({minutes:0})});
  status();
}
status();getConfig();
setInterval(status,5000);
</script>
</body>
</html>
)HVAC";

// ─────────────────────────────────────────────────────────────────
// Handlers
// ─────────────────────────────────────────────────────────────────
static void handle_root() {
    server.send_P(200, "text/html", HTML);
}

static void handle_status() {
    DynamicJsonDocument doc(512);
    time_t now = time(nullptr);
    struct tm* ti = localtime(&now);
    char tbuf[10];
    strftime(tbuf, sizeof(tbuf), "%H:%M:%S", ti);
    doc["time"] = tbuf;

    if (g_sensors->valid) {
        doc["T_int"] = g_sensors->T_int;
        doc["H_int"] = g_sensors->H_int;
        doc["T_ext"] = g_sensors->T_ext;
        doc["H_ext"] = g_sensors->H_ext;
    }
    doc["fan"]    = g_gpio->fan;
    doc["dehum"]  = g_gpio->dehum;
    doc["heater"] = g_gpio->heater;

    doc["fan_reason"]    = g_decisions->fan_reason;
    doc["dehum_reason"]  = g_decisions->dehum_reason;
    doc["heater_reason"] = g_decisions->heater_reason;

    doc["manual_fan"]   = (int)g_state->manual_fan;
    doc["manual_dehum"] = (int)g_state->manual_dehum;

    long timer = 0;
    if (g_state->heater_manual_off_at > 0 && now < g_state->heater_manual_off_at)
        timer = (long)(g_state->heater_manual_off_at - now);
    doc["heater_timer"] = timer;

    doc["seuil_on"]  = g_decisions->seuil_on;
    doc["seuil_off"] = g_decisions->seuil_off;
    doc["rate_ext"]  = g_decisions->rate_ext;

    String out;
    serializeJson(doc, out);
    server.send(200, "application/json", out);
}

static void handle_config_get() {
    StaticJsonDocument<512> doc;
    doc["humidity_on"]         = g_cfg->humidity_on;
    doc["humidity_hysteresis"] = g_cfg->humidity_hysteresis;
    doc["humidity_fan_max"]    = g_cfg->humidity_fan_max;
    doc["temp_min_dehum"]      = g_cfg->temp_min_dehum;
    doc["temp_heater_low"]     = g_cfg->temp_heater_low;
    doc["temp_heater_high"]    = g_cfg->temp_heater_high;
    doc["temp_fan_delta"]      = g_cfg->temp_fan_delta;
    doc["heater_default_min"]  = g_cfg->heater_default_min;
    doc["k_predictive"]        = g_cfg->k_predictive;
    doc["rate_min"]            = g_cfg->rate_min;
    doc["min_run_dehum_min"]   = g_cfg->min_run_dehum_min;
    doc["humidity_correction"] = g_cfg->humidity_correction;
    String out;
    serializeJson(doc, out);
    server.send(200, "application/json", out);
}

static void handle_config_post() {
    if (!server.hasArg("plain")) { server.send(400); return; }
    StaticJsonDocument<512> doc;
    if (deserializeJson(doc, server.arg("plain"))) { server.send(400); return; }
    if (doc.containsKey("humidity_on"))         g_cfg->humidity_on         = doc["humidity_on"];
    if (doc.containsKey("humidity_hysteresis")) g_cfg->humidity_hysteresis = doc["humidity_hysteresis"];
    if (doc.containsKey("humidity_fan_max"))    g_cfg->humidity_fan_max    = doc["humidity_fan_max"];
    if (doc.containsKey("temp_min_dehum"))      g_cfg->temp_min_dehum      = doc["temp_min_dehum"];
    if (doc.containsKey("temp_heater_low"))     g_cfg->temp_heater_low     = doc["temp_heater_low"];
    if (doc.containsKey("temp_heater_high"))    g_cfg->temp_heater_high    = doc["temp_heater_high"];
    if (doc.containsKey("temp_fan_delta"))      g_cfg->temp_fan_delta      = doc["temp_fan_delta"];
    if (doc.containsKey("heater_default_min"))  g_cfg->heater_default_min  = doc["heater_default_min"];
    if (doc.containsKey("k_predictive"))        g_cfg->k_predictive        = doc["k_predictive"];
    if (doc.containsKey("rate_min"))            g_cfg->rate_min            = doc["rate_min"];
    if (doc.containsKey("min_run_dehum_min"))   g_cfg->min_run_dehum_min   = doc["min_run_dehum_min"];
    if (doc.containsKey("humidity_correction")) g_cfg->humidity_correction = doc["humidity_correction"];
    storage_save_config(*g_cfg);
    server.send(200, "text/plain", "OK");
}

// Cycle AUTO -> MANUEL ON -> MANUEL OFF -> AUTO (identique au TUI Python)
static void toggle_manual(int8_t& manual, int pin, bool& gpio_val) {
    if (manual == -1) {
        manual = 1;
        digitalWrite(pin, HIGH);
        gpio_val = true;
    } else if (manual == 1) {
        manual = 0;
        digitalWrite(pin, LOW);
        gpio_val = false;
    } else {
        manual = -1;
    }
    storage_save_state(*g_state);
}

static void handle_fan() {
    toggle_manual(g_state->manual_fan, PIN_FAN, g_gpio->fan);
    server.send(200, "text/plain", "OK");
}

static void handle_dehum() {
    toggle_manual(g_state->manual_dehum, PIN_DEHUM, g_gpio->dehum);
    server.send(200, "text/plain", "OK");
}

static void handle_heater() {
    if (!server.hasArg("plain")) { server.send(400); return; }
    StaticJsonDocument<64> doc;
    if (deserializeJson(doc, server.arg("plain"))) { server.send(400); return; }
    int mins = doc["minutes"] | 0;
    if (mins > 0) {
        g_state->heater_manual_off_at = time(nullptr) + (long)mins * 60L;
    } else {
        g_state->heater_manual_off_at = 0;
        digitalWrite(PIN_HEATER, LOW);
        g_gpio->heater = false;
    }
    storage_save_state(*g_state);
    server.send(200, "text/plain", "OK");
}

// ─────────────────────────────────────────────────────────────────
void web_begin(HvacConfig& cfg, HvacState& state,
               Sensors& sensors, GpioState& gpio, Decisions& decisions) {
    g_cfg       = &cfg;
    g_state     = &state;
    g_sensors   = &sensors;
    g_gpio      = &gpio;
    g_decisions = &decisions;

    server.on("/",           HTTP_GET,  handle_root);
    server.on("/api/status", HTTP_GET,  handle_status);
    server.on("/api/config", HTTP_GET,  handle_config_get);
    server.on("/api/config", HTTP_POST, handle_config_post);
    server.on("/api/fan",    HTTP_POST, handle_fan);
    server.on("/api/dehum",  HTTP_POST, handle_dehum);
    server.on("/api/heater", HTTP_POST, handle_heater);
    server.begin();
    Serial.println("Serveur web demarre sur port 80");
}

void web_handle() {
    server.handleClient();
}
