#pragma once

const char HTML_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="id">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Smart Coffee Dehumidifier</title>
<link rel="stylesheet"
 href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.4.0/css/all.min.css">
<style>
:root{
  --bg:#EBE3D5; --text:#3E2723; --muted:#8D6E63;
  --red:#E20613; --green:#2E7D32;
  --light:#ffffff; --dark:#c7c1b5;
}
*{ margin:0; padding:0; box-sizing:border-box;
   font-family:Segoe UI,Tahoma,Geneva,Verdana,sans-serif; }
body{ background:var(--bg); color:var(--text); padding:20px; }
.container{ max-width:1100px; margin:auto; }
.card{
  background:var(--bg); border-radius:20px; padding:20px; margin-bottom:20px;
  box-shadow: 8px 8px 16px var(--dark), -8px -8px 16px var(--light);
}
.grid{ display:grid; grid-template-columns:repeat(auto-fit,minmax(280px,1fr)); gap:20px; }
h1{ color:var(--red); margin-bottom:5px; }
h2{ color:var(--muted); font-size:1rem; font-weight:500; }
h3{ color:var(--muted); font-size:0.95rem; font-weight:600; margin-bottom:15px; }
.value{ font-size:3rem; font-weight:900; color:var(--red); }
.label{ color:var(--muted); margin-bottom:10px; }
.row{ display:flex; justify-content:space-between; align-items:center; margin:10px 0; }
.badge{
  padding:6px 14px; border-radius:20px; font-weight:bold;
  box-shadow: inset 3px 3px 6px var(--dark), inset -3px -3px 6px var(--light);
}
.active{ color:var(--red); } .inactive{ color:var(--muted); }
button{
  width:100%; border:none; border-radius:12px; padding:12px;
  cursor:pointer; font-weight:bold; background:var(--bg); color:var(--text);
  box-shadow: 5px 5px 10px var(--dark), -5px -5px 10px var(--light);
  transition:opacity 0.2s;
}
button:active{ box-shadow: inset 4px 4px 8px var(--dark), inset -4px -4px 8px var(--light); }
button:disabled{ opacity:0.4; cursor:not-allowed; }
button.btn-danger{ color:var(--red); }
button.btn-sm{ width:auto; padding:8px 16px; font-size:0.85rem; }
button.btn-success{ color:var(--green); }
input[type="text"],input[type="password"],
input[type="number"],input[type="time"]{
  width:100%; border:none; border-radius:10px; padding:12px;
  margin-top:5px; margin-bottom:15px; background:var(--bg); color:var(--text);
  font-size:1rem;
  box-shadow: inset 4px 4px 8px var(--dark), inset -4px -4px 8px var(--light);
}
label{ font-size:0.85rem; color:var(--muted); display:block; }
#conn-status{
  font-size:0.75rem; padding:3px 10px; border-radius:20px;
  margin-left:10px; font-weight:bold; transition:background 0.3s;
}
#conn-status.online{ background:#c8e6c9; color:var(--green); }
#conn-status.offline{ background:#ffcdd2; color:var(--red); }
.sched-pill{
  font-size:0.72rem; padding:2px 10px; border-radius:20px; font-weight:bold;
  margin-left:8px; display:inline-block;
}
.sched-on { background:#c8e6c9; color:var(--green); }
.sched-off{ background:#ffcdd2; color:var(--red); }
.note{
  font-size:0.78rem; color:var(--muted); margin-bottom:15px;
  padding:8px 12px; border-radius:10px; background:rgba(0,0,0,0.05);
}
.log-wrap{
  overflow-x:auto; margin-top:10px; border-radius:12px; padding:10px;
  max-height:320px; overflow-y:auto;
  box-shadow: inset 3px 3px 8px var(--dark), inset -3px -3px 8px var(--light);
}
table{ width:100%; border-collapse:collapse; font-size:0.82rem; }
thead th{
  text-align:left; padding:8px 10px; color:var(--muted);
  font-weight:600; border-bottom:2px solid var(--dark); white-space:nowrap;
}
tbody tr:nth-child(even){ background:rgba(0,0,0,0.03); }
tbody td{ padding:7px 10px; border-bottom:1px solid var(--dark); white-space:nowrap; }
.fan-on{ color:var(--red); font-weight:bold; }
.fan-off{ color:var(--muted); }
.log-live{ display:inline-block; width:8px; height:8px; border-radius:50%;
           background:var(--green); animation:pulse 1.5s infinite; margin-right:6px; }
@keyframes pulse{ 0%,100%{opacity:1;} 50%{opacity:0.3;} }
.toast{
  position:fixed; bottom:-100px; left:50%; transform:translateX(-50%);
  background:var(--red); color:white; padding:12px 20px;
  border-radius:30px; transition:0.3s; z-index:999; white-space:nowrap;
}
.toast.show{ bottom:30px; }
</style>
</head>
<body>
<div class="container">

<!-- HEADER -->
<div class="card">
  <div style="display:flex;align-items:center;justify-content:space-between;flex-wrap:wrap;gap:12px;">
    <div style="display:flex;align-items:center;gap:16px;">
      <i class="fa-solid fa-mug-hot" style="font-size:2.5rem;color:var(--red);"></i>
      <div>
        <h1 style="margin-bottom:2px;">Smart Coffee Dehumidifier</h1>
        <h2>Tel-U Purwokerto &mdash; Industrial IoT</h2>
      </div>
    </div>
    <div style="display:flex;flex-direction:column;align-items:flex-end;gap:4px;">
      <span id="conn-status" class="offline">&#9679; OFFLINE</span>
      <div id="rtc-time" style="font-size:1.4rem;font-weight:bold;">--:--:--</div>
      <div id="rtc-date" style="color:var(--muted);font-size:0.9rem;">----/--/--</div>
    </div>
  </div>
</div>

<!-- SENSOR GRID -->
<div class="grid">

<div class="card">
  <div class="label">Kelembapan</div>
  <div class="value"><span id="hum">--</span>%</div>
  <div id="hum-status" style="color:var(--muted);margin-top:6px;">Memuat...</div>
</div>

<div class="card">
  <div class="label">Suhu</div>
  <div class="value"><span id="temp">--</span>&deg;C</div>
</div>

<div class="card">
  <div class="row"><span>Fan</span><span id="fan-badge" class="badge inactive">OFF</span></div>
  <div class="row"><span>Mode</span><span id="mode-badge" class="badge active">AUTO</span></div>
  <div class="row">
    <span>Jadwal
      <span id="sched-pill" class="sched-pill sched-off">IDLE</span>
    </span>
  </div>
  <div class="row"><span>WiFi</span><span id="wifi-mode">--</span></div>
  <div class="row"><span>IP</span><span id="ip-address">--</span></div>
</div>

<!-- CONTROLS -->
<div class="card">
  <div style="margin-bottom:15px;"><button onclick="setMode('AUTO')">AUTO MODE</button></div>
  <div style="margin-bottom:15px;"><button onclick="setMode('MANUAL')">MANUAL MODE</button></div>
  <div style="margin-bottom:15px;">
    <button id="fan-on-btn" onclick="setFan('ON')">
      <i class="fa-solid fa-fan"></i> FAN ON
    </button>
  </div>
  <div><button id="fan-off-btn" onclick="setFan('OFF')">FAN OFF</button></div>
</div>

<!-- HYSTERESIS -->
<div class="card">
  <h3><i class="fa-solid fa-sliders"></i> Hysteresis</h3>
  <label>Min Humidity (%)</label>
  <input type="number" id="minHum" min="0" max="99" step="0.5">
  <label>Max Humidity (%)</label>
  <input type="number" id="maxHum" min="1" max="100" step="0.5">
  <button class="btn-success" onclick="saveHysteresis()">SAVE HYSTERESIS</button>
</div>

<!-- SCHEDULE -->
<div class="card">
  <h3><i class="fa-solid fa-clock"></i> Jadwal Operasi</h3>
  <label>Start Time</label>
  <input type="time" id="timeStart">
  <label>End Time</label>
  <input type="time" id="timeEnd">
  <button class="btn-success" onclick="saveSchedule()">SAVE SCHEDULE</button>
</div>

<!-- LCD DISPLAY SETTINGS -->
<div class="card">
  <h3><i class="fa-solid fa-display"></i> LCD Display</h3>
  <label>Interval Halaman (detik, 1&ndash;30)</label>
  <input type="number" id="lcdInterval" min="1" max="30" step="1" value="3">
  <p class="note">
    LCD menampilkan 5 halaman bergantian:<br>
    1&nbsp;Humidity+Fan &nbsp;|&nbsp; 2&nbsp;Suhu+Mode &nbsp;|&nbsp;
    3&nbsp;WiFi+IP &nbsp;|&nbsp; 4&nbsp;Jadwal &nbsp;|&nbsp; 5&nbsp;Tanggal+Waktu
  </p>
  <button class="btn-success" onclick="saveLcdInterval()">SAVE LCD INTERVAL</button>
</div>

</div><!-- end grid -->

<!-- WIFI SETTINGS -->
<div class="card">
  <h3><i class="fa-solid fa-wifi"></i> Pengaturan WiFi STA</h3>
  <p class="note">
    <i class="fa-solid fa-triangle-exclamation"></i>
    Device akan restart otomatis setelah menyimpan.
    Akses tetap via AP: <strong>192.168.4.1</strong>
  </p>
  <div style="display:grid;grid-template-columns:1fr 1fr;gap:15px;">
    <div>
      <label>SSID (Nama WiFi)</label>
      <input type="text" id="wifiSSID" placeholder="NamaWifi...">
    </div>
    <div>
      <label>Password</label>
      <input type="password" id="wifiPass" placeholder="Password...">
    </div>
  </div>
  <button class="btn-success" onclick="saveWifi()">
    <i class="fa-solid fa-floppy-disk"></i> SIMPAN &amp; RESTART
  </button>
</div>

<!-- LOG VIEWER -->
<div class="card">
  <div style="display:flex;justify-content:space-between;align-items:center;margin-bottom:15px;flex-wrap:wrap;gap:10px;">
    <h3 style="margin:0;">
      <span class="log-live" id="log-live-dot"></span>
      <i class="fa-solid fa-table-list"></i> Log Perubahan Kipas
    </h3>
    <div style="display:flex;gap:10px;flex-wrap:wrap;">
      <button class="btn-sm" onclick="fetchLog()">
        <i class="fa-solid fa-rotate"></i> Refresh
      </button>
      <button class="btn-sm btn-danger" onclick="resetLog()">
        <i class="fa-solid fa-trash"></i> Reset Log
      </button>
      <button class="btn-sm" onclick="downloadCSV()">
        <i class="fa-solid fa-download"></i> Download CSV
      </button>
      <button class="btn-sm btn-success" onclick="downloadExcel()">
        <i class="fa-solid fa-file-excel"></i> Download Excel
      </button>
    </div>
  </div>
  <div class="log-wrap">
    <table>
      <thead>
        <tr>
          <th>Waktu</th>
          <th>Kelembapan (%)</th>
          <th>Suhu (&deg;C)</th>
          <th>Fan</th>
          <th>Mode</th>
        </tr>
      </thead>
      <tbody id="log-tbody">
        <tr><td colspan="5" style="text-align:center;color:var(--muted);padding:20px;">
          Memuat log...
        </td></tr>
      </tbody>
    </table>
  </div>
  <div style="margin-top:8px;font-size:0.78rem;color:var(--muted);" id="log-count"></div>
</div>

</div><!-- end container -->
<div id="toast" class="toast"></div>

<script>let currentMode  = "AUTO";
let lastLogCount = -1;

// Dirty state — set saat user mengubah nilai, clear setelah save sukses
// Mencegah fetchData() overwrite input yang sudah diisi user
const dirtyFields = new Set();

document.querySelectorAll('input').forEach(el => {
  el.addEventListener('input', () => dirtyFields.add(el.id));
  // Tidak pakai 'change' karena input[type=number] fired saat arrow-key
});

// Helper: update field hanya jika tidak dirty
function safeSetInput(id, val) {
  if (!dirtyFields.has(id)) {
    document.getElementById(id).value = val ?? '';
  }
}

// ── Toast ──
function showToast(msg, color) {
  const t = document.getElementById("toast");
  t.innerText = msg;
  t.style.background = color || 'var(--red)';
  t.classList.add("show");
  setTimeout(() => t.classList.remove("show"), 3000);
}

// ── Connection ──
function setConnStatus(online) {
  const el = document.getElementById("conn-status");
  el.textContent = online ? "\u25CF ONLINE" : "\u25CF OFFLINE";
  el.className   = online ? "online" : "offline";
}

// ── Fetch sensor data (polls every 2s) ──
async function fetchData() {
  try {
    const res = await fetch('/api/status');
    const d   = await res.json();
    setConnStatus(true);

    const hum  = (typeof d.hum  === 'number' && isFinite(d.hum))
                 ? d.hum.toFixed(1)  : '--';
    const temp = (typeof d.temp === 'number' && isFinite(d.temp))
                 ? d.temp.toFixed(1) : '--';

    document.getElementById("hum").innerText        = hum;
    document.getElementById("temp").innerText       = temp;
    document.getElementById("rtc-time").innerText   = d.time || '--:--:--';
    document.getElementById("rtc-date").innerText   = d.date || '----/--/--';
    document.getElementById("ip-address").innerText = d.ip   || '--';
    document.getElementById("wifi-mode").innerText  = d.wifi_mode || '--';
    document.getElementById("fan-badge").innerText  = d.fan  || '--';
    document.getElementById("mode-badge").innerText = d.mode || '--';
    currentMode = d.mode;

    // Hum status
    if (typeof d.hum === 'number') {
      let s = "Normal";
      if (d.hum >= d.max_hum)      s = "\u26A0 Terlalu Tinggi";
      else if (d.hum <= d.min_hum) s = "\u2713 Aman";
      document.getElementById("hum-status").innerText = s;
    }

    document.getElementById("fan-badge").className =
      (d.fan === "ON") ? "badge active" : "badge inactive";

    const isAuto = (d.mode === "AUTO");
    document.getElementById("fan-on-btn").disabled  = isAuto;
    document.getElementById("fan-off-btn").disabled = isAuto;

    const pill = document.getElementById("sched-pill");
    if (d.sched_active) {
      pill.textContent = "ACTIVE";
      pill.className   = "sched-pill sched-on";
    } else {
      pill.textContent = "IDLE";
      pill.className   = "sched-pill sched-off";
    }

    // Gunakan safeSetInput — tidak overwrite field yang dirty
    safeSetInput("minHum",      d.min_hum);
    safeSetInput("maxHum",      d.max_hum);
    safeSetInput("timeStart",   d.time_start);
    safeSetInput("timeEnd",     d.time_end);
    safeSetInput("lcdInterval", d.lcd_interval);

    if (typeof d.log_count === 'number' && d.log_count !== lastLogCount) {
      lastLogCount = d.log_count;
      fetchLog();
    }

  } catch (e) {
    setConnStatus(false);
    console.warn("fetchData:", e);
  }
}

// ── Mode & Fan ──
async function setMode(mode) {
  try {
    await fetch('/api/control', {
      method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body: `mode=${mode}`
    });
    showToast("Mode: " + mode, '#5D4037');
    fetchData();
  } catch (e) { showToast("Gagal ubah mode"); }
}

async function setFan(state) {
  if (currentMode === "AUTO") { showToast("Harus MANUAL dulu!"); return; }
  try {
    await fetch('/api/control', {
      method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body: `fan=${state}`
    });
    showToast("Fan: " + state, '#5D4037');
    fetchData();
  } catch (e) { showToast("Gagal kontrol fan"); }
}

// ── Hysteresis ──
async function saveHysteresis() {
  const min = parseFloat(document.getElementById("minHum").value);
  const max = parseFloat(document.getElementById("maxHum").value);
  if (isNaN(min) || isNaN(max)) { showToast("Isi Min dan Max"); return; }
  if (min < 0 || max > 100)     { showToast("Nilai harus 0-100 %"); return; }
  if (min >= max)               { showToast("Min harus < Max"); return; }
  try {
    const res = await fetch('/api/settings/hysteresis', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ min, max })
    });
    const d = await res.json();
    if (d.ok) {
      // Clear dirty state setelah save berhasil
      dirtyFields.delete("minHum");
      dirtyFields.delete("maxHum");
      showToast("Hysteresis disimpan \u2713", '#2E7D32');
    } else {
      showToast("Error: " + (d.error || "Gagal"));
    }
  } catch (e) { showToast("Gagal simpan"); }
}

