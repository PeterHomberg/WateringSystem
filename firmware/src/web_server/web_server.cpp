#include "web_server.h"
#include "config.h"
#include "valves/valves.h"
#include "rain/rain.h"
#include "rtc/rtc.h"
#include "scheduler/scheduler.h"
#include <WebServer.h>
#include <ArduinoJson.h>
#include "wifi_manager/wifi_manager.h"

static WebServer server(WEB_SERVER_PORT);

// Stores the last schedule POSTed as JSON so GET /api/schedule can return it.
static String g_scheduleJson = "[]";

// ── Embedded dashboard ────────────────────────────────────────────────────────
static const char DASHBOARD_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Watering System</title>
<style>
  :root {
    --bg:#0f172a; --card:#1e293b; --border:#334155;
    --text:#f1f5f9; --muted:#94a3b8;
    --green:#22c55e; --red:#ef4444; --blue:#3b82f6;
    --r:12px;
  }
  *{box-sizing:border-box;margin:0;padding:0}
  body{background:var(--bg);color:var(--text);font-family:system-ui,sans-serif;padding:16px}
  h1{font-size:1.4rem;margin-bottom:4px}
  .sub{color:var(--muted);font-size:.85rem;margin-bottom:20px}
  .grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(280px,1fr));gap:16px}
  .card{background:var(--card);border:1px solid var(--border);border-radius:var(--r);padding:20px}
  .card h2{font-size:.75rem;text-transform:uppercase;letter-spacing:.08em;color:var(--muted);margin-bottom:14px}
  .row{display:flex;align-items:center;gap:10px;margin-bottom:10px}
  .dot{width:10px;height:10px;border-radius:50%;flex-shrink:0;background:var(--border)}
  .dot.on{background:var(--green)} .dot.rain{background:var(--blue)}
  .lbl{color:var(--muted);font-size:.85rem;min-width:80px}
  .val{font-weight:600}
  .bar-w{flex:1;height:8px;background:var(--border);border-radius:4px;overflow:hidden}
  .bar-f{height:100%;background:var(--blue);border-radius:4px;transition:width .5s}
  .btn{display:inline-flex;align-items:center;justify-content:center;border:none;
       border-radius:8px;padding:10px 20px;font-size:.9rem;font-weight:600;
       cursor:pointer;transition:opacity .15s}
  .btn:hover{opacity:.85}.btn:active{opacity:.7}
  .g{background:var(--green);color:#000}
  .r{background:var(--red);color:#fff}
  .b{background:var(--blue);color:#fff}
  .m{background:var(--border);color:var(--text)}
  .vrow{display:flex;gap:10px;margin-bottom:10px}
  .vrow .btn{flex:1}
  input,select{width:100%;background:var(--bg);border:1px solid var(--border);
               color:var(--text);border-radius:8px;padding:8px 12px;
               font-size:.9rem;margin-bottom:10px}
  .entry{background:var(--bg);border:1px solid var(--border);border-radius:8px;
         padding:12px;margin-bottom:10px}
  .entry h3{font-size:.8rem;color:var(--muted);margin-bottom:8px}
  .dgrid{display:grid;grid-template-columns:repeat(7,1fr);gap:4px;margin-bottom:8px}
  .dbt{border:1px solid var(--border);background:var(--bg);color:var(--muted);
       border-radius:6px;padding:4px 2px;font-size:.7rem;cursor:pointer;text-align:center;
       transition:all .15s}
  .dbt.act{background:var(--blue);border-color:var(--blue);color:#fff}
  .r2{display:grid;grid-template-columns:1fr 1fr;gap:8px}
  .msg{font-size:.8rem;color:var(--green);margin-top:6px;min-height:18px}
  .msg.e{color:var(--red)}
  .clock{font-size:2rem;font-weight:700;letter-spacing:.05em;margin-bottom:4px}
  .dateline{color:var(--muted);font-size:.85rem;margin-bottom:16px}
</style>
</head>
<body>
<h1>🌿 Watering System</h1>
<p class="sub" id="sub">Connecting...</p>
<div class="grid">

  <!-- STATUS -->
  <div class="card">
    <h2>Status</h2>
    <div class="clock" id="rtcTime">--:--</div>
    <div class="dateline" id="rtcDate">---</div>
    <div class="row"><span class="lbl">WiFi</span><span class="dot on"></span><span class="val" id="ipVal">--</span></div>
    <div class="row"><span class="lbl">Rain</span><span class="dot" id="rainDot"></span><span class="val" id="rainVal">--</span></div>
    <div class="row">
      <span class="lbl">Wetness</span>
      <div class="bar-w"><div class="bar-f" id="wetBar" style="width:0%"></div></div>
      <span class="val" id="wetVal" style="min-width:36px;text-align:right">--%</span>
    </div>
    <div class="row"><span class="lbl">Valve 1</span><span class="dot" id="v1d"></span><span class="val" id="v1v">--</span></div>
    <div class="row"><span class="lbl">Valve 2</span><span class="dot" id="v2d"></span><span class="val" id="v2v">--</span></div>
    <div class="row"><span class="lbl">Schedule</span><span class="val" id="schAck">--</span></div>
  </div>

  <!-- MANUAL CONTROL -->
  <div class="card">
    <h2>Manual Control</h2>
    <p style="color:var(--muted);font-size:.85rem;margin-bottom:14px">Manual commands bypass rain inhibit.</p>
    <p style="font-weight:600;margin-bottom:8px">Valve 1</p>
    <div class="vrow">
      <button class="btn g" onclick="valve(1,true)">Open</button>
      <button class="btn r" onclick="valve(1,false)">Close</button>
    </div>
    <p style="font-weight:600;margin-bottom:8px">Valve 2</p>
    <div class="vrow">
      <button class="btn g" onclick="valve(2,true)">Open</button>
      <button class="btn r" onclick="valve(2,false)">Close</button>
    </div>
    <p class="msg" id="manMsg"></p>
  </div>

  <!-- SET CLOCK -->
  <div class="card">
    <h2>Set Clock</h2>
    <p style="color:var(--muted);font-size:.85rem;margin-bottom:14px">Sync the DS3231 to your local time.</p>
    <label style="font-size:.85rem;color:var(--muted)">Date &amp; time</label>
    <input type="datetime-local" id="dtIn">
    <button class="btn b" style="width:100%" onclick="setTime()">Sync Clock</button>
    <p class="msg" id="timeMsg"></p>
  </div>

  <!-- SCHEDULE -->
  <div class="card" style="grid-column:1/-1">
    <h2>Schedule</h2>
    <div id="entries"></div>
    <button class="btn m" style="margin-bottom:14px" onclick="addEntry()">+ Add entry</button><br>
    <button class="btn b" onclick="saveSchedule()">Save Schedule</button>
    <p class="msg" id="schedMsg"></p>
  </div>

</div>
<script>
const DAYS=['Mon','Tue','Wed','Thu','Fri','Sat','Sun'];
let entries=[];

async function poll(){
  try{
    const d=await(await fetch('/api/status')).json();
    document.getElementById('rtcTime').textContent=d.time||'--:--';
    document.getElementById('rtcDate').textContent=d.date||'---';
    document.getElementById('ipVal').textContent=d.ip||'--';
    document.getElementById('sub').textContent='Connected · '+d.ip;
    const raining=d.rain===1;
    document.getElementById('rainDot').className='dot'+(raining?' rain':'');
    document.getElementById('rainVal').textContent=raining?'Yes':'No';
    const lvl=d.rain_level??0;
    document.getElementById('wetBar').style.width=lvl+'%';
    document.getElementById('wetVal').textContent=lvl+'%';
    vui('v1',d.valve1===1);vui('v2',d.valve2===1);
    document.getElementById('schAck').textContent=d.schedule_ack||'--';
  }catch(e){}
}
function vui(id,open){
  document.getElementById(id+'d').className='dot'+(open?' on':'');
  document.getElementById(id+'v').textContent=open?'Open':'Closed';
}
setInterval(poll,3000);poll();

async function valve(v,open){
  const m=document.getElementById('manMsg');
  try{
    const d=await(await fetch('/api/valve',{method:'POST',
      headers:{'Content-Type':'application/json'},
      body:JSON.stringify({valve:v,open})})).json();
    m.className='msg';m.textContent=d.ok?`Valve ${v} ${open?'opened':'closed'}`:'Error: '+d.error;
  }catch(e){m.className='msg e';m.textContent='Request failed';}
  setTimeout(()=>m.textContent='',3000);
}

async function setTime(){
  const m=document.getElementById('timeMsg');
  const val=document.getElementById('dtIn').value;
  if(!val){m.className='msg e';m.textContent='Pick a date/time first';return;}
  const dt=new Date(val);
  const fwDay=dt.getDay()===0?6:dt.getDay()-1;
  const iso=val.length===16?val+':00':val;
  try{
    const d=await(await fetch('/api/time',{method:'POST',
      headers:{'Content-Type':'application/json'},
      body:JSON.stringify({datetime:iso,weekday:fwDay})})).json();
    m.className=d.ok?'msg':'msg e';
    m.textContent=d.ok?'Clock updated ✓':'Error: '+d.error;
  }catch(e){m.className='msg e';m.textContent='Request failed';}
}
(()=>{const n=new Date();n.setSeconds(0,0);document.getElementById('dtIn').value=n.toISOString().slice(0,16);})();

async function loadSchedule(){
  try{entries=await(await fetch('/api/schedule')).json();renderEntries();}catch(e){}
}
function renderEntries(){
  const el=document.getElementById('entries');
  el.innerHTML='';
  entries.forEach((e,i)=>el.appendChild(makeEntry(e,i)));
}
function makeEntry(e,i){
  const div=document.createElement('div');
  div.className='entry';
  div.innerHTML=`
    <h3>Entry ${i+1} <span style="float:right;cursor:pointer;color:var(--red)" onclick="rmEntry(${i})">✕</span></h3>
    <div class="r2">
      <div><label style="font-size:.8rem;color:var(--muted)">Zone</label>
        <select onchange="entries[${i}].valve=+this.value">
          <option value="1" ${e.valve==1?'selected':''}>Valve 1</option>
          <option value="2" ${e.valve==2?'selected':''}>Valve 2</option>
        </select></div>
      <div><label style="font-size:.8rem;color:var(--muted)">Duration (min)</label>
        <input type="number" min="1" max="120" value="${e.duration}"
               oninput="entries[${i}].duration=+this.value"></div>
    </div>
    <label style="font-size:.8rem;color:var(--muted)">Start time</label>
    <input type="time" value="${pad(e.hour)}:${pad(e.minute)}"
           oninput="updTime(${i},this.value)" style="margin-bottom:10px">
    <label style="font-size:.8rem;color:var(--muted)">Days</label>
    <div class="dgrid" id="dg${i}"></div>`;
  const dg=div.querySelector(`#dg${i}`);
  DAYS.forEach((d,bit)=>{
    const b=document.createElement('button');
    b.className='dbt'+(e.dayMask&(1<<bit)?' act':'');
    b.textContent=d;
    b.onclick=()=>{entries[i].dayMask^=(1<<bit);b.classList.toggle('act');};
    dg.appendChild(b);
  });
  return div;
}
function pad(n){return String(n).padStart(2,'0');}
function updTime(i,v){const[h,m]=v.split(':');entries[i].hour=+h;entries[i].minute=+m;}
function addEntry(){entries.push({valve:1,dayMask:0x1F,hour:7,minute:0,duration:20});renderEntries();}
function rmEntry(i){entries.splice(i,1);renderEntries();}

async function saveSchedule(){
  const m=document.getElementById('schedMsg');
  try{
    const d=await(await fetch('/api/schedule',{method:'POST',
      headers:{'Content-Type':'application/json'},
      body:JSON.stringify(entries)})).json();
    m.className=d.ok?'msg':'msg e';
    m.textContent=d.ok?'Schedule saved ✓':'Error: '+d.error;
  }catch(e){m.className='msg e';m.textContent='Request failed';}
  setTimeout(()=>m.textContent='',4000);
}
loadSchedule();
</script>
</body>
</html>
)rawhtml";

// ── Helpers ───────────────────────────────────────────────────────────────────

static void sendJson(int code, JsonDocument& doc) {
    String out;
    serializeJson(doc, out);
    server.send(code, "application/json", out);
}

static void sendOk() {
    StaticJsonDocument<32> d;
    d["ok"] = true;
    sendJson(200, d);
}

static void sendErr(const char* msg, int code = 400) {
    StaticJsonDocument<64> d;
    d["ok"] = false; d["error"] = msg;
    sendJson(code, d);
}

// ── Handlers ──────────────────────────────────────────────────────────────────

static void handleRoot() {
    server.send_P(200, "text/html", DASHBOARD_HTML);
}

static void handleStatus() {
    StaticJsonDocument<200> doc;
    char timeBuf[8], dateBuf[14];
    getRTCTimeString(timeBuf, sizeof(timeBuf));
    getRTCDateString(dateBuf, sizeof(dateBuf));
    doc["time"]         = timeBuf;
    doc["date"]         = dateBuf;
    doc["ip"]           = getWiFiIP();
    doc["rain"]         = isRaining() ? 1 : 0;
    doc["rain_level"]   = getRainLevel();
    doc["valve1"]       = isValveOpen(1) ? 1 : 0;
    doc["valve2"]       = isValveOpen(2) ? 1 : 0;
    doc["schedule_ack"] = getScheduleAck();
    sendJson(200, doc);
}

static void handleValvePost() {
    if (!server.hasArg("plain")) { sendErr("no body"); return; }
    StaticJsonDocument<64> body;
    if (deserializeJson(body, server.arg("plain"))) { sendErr("bad JSON"); return; }
    int  v    = body["valve"] | 0;
    bool open = body["open"]  | false;
    if (v != 1 && v != 2) { sendErr("valve must be 1 or 2"); return; }
    open ? openValve(v) : closeValve(v);
    sendOk();
}

static void handleTimePost() {
    if (!server.hasArg("plain")) { sendErr("no body"); return; }
    StaticJsonDocument<128> body;
    if (deserializeJson(body, server.arg("plain"))) { sendErr("bad JSON"); return; }
    const char* dt = body["datetime"] | "";
    int weekday    = body["weekday"]  | -1;
    if (strlen(dt) < 19 || weekday < 0 || weekday > 6) {
        sendErr("invalid fields"); return;
    }
    String s = String("TIME:") + dt + "W" + String(weekday);
    s.replace(' ', 'T');
    if (!setRTCTimeFromString(s)) { sendErr("RTC write failed"); return; }
    sendOk();
}

static void handleScheduleGet() {
    server.send(200, "application/json", g_scheduleJson);
}

static void handleSchedulePost() {
    if (!server.hasArg("plain")) { sendErr("no body"); return; }
    DynamicJsonDocument body(1024);
    if (deserializeJson(body, server.arg("plain"))) { sendErr("bad JSON"); return; }
    if (!body.is<JsonArray>())                      { sendErr("expected array"); return; }
    JsonArray arr = body.as<JsonArray>();
    if (arr.size() > 6)                             { sendErr("max 6 entries"); return; }

    const char* dayNames[] = {"Mon","Tue","Wed","Thu","Fri","Sat","Sun"};
    processScheduleLine("SCHED:BEGIN");
    for (JsonObject e : arr) {
        int valve    = e["valve"]    | 1;
        int dayMask  = e["dayMask"]  | 0;
        int hour     = e["hour"]     | 0;
        int minute   = e["minute"]   | 0;
        int duration = e["duration"] | 20;
        String days = "";
        for (int d = 0; d < 7; d++) {
            if (dayMask & (1 << d)) {
                if (days.length()) days += "+";
                days += dayNames[d];
            }
        }
        if (days.length() == 0) continue;
        char line[64];
        snprintf(line, sizeof(line), "SCHED:V%d,%s,%02d:%02d,%d",
                 valve, days.c_str(), hour, minute, duration);
        processScheduleLine(String(line));
    }
    processScheduleLine("SCHED:END");

    if (strcmp(getScheduleAck(), "SCH:OK") == 0) {
        serializeJson(body, g_scheduleJson);
        sendOk();
    } else {
        sendErr("schedule parse error");
    }
}

// ── Public API ────────────────────────────────────────────────────────────────

void initWebServer() {
    server.on("/",             HTTP_GET,  handleRoot);
    server.on("/api/status",   HTTP_GET,  handleStatus);
    server.on("/api/valve",    HTTP_POST, handleValvePost);
    server.on("/api/time",     HTTP_POST, handleTimePost);
    server.on("/api/schedule", HTTP_GET,  handleScheduleGet);
    server.on("/api/schedule", HTTP_POST, handleSchedulePost);
    server.onNotFound([]() {
        server.send(404, "text/plain", "Not found");
    });
    server.begin();
    Serial.printf("Web server started on port %d\n", WEB_SERVER_PORT);
}

void handleWebServer() {
    server.handleClient();
}
