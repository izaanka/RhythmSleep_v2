#include "web_server.h"
#include <ArduinoJson.h>

const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>RhythmSleep Dashboard</title>
    <style>
        @import url('https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600;700&display=swap');
        :root {
            --bg-color: #0d1117; --card-bg: #161b22; --text-main: #c9d1d9; --text-dim: #8b949e;
            --accent: #58a6ff; --border: #30363d; --wake: #d2a8ff; --light: #58a6ff; --deep: #1f6feb; --rem: #ff7b72;
            --delta: #1f6feb; --theta: #58a6ff; --alpha: #3fb950; --beta: #d2a8ff; --gamma: #ff7b72;
        }
        body { background-color: var(--bg-color); color: var(--text-main); font-family: 'Inter', sans-serif; margin: 0; padding: 20px; box-sizing: border-box; display: flex; flex-direction: column; align-items: center; }
        .container { width: 100%; max-width: 1200px; display: grid; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); gap: 20px; }
        .header { width: 100%; max-width: 1200px; display: flex; justify-content: space-between; align-items: center; margin-bottom: 30px; padding-bottom: 20px; border-bottom: 1px solid var(--border); }
        .header-title { display: flex; align-items: center; gap: 10px; font-size: 24px; font-weight: 700; }
        .status-dot { width: 10px; height: 10px; border-radius: 50%; background-color: #3fb950; box-shadow: 0 0 10px #3fb950; }
        .status-dot.disconnected { background-color: #f85149; box-shadow: 0 0 10px #f85149; }
        .card { background-color: var(--card-bg); border: 1px solid var(--border); border-radius: 12px; padding: 20px; box-shadow: 0 4px 12px rgba(0,0,0,0.2); }
        .card-title { font-size: 16px; font-weight: 600; margin: 0 0 15px 0; color: var(--text-dim); text-transform: uppercase; }
        .stage-badge { display: inline-block; padding: 8px 16px; border-radius: 20px; font-weight: 700; font-size: 24px; margin-bottom: 20px; }
        .stage-WAKE { background: rgba(210, 168, 255, 0.2); color: var(--wake); }
        .stage-LIGHT { background: rgba(88, 166, 255, 0.2); color: var(--light); }
        .stage-DEEP { background: rgba(31, 111, 235, 0.2); color: var(--deep); }
        .stage-REM { background: rgba(255, 123, 114, 0.2); color: var(--rem); }
        .conf-bars { display: flex; flex-direction: column; gap: 8px; margin-bottom: 20px; }
        .conf-row { display: flex; align-items: center; gap: 10px; font-size: 12px; }
        .conf-label { width: 50px; }
        .conf-bar-container { flex-grow: 1; height: 6px; background: rgba(255,255,255,0.1); border-radius: 3px; overflow: hidden; }
        .conf-bar-fill { height: 100%; border-radius: 3px; transition: width 0.3s ease; }
        .band-powers { display: flex; flex-direction: column; gap: 12px; }
        .band-row { display: flex; align-items: center; gap: 10px; }
        .band-label { width: 60px; font-size: 14px; font-weight: 500;}
        .band-bar-container { flex-grow: 1; height: 12px; background: rgba(255,255,255,0.1); border-radius: 6px; overflow: hidden; }
        .band-bar-fill { height: 100%; border-radius: 6px; transition: width 0.3s ease; }
        .band-pct { width: 45px; text-align: right; font-size: 12px; font-family: monospace; }
        canvas { width: 100%; background: rgba(0,0,0,0.5); border-radius: 8px; border: 1px solid var(--border); }
        #eegCanvas { height: 100px; } #hypnogramCanvas { height: 120px; margin-bottom: 15px;}
        .form-group { margin-bottom: 15px; }
        .form-group label { display: block; margin-bottom: 5px; font-size: 14px; color: var(--text-dim); }
        input[type="text"], input[type="number"], input[type="password"] { width: 100%; padding: 10px; background: rgba(255,255,255,0.05); border: 1px solid var(--border); border-radius: 6px; color: var(--text-main); box-sizing: border-box; }
        .time-inputs { display: flex; gap: 10px; align-items: center; }
        button { background: var(--accent); color: #fff; border: none; padding: 10px 20px; border-radius: 6px; font-weight: 600; cursor: pointer; width: 100%; }
        #toast { position: fixed; bottom: 20px; right: 20px; background: #3fb950; color: white; padding: 12px 24px; border-radius: 8px; transform: translateY(100px); opacity: 0; transition: all 0.3s ease; }
        #toast.show { transform: translateY(0); opacity: 1; }
        .stats-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; font-size: 14px; }
        .stat-box { background: rgba(255,255,255,0.05); padding: 10px; border-radius: 6px; text-align: center; }
        .stat-val { font-size: 18px; font-weight: 700; margin-top: 5px;}
    </style>
</head>
<body>
    <div class="header">
        <div class="header-title">RhythmSleep</div>
        <div style="display: flex; align-items: center; gap: 10px;">
            <span id="timeDisplay" style="font-family: monospace;">--:--:--</span>
            <div id="connStatus" class="status-dot disconnected"></div>
        </div>
    </div>
    <div class="container">
        <div class="card">
            <h2 class="card-title">Live Status</h2>
            <div style="text-align: center;"><div id="stageBadge" class="stage-badge stage-WAKE">WAKE</div></div>
            <div class="conf-bars">
                <div class="conf-row"><div class="conf-label">WAKE</div><div class="conf-bar-container"><div id="c-wake" class="conf-bar-fill" style="background: var(--wake); width: 0%;"></div></div></div>
                <div class="conf-row"><div class="conf-label">LIGHT</div><div class="conf-bar-container"><div id="c-light" class="conf-bar-fill" style="background: var(--light); width: 0%;"></div></div></div>
                <div class="conf-row"><div class="conf-label">DEEP</div><div class="conf-bar-container"><div id="c-deep" class="conf-bar-fill" style="background: var(--deep); width: 0%;"></div></div></div>
                <div class="conf-row"><div class="conf-label">REM</div><div class="conf-bar-container"><div id="c-rem" class="conf-bar-fill" style="background: var(--rem); width: 0%;"></div></div></div>
            </div>
            <canvas id="eegCanvas" width="600" height="200"></canvas>
        </div>
        <div class="card">
            <h2 class="card-title">Band Powers</h2>
            <div class="band-powers">
                <div class="band-row"><div class="band-label">Delta</div><div class="band-bar-container"><div id="b-delta" class="band-bar-fill" style="background: var(--delta); width: 0%;"></div></div><div id="p-delta" class="band-pct">0%</div></div>
                <div class="band-row"><div class="band-label">Theta</div><div class="band-bar-container"><div id="b-theta" class="band-bar-fill" style="background: var(--theta); width: 0%;"></div></div><div id="p-theta" class="band-pct">0%</div></div>
                <div class="band-row"><div class="band-label">Alpha</div><div class="band-bar-container"><div id="b-alpha" class="band-bar-fill" style="background: var(--alpha); width: 0%;"></div></div><div id="p-alpha" class="band-pct">0%</div></div>
                <div class="band-row"><div class="band-label">Beta</div><div class="band-bar-container"><div id="b-beta" class="band-bar-fill" style="background: var(--beta); width: 0%;"></div></div><div id="p-beta" class="band-pct">0%</div></div>
                <div class="band-row"><div class="band-label">Gamma</div><div class="band-bar-container"><div id="b-gamma" class="band-bar-fill" style="background: var(--gamma); width: 0%;"></div></div><div id="p-gamma" class="band-pct">0%</div></div>
            </div>
        </div>
        <div class="card" style="grid-column: 1 / -1;">
            <h2 class="card-title">Sleep History (Hypnogram)</h2>
            <canvas id="hypnogramCanvas" width="1200" height="200"></canvas>
            <div class="stats-grid">
                <div class="stat-box">Total Sleep<div id="stat-total" class="stat-val">--h --m</div></div>
                <div class="stat-box">Efficiency<div id="stat-eff" class="stat-val">--%</div></div>
                <div class="stat-box">Deep Sleep<div id="stat-deep" class="stat-val">--h --m</div></div>
                <div class="stat-box">REM Sleep<div id="stat-rem" class="stat-val">--h --m</div></div>
            </div>
        </div>
        <div class="card" style="grid-column: 1 / -1;">
            <h2 class="card-title">Configuration</h2>
            <form id="configForm">
                <div style="display: grid; grid-template-columns: repeat(auto-fit, minmax(250px, 1fr)); gap: 20px;">
                    <div>
                        <div class="form-group"><label>Alarm Window Start (HH:MM)</label><div class="time-inputs"><input type="number" id="cfg_minH" min="0" max="23"> : <input type="number" id="cfg_minM" min="0" max="59"></div></div>
                        <div class="form-group"><label>Alarm Window End (HH:MM)</label><div class="time-inputs"><input type="number" id="cfg_maxH" min="0" max="23"> : <input type="number" id="cfg_maxM" min="0" max="59"></div></div>
                        <div class="form-group"><label>Alarm Volume (0-30)</label><input type="number" id="cfg_vol" min="0" max="30"></div>
                    </div>
                    <div>
                        <div class="form-group"><label>WiFi SSID</label><input type="text" id="cfg_ssid"></div>
                        <div class="form-group"><label>WiFi Password</label><input type="password" id="cfg_pass"></div>
                        <div class="form-group"><label><input type="checkbox" id="cfg_ap"> Access Point Mode</label></div>
                        <div class="form-group"><label><input type="checkbox" id="cfg_raw"> Save Raw EEG to SD</label></div>
                    </div>
                </div>
                <button type="submit" style="margin-top: 15px;">Save Settings</button>
            </form>
        </div>
    </div>
    <div id="toast">Settings Saved!</div>
    <script>
        const stages = ["WAKE", "LIGHT", "DEEP", "REM"];
        const stageColors = ["#d2a8ff", "#58a6ff", "#1f6feb", "#ff7b72"];
        setInterval(() => { document.getElementById('timeDisplay').textContent = new Date().toLocaleTimeString('en-US', { hour12: false }); }, 1000);
        const eegCanvas = document.getElementById('eegCanvas'); const ctxEeg = eegCanvas.getContext('2d');
        const eegData = new Array(eegCanvas.width).fill(0); let eegIdx = 0;
        function drawEeg() {
            ctxEeg.clearRect(0, 0, eegCanvas.width, eegCanvas.height); ctxEeg.beginPath(); ctxEeg.strokeStyle = '#3fb950'; ctxEeg.lineWidth = 2;
            const mid = eegCanvas.height / 2;
            for (let i=0; i<eegCanvas.width; i++) {
                const idx = (eegIdx + i) % eegCanvas.width; const y = mid - (eegData[idx] * (eegCanvas.height/2) / 500);
                if (i===0) ctxEeg.moveTo(i, y); else ctxEeg.lineTo(i, y);
            }
            ctxEeg.stroke(); requestAnimationFrame(drawEeg);
        }
        drawEeg();

        function pollStatus() {
            fetch('/api/status').then(res => res.json()).then(data => {
                document.getElementById('connStatus').className = 'status-dot';
                if (data.eeg !== undefined) { eegData[eegIdx] = data.eeg; eegIdx = (eegIdx + 1) % eegCanvas.width; }
                if (data.stage !== undefined) { const s = data.stage; const sName = (s >= 0 && s <= 3) ? stages[s] : "UNKNOWN"; const badge = document.getElementById('stageBadge'); badge.textContent = sName; badge.className = 'stage-badge stage-' + sName; }
                if (data.conf) { document.getElementById('c-wake').style.width = (data.conf[0]*100) + '%'; document.getElementById('c-light').style.width = (data.conf[1]*100) + '%'; document.getElementById('c-deep').style.width = (data.conf[2]*100) + '%'; document.getElementById('c-rem').style.width = (data.conf[3]*100) + '%'; }
                if (data.bands) {
                    const updateBand = (id, val) => { const p = Math.min(100, Math.max(0, val * 100)); document.getElementById('b-'+id).style.width = p + '%'; document.getElementById('p-'+id).textContent = Math.round(p) + '%'; };
                    updateBand('delta', data.bands.d); updateBand('theta', data.bands.t); updateBand('alpha', data.bands.a); updateBand('beta', data.bands.b); updateBand('gamma', data.bands.g);
                }
            }).catch(e => {
                document.getElementById('connStatus').className = 'status-dot disconnected';
            });
        }
        setInterval(pollStatus, 500);
        pollStatus();

        fetch('/api/config').then(res => res.json()).then(cfg => {
            document.getElementById('cfg_minH').value = cfg.minWakeHour; document.getElementById('cfg_minM').value = cfg.minWakeMinute;
            document.getElementById('cfg_maxH').value = cfg.maxWakeHour; document.getElementById('cfg_maxM').value = cfg.maxWakeMinute;
            document.getElementById('cfg_vol').value = cfg.alarmVolume; document.getElementById('cfg_ssid').value = cfg.wifiSSID;
            document.getElementById('cfg_pass').value = cfg.wifiPassword; document.getElementById('cfg_ap').checked = cfg.apMode;
            document.getElementById('cfg_raw').checked = cfg.saveRawEEG;
        });
        fetch('/api/sleep').then(res => res.json()).then(epochs => {
            if (!epochs || epochs.length === 0) return;
            const canvas = document.getElementById('hypnogramCanvas'); const ctx = canvas.getContext('2d'); ctx.clearRect(0,0,canvas.width,canvas.height);
            const w = canvas.width / epochs.length; let wCnt=0, lCnt=0, dCnt=0, rCnt=0;
            epochs.forEach((ep, i) => {
                const s = ep.stage;
                if (s>=0 && s<=3) { ctx.fillStyle = stageColors[s]; ctx.fillRect(i*w, 0, Math.ceil(w), canvas.height); if(s===0) wCnt++; else if(s===1) lCnt++; else if(s===2) dCnt++; else if(s===3) rCnt++; }
            });
            const epMins = 30 / 60; const tSleep = (lCnt+dCnt+rCnt)*epMins; const tTotal = (wCnt+lCnt+dCnt+rCnt)*epMins; const eff = tTotal > 0 ? (tSleep / tTotal) * 100 : 0;
            document.getElementById('stat-total').textContent = `${Math.floor(tSleep/60)}h ${Math.round(tSleep%60)}m`;
            document.getElementById('stat-eff').textContent = `${Math.round(eff)}%`;
            document.getElementById('stat-deep').textContent = `${Math.floor((dCnt*epMins)/60)}h ${Math.round((dCnt*epMins)%60)}m`;
            document.getElementById('stat-rem').textContent = `${Math.floor((rCnt*epMins)/60)}h ${Math.round((rCnt*epMins)%60)}m`;
        });
        document.getElementById('configForm').onsubmit = (e) => {
            e.preventDefault();
            const data = {
                minWakeHour: parseInt(document.getElementById('cfg_minH').value), minWakeMinute: parseInt(document.getElementById('cfg_minM').value),
                maxWakeHour: parseInt(document.getElementById('cfg_maxH').value), maxWakeMinute: parseInt(document.getElementById('cfg_maxM').value),
                alarmVolume: parseInt(document.getElementById('cfg_vol').value), wifiSSID: document.getElementById('cfg_ssid').value,
                wifiPassword: document.getElementById('cfg_pass').value, apMode: document.getElementById('cfg_ap').checked, saveRawEEG: document.getElementById('cfg_raw').checked
            };
            fetch('/api/config', { method: 'POST', headers: {'Content-Type': 'application/json'}, body: JSON.stringify(data) }).then(res => {
                if (res.ok) { const t = document.getElementById('toast'); t.classList.add('show'); setTimeout(() => t.classList.remove('show'), 3000); }
            });
        };
    </script>
</body>
</html>
)rawliteral";

void WebDashboard::begin(UserConfig& config) {
    configPtr = &config;
    if (config.apMode) {
        WiFi.softAP(config.wifiSSID, config.wifiPassword);
        Serial.printf("[WiFi] AP Started: %s (IP: %s)\n", config.wifiSSID, WiFi.softAPIP().toString().c_str());
    } else {
        WiFi.begin(config.wifiSSID, config.wifiPassword);
        Serial.printf("[WiFi] Connecting to %s...\n", config.wifiSSID);
    }
    setupRoutes();
    server.begin();
    Serial.println("[Web] HTTP Server started on port 80");
}

void WebDashboard::update() {
    server.handleClient();
}

void WebDashboard::setSleepData(const SleepEpochData* data, int count) {
    sleepData = data;
    sleepDataCount = count;
}

void WebDashboard::setCurrentStage(SleepStage stage, const float* confs) {
    currentStage = stage;
    if (confs) {
        for (int i = 0; i < NN_OUTPUT_SIZE; i++) {
            confidences[i] = confs[i];
        }
    }
}

void WebDashboard::setBandPowers(const BandPowers& bp) {
    bands = bp;
}

void WebDashboard::setFilteredSample(float sample) {
    latestSample = sample;
}

void WebDashboard::setConfigCallback(std::function<void(const UserConfig&)> cb) {
    onConfigChange = cb;
}

String WebDashboard::getIPAddress() {
    if (configPtr && configPtr->apMode) {
        return WiFi.softAPIP().toString();
    }
    return WiFi.localIP().toString();
}

void WebDashboard::setupRoutes() {
    server.on("/", HTTP_GET, [this]() {
        server.send_P(200, "text/html", DASHBOARD_HTML);
    });
    
    server.on("/api/status", HTTP_GET, [this]() {
        JsonDocument doc;
        doc["stage"] = (int)currentStage;
        JsonArray conf = doc["conf"].to<JsonArray>();
        for (int i = 0; i < NN_OUTPUT_SIZE; i++) conf.add(confidences[i]);
        JsonObject b = doc["bands"].to<JsonObject>();
        b["d"] = bands.relDelta; b["t"] = bands.relTheta; b["a"] = bands.relAlpha; b["b"] = bands.relBeta; b["g"] = bands.relGamma;
        doc["eeg"] = latestSample;
        String res; serializeJson(doc, res);
        server.send(200, "application/json", res);
    });

    server.on("/api/sleep", HTTP_GET, [this]() {
        if (!sleepData || sleepDataCount == 0) {
            server.send(200, "application/json", "[]");
            return;
        }
        JsonDocument doc;
        JsonArray arr = doc.to<JsonArray>();
        int limit = (sleepDataCount > 200) ? 200 : sleepDataCount;
        for (int i = 0; i < limit; i++) {
            JsonObject obj = arr.add<JsonObject>();
            obj["stage"] = (int)sleepData[i].stage;
            obj["confidence"] = sleepData[i].confidence;
        }
        String res; serializeJson(doc, res);
        server.send(200, "application/json", res);
    });

    server.on("/api/config", HTTP_GET, [this]() {
        if (!configPtr) { server.send(500, "text/plain", "Config Null"); return; }
        JsonDocument doc;
        doc["minWakeHour"] = configPtr->minWakeHour; doc["minWakeMinute"] = configPtr->minWakeMinute;
        doc["maxWakeHour"] = configPtr->maxWakeHour; doc["maxWakeMinute"] = configPtr->maxWakeMinute;
        doc["alarmVolume"] = configPtr->alarmVolume; doc["wifiSSID"] = configPtr->wifiSSID;
        doc["wifiPassword"] = configPtr->wifiPassword; doc["apMode"] = configPtr->apMode; doc["saveRawEEG"] = configPtr->saveRawEEG;
        String res; serializeJson(doc, res);
        server.send(200, "application/json", res);
    });

    server.on("/api/config", HTTP_POST, [this]() {
        if (!configPtr) { server.send(500, "text/plain", "Config Null"); return; }
        String body = server.arg("plain");
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, body);
        if (err) { server.send(400, "text/plain", "Invalid JSON"); return; }
        if (doc.containsKey("minWakeHour")) configPtr->minWakeHour = doc["minWakeHour"];
        if (doc.containsKey("minWakeMinute")) configPtr->minWakeMinute = doc["minWakeMinute"];
        if (doc.containsKey("maxWakeHour")) configPtr->maxWakeHour = doc["maxWakeHour"];
        if (doc.containsKey("maxWakeMinute")) configPtr->maxWakeMinute = doc["maxWakeMinute"];
        if (doc.containsKey("alarmVolume")) configPtr->alarmVolume = doc["alarmVolume"];
        if (doc.containsKey("wifiSSID")) strlcpy(configPtr->wifiSSID, doc["wifiSSID"] | "RhythmSleep", sizeof(configPtr->wifiSSID));
        if (doc.containsKey("wifiPassword")) strlcpy(configPtr->wifiPassword, doc["wifiPassword"] | "sleep1234", sizeof(configPtr->wifiPassword));
        if (doc.containsKey("apMode")) configPtr->apMode = doc["apMode"];
        if (doc.containsKey("saveRawEEG")) configPtr->saveRawEEG = doc["saveRawEEG"];
        if (onConfigChange) onConfigChange(*configPtr);
        server.send(200, "application/json", "{\"status\":\"ok\"}");
    });
}