// ── Schedule ──
async function saveSchedule() {
  const start = document.getElementById("timeStart").value;
  const end   = document.getElementById("timeEnd").value;
  if (!start || !end) { showToast("Isi Start & End Time"); return; }
  try {
    const res = await fetch('/api/settings/schedule', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ start, end })
    });
    const d = await res.json();
    if (d.ok) {
      dirtyFields.delete("timeStart");
      dirtyFields.delete("timeEnd");
      showToast("Jadwal disimpan \u2713", '#2E7D32');
    } else {
      showToast("Error: " + (d.error || "Gagal"));
    }
  } catch (e) { showToast("Gagal simpan jadwal"); }
}

// ── LCD Interval ──
async function saveLcdInterval() {
  const val = parseInt(document.getElementById("lcdInterval").value);
  if (isNaN(val) || val < 1 || val > 30) { showToast("Interval harus 1-30 detik"); return; }
  try {
    const res = await fetch('/api/settings/lcd-interval', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ interval: val })
    });
    const d = await res.json();
    if (d.ok) {
      dirtyFields.delete("lcdInterval");
      showToast("LCD interval disimpan \u2713", '#2E7D32');
    } else {
      showToast("Error: " + (d.error || "Gagal"));
    }
  } catch (e) { showToast("Gagal simpan LCD interval"); }
}

