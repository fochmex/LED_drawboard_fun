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
.tools{display:flex;flex-wrap:wrap;gap:12px;align-items:end}.tools label{margin:0}
.tools select{margin:8px 0 0}.tools input{display:block;max-width:100%;margin-top:8px}
input[type=number]{font:inherit;width:110px;padding:10px;background:#101522;color:inherit;border:1px solid #60748e;border-radius:8px}
#drawing-grid{display:grid;grid-template-columns:repeat(16,minmax(0,1fr));gap:2px;max-width:480px;margin:20px auto;touch-action:none;user-select:none}
#drawing-grid button{aspect-ratio:1;padding:0;min-width:0;border:1px solid #60748e;border-radius:2px}
#drawing-grid button:focus-visible{outline-offset:0;z-index:1}
.actions{display:flex;flex-wrap:wrap;gap:10px}#draft-status{min-height:1.5em}
</style>
</head>
<body>
<header><div><small>LOCAL LED CONTROLLER</small><h1>LED Drawboard</h1></div><span class="badge" id="connection" role="status">Connecting...</span></header>
<main>
<section><h2>Now playing</h2><p id="playing">Waiting for the board...</p>
<form id="effect-form"><label for="effect">Choose an effect</label><select id="effect" disabled></select>
<button id="apply" type="submit" disabled>Play selected effect</button></form>
<div class="tools"><label for="clock-style">Clock style<select id="clock-style" disabled>
<option value="analog">Analog</option><option value="binary">Binary</option></select></label>
<button id="clock-toggle" type="button" disabled>Show clock (waiting for time sync)</button></div>
<small id="clock-style-help">Binary columns read left to right: hours (orange), minutes (cyan), seconds (magenta).
Each reads top to bottom as 32, 16, 8, 4, 2, 1; bright markers are set bits and dim markers are clear bits.</small>
<label class="toggle"><input id="playlist" type="checkbox" disabled>Automatic playlist (20 seconds per effect)</label>
<p id="message" role="status" aria-live="polite"></p>
<small>Playing a selected effect turns off the playlist. Clock styles become available after Wi-Fi time synchronization.
A short physical button press advances one effect without changing the playback mode.</small>
</section>
<section><h2>Drawing studio</h2>
<p>Draw locally, then press <b>Apply drawing</b> to show it on the LEDs and pause effects.
Top-left is (0, 0). Browser colors are a preview; LED brightness and color correction differ.</p>
<div class="tools">
<label for="color">Color<input id="color" type="color" value="#ff0000"></label>
<label for="tool">Tool<select id="tool"><option value="brush">Brush</option><option value="erase">Eraser</option></select></label>
<label for="brush-size">Brush size<select id="brush-size"><option value="1">1 pixel</option><option value="2">2 x 2</option><option value="3">3 x 3</option></select></label>
</div>
<div id="drawing-grid" role="group" aria-label="16 by 16 drawing grid. Arrow keys move; Space or Enter paints."></div>
<div class="actions"><button id="clear" type="button">Clear draft</button><button id="fill" type="button">Fill with color</button>
<button id="apply-drawing" type="button" disabled>Apply drawing</button></div>
<p id="draft-status" role="status">Local draft only. Not sent to LEDs.</p>
<small>Edits are kept only in this open page, not saved on reload or synchronized with other browsers.
Clear and Fill edit the draft; Apply sends it. Selecting an effect, enabling the playlist,
showing the clock, or short-pressing the physical button leaves drawing mode.</small>
<form id="brightness-form"><label for="brightness-value">LED brightness (safe ceiling: <span id="brightness-limit">--</span>/255)</label>
<div class="tools"><input id="brightness-value" aria-label="LED brightness" type="number" min="0" max="70" step="1" value="15" required disabled>
<button id="apply-brightness" type="submit" disabled>Set brightness</button></div></form>
<small>0 turns the LEDs dark without erasing the drawing. Applied image and brightness survive deep sleep,
not power removal. A static drawing sleeps after 60 minutes without LED changes.</small>
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
let ready = false, commandPending = false, initialized = false;
let clockAvailable = false, clockMode = false;
let commandVersion = 0;
let liveSocket = null, liveHeartbeat = null, liveReconnectTimer = null;
let liveReconnectDelay = 1000;
function enableControls() {
  const disabled = !ready || commandPending;
  for (const id of ['effect', 'apply', 'playlist', 'apply-drawing', 'brightness-value', 'apply-brightness']) {
    if (byId(id).disabled !== disabled) byId(id).disabled = disabled;
  }
  byId('clock-style').disabled = disabled || !clockAvailable;
  byId('clock-toggle').disabled = disabled || (!clockAvailable && !clockMode);
  byId('clock-toggle').textContent = clockMode ? 'Return to effects' :
    (clockAvailable ? 'Show clock' : 'Show clock (waiting for time sync)');
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
  clockAvailable = data.clockAvailable;
  clockMode = data.clockMode;
  if (document.activeElement !== byId('clock-style')) byId('clock-style').value = data.clockStyle;
  byId('playing').textContent = data.clockMode ?
    `${data.clockStyle === 'binary' ? 'Binary' : 'Analog'} clock / ${data.clockTime || 'syncing'}` :
    (data.drawing ? 'Drawing / Static image' :
      `${data.effectName} / ${data.playlist ? 'Automatic playlist' : 'Single effect'}`);
  if (!initialized) {
    for (const effect of data.effects) {
      const option = document.createElement('option');
      option.value = effect.id; option.textContent = effect.name;
      byId('effect').appendChild(option);
    }
    byId('effect').value = data.effect;
    byId('brightness-value').value = data.brightness;
    initialized = true;
  }
  byId('brightness-value').max = data.maxBrightness;
  byId('brightness-limit').textContent = data.maxBrightness;
  if (document.activeElement !== byId('playlist')) byId('playlist').checked = data.playlist;
  for (const id of ['hostname', 'ip', 'mac']) byId(id).textContent = data[id];
  byId('rssi').textContent = `${data.rssi} dBm`;
  byId('matrix').textContent = `${data.width} x ${data.height} / ${data.pixels} pixels`;
  byId('brightness').textContent = `${data.brightness} / ${data.maxBrightness} (of 255)`;
  byId('uptime').textContent = `${Math.floor(data.uptimeMs / 1000)} seconds`;
  byId('heap').textContent = `${data.freeHeap} bytes`;
  enableControls();
}
async function control(path, values, successMessage = 'Playback updated.') {
  if (!ready || commandPending) return false;
  commandPending = true;
  ++commandVersion;
  enableControls();
  try {
    showStatus(await request(path, values));
    message(successMessage);
    return true;
  } catch (error) {
    ready = false;
    byId('connection').textContent = 'Unavailable';
    message(`Command not confirmed: ${error.message}. Check board status before retrying.`, true);
    return false;
  } finally { commandPending = false; enableControls(); }
}
byId('effect-form').addEventListener('submit', event => {
  event.preventDefault();
  if (ready && !commandPending) control('/api/effect', {id: byId('effect').value});
});
byId('playlist').addEventListener('change', () => {
  if (ready && !commandPending) control('/api/playlist', {enabled: String(byId('playlist').checked)});
});
byId('clock-toggle').addEventListener('click', () => {
  if (ready && !commandPending && clockAvailable) {
    control('/api/clock', {enabled: String(!clockMode)},
      clockMode ? 'Returned to effects.' : 'Clock enabled.');
  }
});
byId('clock-style').addEventListener('change', () => {
  if (ready && !commandPending && clockAvailable) {
    control('/api/clock-style', {style: byId('clock-style').value}, 'Clock style updated.');
  }
});
const draft = Array(256).fill('000000');
const cells = [];
let draftVersion = 0;
function refreshCell(index) {
  cells[index].style.backgroundColor = `#${draft[index]}`;
  cells[index].setAttribute('aria-label', `Pixel ${index % 16}, ${Math.floor(index / 16)}: #${draft[index]}`);
}
function draftChanged() {
  ++draftVersion;
  byId('draft-status').textContent = 'Local draft changed. Press Apply drawing to update LEDs.';
}
for (let index = 0; index < draft.length; ++index) {
  const cell = document.createElement('button');
  cell.type = 'button';
  cell.dataset.index = index;
  cell.tabIndex = index === 0 ? 0 : -1;
  cells.push(cell);
  refreshCell(index);
  byId('drawing-grid').appendChild(cell);
}
function paint(index) {
  const size = Number(byId('brush-size').value);
  const offset = Math.floor((size - 1) / 2);
  const color = byId('tool').value === 'erase' ? '000000' : byId('color').value.slice(1).toLowerCase();
  for (let dy = 0; dy < size; ++dy) {
    for (let dx = 0; dx < size; ++dx) {
      const x = index % 16 + dx - offset, y = Math.floor(index / 16) + dy - offset;
      if (x >= 0 && x < 16 && y >= 0 && y < 16) {
        const target = y * 16 + x;
        draft[target] = color;
        refreshCell(target);
      }
    }
  }
  draftChanged();
}
function paintLine(from, to) {
  let x = from % 16, y = Math.floor(from / 16);
  const endX = to % 16, endY = Math.floor(to / 16);
  const dx = Math.abs(endX - x), dy = -Math.abs(endY - y);
  const sx = x < endX ? 1 : -1, sy = y < endY ? 1 : -1;
  let error = dx + dy;
  while (true) {
    paint(y * 16 + x);
    if (x === endX && y === endY) break;
    const twice = 2 * error;
    if (twice >= dy) { error += dy; x += sx; }
    if (twice <= dx) { error += dx; y += sy; }
  }
}
let strokePointer = null, lastCell = null;
const drawingGrid = byId('drawing-grid');
function cellIndex(target) {
  return target && target.parentElement === drawingGrid ? Number(target.dataset.index) : null;
}
drawingGrid.addEventListener('pointerdown', event => {
  const index = cellIndex(event.target);
  if (event.button !== 0 || strokePointer !== null || index === null) return;
  event.preventDefault();
  strokePointer = event.pointerId;
  lastCell = index;
  drawingGrid.setPointerCapture(event.pointerId);
  cells[index].focus({preventScroll: true});
  paint(index);
});
drawingGrid.addEventListener('pointermove', event => {
  if (event.pointerId !== strokePointer) return;
  const index = cellIndex(document.elementFromPoint(event.clientX, event.clientY));
  if (index === null) { lastCell = null; return; }
  if (lastCell !== null) paintLine(lastCell, index);
  else paint(index);
  lastCell = index;
});
for (const name of ['pointerup', 'pointercancel', 'lostpointercapture']) {
  drawingGrid.addEventListener(name, event => {
    if (event.pointerId === strokePointer) { strokePointer = null; lastCell = null; }
  });
}
drawingGrid.addEventListener('focusin', event => {
  const index = cellIndex(event.target);
  if (index === null) return;
  for (const cell of cells) cell.tabIndex = -1;
  cells[index].tabIndex = 0;
});
drawingGrid.addEventListener('keydown', event => {
  const index = cellIndex(event.target);
  if (index === null) return;
  const x = index % 16, y = Math.floor(index / 16);
  const moves = {ArrowLeft: y * 16 + Math.max(0, x - 1), ArrowRight: y * 16 + Math.min(15, x + 1),
    ArrowUp: Math.max(0, y - 1) * 16 + x, ArrowDown: Math.min(15, y + 1) * 16 + x};
  if (event.key in moves) { event.preventDefault(); cells[moves[event.key]].focus(); }
  else if (event.key === ' ' || event.key === 'Enter') { event.preventDefault(); paint(index); }
});
for (const id of ['clear', 'fill']) {
  byId(id).addEventListener('click', () => {
    draft.fill(id === 'clear' ? '000000' : byId('color').value.slice(1).toLowerCase());
    draft.forEach((_, index) => refreshCell(index));
    draftChanged();
  });
}
byId('apply-drawing').addEventListener('click', async () => {
  const version = draftVersion;
  if (await control('/api/frame', {pixels: draft.join('')}, 'Drawing applied; effects paused.')) {
    if (version === draftVersion) byId('draft-status').textContent = 'Drawing applied to LEDs.';
  }
});
byId('brightness-form').addEventListener('submit', event => {
  event.preventDefault();
  if (byId('brightness-value').reportValidity()) {
    control('/api/brightness', {value: String(Number(byId('brightness-value').value))}, 'Brightness updated.');
  }
});
function syncFrameHex(hex) {
  if (!/^([0-9a-fA-F]{6}){256}$/.test(hex) || draftVersion !== 0) return;
  for (let index = 0; index < 256; ++index) {
    draft[index] = hex.slice(index * 6, index * 6 + 6).toLowerCase();
    refreshCell(index);
  }
  byId('draft-status').textContent = 'Drawing synchronized from the LEDs.';
}
function connectLive() {
  if (liveSocket && liveSocket.readyState <= 1) return;
  const protocol = location.protocol === 'https:' ? 'wss:' : 'ws:';
  liveSocket = new WebSocket(`${protocol}//${location.hostname}:81/`);
  liveSocket.addEventListener('open', () => {
    ready = true;
    liveReconnectDelay = 1000;
    byId('connection').textContent = 'Online / Live';
    enableControls();
    clearInterval(liveHeartbeat);
    liveHeartbeat = setInterval(() => {
      if (liveSocket && liveSocket.readyState === WebSocket.OPEN) {
        liveSocket.send(`ping:${Date.now()}`);
      }
    }, 15000);
  });
  liveSocket.addEventListener('message', event => {
    if (typeof event.data !== 'string') return;
    if (event.data.startsWith('S')) {
      try { showStatus(JSON.parse(event.data.slice(1))); }
      catch (error) { /* ignore malformed live status */ }
    } else if (event.data.startsWith('F')) {
      syncFrameHex(event.data.slice(1));
    }
  });
  liveSocket.addEventListener('close', () => {
    clearInterval(liveHeartbeat);
    if (liveReconnectTimer) clearTimeout(liveReconnectTimer);
    liveReconnectTimer = setTimeout(connectLive, liveReconnectDelay);
    liveReconnectDelay = Math.min(liveReconnectDelay * 2, 10000);
  });
  liveSocket.addEventListener('error', () => {
    byId('connection').textContent = 'Live unavailable';
  });
}
async function poll() {
  if (!commandPending) {
    const version = commandVersion;
    try {
      const data = await request('/api/status');
      // A read started before a command must not overwrite its result.
      if (version === commandVersion) showStatus(data);
    }
    catch (error) {
      if (version === commandVersion) {
        ready = false;
        byId('connection').textContent = 'Unavailable';
        message(`Board unavailable: ${error.message}. Waiting for wake or network recovery...`, true);
        enableControls();
      }
    }
  }
  setTimeout(poll, 10000);
}
connectLive();
poll();
</script>
</body>
</html>)HTML";
