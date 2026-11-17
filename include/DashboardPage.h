#pragma once

#include <Arduino.h>

static const char kDashboardPage[] PROGMEM = R"HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>LED Drawboard</title>
<style>
:root{color-scheme:dark;font:16px system-ui,sans-serif;background:#101522;color:#edf2fa}
*{box-sizing:border-box}body{max-width:900px;margin:auto;padding:32px 20px}
header{display:flex;justify-content:space-between;align-items:center;gap:16px;margin-bottom:28px}
h1{font-size:clamp(1.7rem,5vw,2.4rem);margin:0}h2{font-size:1.15rem;margin-top:0}
p{line-height:1.6;color:#b5c3d9}.badge{border:1px solid #576d87;border-radius:20px;padding:8px 14px;white-space:nowrap}
section{background:#1b2435;border:1px solid #34435a;border-radius:16px;padding:24px;margin-bottom:20px}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));gap:20px}
dl{margin:0}dt{font-size:.85rem;color:#b5c3d9;margin-bottom:6px}dd{margin:0;overflow-wrap:anywhere}
label{display:block;margin:18px 0 8px}select,button{font:inherit;border-radius:8px;padding:12px;border:1px solid #60748e}
select{background:#101522;color:inherit;width:100%;margin-bottom:12px}
button{background:#75e4ca;color:#101522;font-weight:650;cursor:pointer}button:disabled,select:disabled{opacity:.5;cursor:default}
.toggle{display:flex;align-items:center;gap:12px;line-height:1.5}.toggle input{width:20px;height:20px}
#message{min-height:1.5em;color:#9bead8}#message.error{color:#ffb4ad}
small{color:#b5c3d9}a{color:#75e4ca}:focus-visible{outline:3px solid #75e4ca;outline-offset:3px}
</style>
</head>
<body>
<header><div><small>LOCAL LED CONTROLLER</small><h1>LED Drawboard</h1></div><span class="badge" id="connection" role="status">Connecting...</span></header>
<main>
<section><h2>Now playing</h2><p id="playing">Waiting for the board...</p>
<form id="effect-form"><label for="effect">Choose an effect</label><select id="effect" disabled></select>
<button id="apply" type="submit" disabled>Play selected effect</button></form>
<label class="toggle"><input id="playlist" type="checkbox" disabled>Automatic playlist (20 seconds per effect)</label>
<p id="message" role="status" aria-live="polite"></p>
<small>Playing a selected effect turns off the playlist. A short physical button press advances one effect without changing the playback mode.</small>
</section>
<section><h2>Board status</h2><div class="grid">
<dl><dt>Device</dt><dd id="hostname">--</dd></dl><dl><dt>IP address</dt><dd id="ip">--</dd></dl>
<dl><dt>Wi-Fi signal</dt><dd id="rssi">--</dd></dl><dl><dt>Station MAC</dt><dd id="mac">--</dd></dl>
<dl><dt>Matrix</dt><dd id="matrix">--</dd></dl><dl><dt>Brightness / ceiling</dt><dd id="brightness">--</dd></dl>
<dl><dt>Uptime</dt><dd id="uptime">--</dd></dl><dl><dt>Free heap</dt><dd id="heap">--</dd></dl>
</div></section>
<section><h2>Sleep and wake</h2><p>Hold the physical button for 5 seconds, then release to sleep. Press again to wake. This page is unavailable while the board sleeps and reconnects automatically afterward.</p>
<small>Local network only. No account or encryption: use a trusted LAN and do not forward this device's HTTP port to the internet.</small></section>
</main>
<script>
'use strict';
const byId = id => document.getElementById(id);
let ready = false, busy = false, initialized = false;
function enableControls() {
  for (const id of ['effect', 'apply', 'playlist']) byId(id).disabled = !ready || busy;
}
function message(text, error = false) {
  byId('message').textContent = text;
  byId('message').className = error ? 'error' : '';
}
async function request(path, values) {
  const controller = new AbortController();
  const timeout = setTimeout(() => controller.abort(), 4000);
  try {
    const options = {signal: controller.signal, cache: 'no-store'};
    if (values) {
      options.method = 'POST';
      options.headers = {'X-Drawboard-Control': '1'};
      options.body = new URLSearchParams(values);
    }
    const response = await fetch(path, options);
    const data = await response.json();
    if (!response.ok) throw new Error(data.error || `HTTP ${response.status}`);
    return data;
  } finally { clearTimeout(timeout); }
}
function showStatus(data) {
  if (!ready && initialized) message('Connection restored.');
  ready = true;
  byId('connection').textContent = 'Online';
  byId('playing').textContent = `${data.effectName} / ${data.playlist ? 'Automatic playlist' : 'Single effect'}`;
  if (!initialized) {
    for (const effect of data.effects) {
      const option = document.createElement('option');
      option.value = effect.id; option.textContent = effect.name;
      byId('effect').appendChild(option);
    }
    byId('effect').value = data.effect;
    initialized = true;
  }
  byId('playlist').checked = data.playlist;
  for (const id of ['hostname', 'ip', 'mac']) byId(id).textContent = data[id];
  byId('rssi').textContent = `${data.rssi} dBm`;
  byId('matrix').textContent = `${data.width} x ${data.height} / ${data.pixels} pixels`;
  byId('brightness').textContent = `${data.brightness} / ${data.maxBrightness} (of 255)`;
  byId('uptime').textContent = `${Math.floor(data.uptimeMs / 1000)} seconds`;
  byId('heap').textContent = `${data.freeHeap} bytes`;
  enableControls();
}
async function control(path, values) {
  busy = true; enableControls();
  try {
    showStatus(await request(path, values));
    message('Playback updated.');
  } catch (error) {
    ready = false;
    byId('connection').textContent = 'Unavailable';
    message(`Command not confirmed: ${error.message}. Check board status before retrying.`, true);
  } finally { busy = false; enableControls(); }
}
byId('effect-form').addEventListener('submit', event => {
  event.preventDefault();
  if (ready && !busy) control('/api/effect', {id: byId('effect').value});
});
byId('playlist').addEventListener('change', () => {
  if (ready && !busy) control('/api/playlist', {enabled: String(byId('playlist').checked)});
});
async function poll() {
  if (!busy) {
    busy = true; enableControls();
    try { showStatus(await request('/api/status')); }
    catch (error) {
      ready = false;
      byId('connection').textContent = 'Unavailable';
      message(`Board unavailable: ${error.message}. Waiting for wake or network recovery...`, true);
    } finally { busy = false; enableControls(); }
  }
  setTimeout(poll, 2000);
}
poll();
</script>
</body>
</html>)HTML";