// ── WiFi Settings ──
async function fetchWifiConfig() {
  try {
    const d = await (await fetch('/api/wifi/config')).json();
    if (d.ssid && d.ssid !== "YOUR_WIFI" && !dirtyFields.has("wifiSSID"))
      document.getElementById("wifiSSID").value = d.ssid;
  } catch (e) {}
}

async function saveWifi() {
  const ssid = document.getElementById("wifiSSID").value.trim();
  const pass = document.getElementById("wifiPass").value;
  if (!ssid)           { showToast("Masukkan SSID"); return; }
  if (ssid.length > 32){ showToast("SSID maks 32 karakter"); return; }
  try {
    const res = await fetch('/api/settings/wifi', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ ssid, pass })
    });
    const d = await res.json();
    if (d.ok) {
      dirtyFields.delete("wifiSSID");
      dirtyFields.delete("wifiPass");
      showToast(d.msg || "Tersimpan!", '#2E7D32');
    } else {
      showToast("Error: " + (d.error || "Gagal"));
    }
  } catch (e) { showToast("Gagal simpan WiFi"); }
}

// ── Log Viewer ── (tidak berubah dari sebelumnya)
async function fetchLog() {
  try {
    const text  = await (await fetch('/api/log')).text();
    const lines = text.trim().split('\n').filter(l => l.trim());
    const tbody = document.getElementById("log-tbody");

    if (lines.length <= 1) {
      tbody.innerHTML = '<tr><td colspan="5" style="text-align:center;color:var(--muted);padding:15px;">Log kosong</td></tr>';
      document.getElementById("log-count").innerText = "";
      return;
    }

    const rows = lines.slice(1).reverse();
    tbody.innerHTML = '';
    rows.forEach(line => {
      const c = line.split(',');
      if (c.length < 5) return;
      const fanVal = (c[3] || '').trim();
      const tr = document.createElement('tr');
      tr.innerHTML =
        `<td>${(c[0]||'').trim()}</td>` +
        `<td>${(c[1]||'').trim()}</td>` +
        `<td>${(c[2]||'').trim()}</td>` +
        `<td class="${fanVal==='ON'?'fan-on':'fan-off'}">${fanVal}</td>` +
        `<td>${(c[4]||'').trim()}</td>`;
      tbody.appendChild(tr);
    });

    document.getElementById("log-count").innerText =
      rows.length + " entri (terbaru di atas \u2014 auto-update aktif)";

  } catch (e) {
    showToast("Gagal muat log");
    console.warn("fetchLog:", e);
  }
}

async function resetLog() {
  if (!confirm("Yakin hapus semua log?\nAksi ini tidak bisa dibatalkan.")) return;
  try {
    await fetch('/api/log/reset', { method: 'POST' });
    showToast("Log direset \u2713", '#2E7D32');
    lastLogCount = 0;
    document.getElementById("log-tbody").innerHTML =
      '<tr><td colspan="5" style="text-align:center;color:var(--muted);padding:15px;">Log kosong</td></tr>';
    document.getElementById("log-count").innerText = "";
  } catch (e) { showToast("Gagal reset log"); }
}

function downloadCSV()   { window.location.href = '/download-log'; }
function downloadExcel() { window.location.href = '/download-excel'; }

// ── Init ──
fetchData();
fetchWifiConfig();
fetchLog();
setInterval(fetchData, 2000);
</script>
</body>
</html>
)rawliteral";