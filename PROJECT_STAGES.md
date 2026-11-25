# LED_drawboard_fun - ESP32-S3 16x16 RGB LED Matrix

## Project overview
This project is a 16x16 RGB LED matrix controlled by an ESP32-S3. The long-term goal is a local, browser-driven display that can draw and animate without any cloud dependency.

This project was built in stages, beginning with hardware and power validation
before adding the current Wi-Fi dashboard, drawing studio, live synchronization,
and optional clock. The following milestone history and checks document that
progression.

## Current implementation status
**Stage 1: Basic LED Control - COMPLETED.**
**Stage 2: Power System - COMPLETED.**
**Stage 3: Wi-Fi and Networking - IMPLEMENTED AND ACCEPTED.**
**Stage 4: Basic Web Server - IMPLEMENTED AND ACCEPTED.**
**Stage 5: Drawing Interface - IMPLEMENTED AND ACCEPTED.**
**Stage 6: Live Communication - IMPLEMENTED AND ACCEPTED.**
**Stage 6.5: Optional Wi-Fi Clock - ANALOG AND BINARY TESTED ON HARDWARE.**
**Stage 6.6: Dashboard Tetris - IMPLEMENTED.**
**Stage 6.7: Scrolling Text - IMPLEMENTED.**

Completion confirmed by the hardware owner on 2026-11-17: the LED panel and
sleep/wake work, the fuse and 5 V supply are connected and working, and the
existing brightness limit is accepted. This records the owner's acceptance,
not new current or voltage-drop measurements by the firmware.

Stage 3 accepted by the hardware owner on 2026-11-17 after four successful
sleep/wake retests with Wi-Fi. The current firmware adds Stage 4: a local status
dashboard, effect selection, and playlist toggle. Stage 4 and its dropdown
polling fix were accepted by the hardware owner on 2026-11-17. This records
owner acceptance, not independent execution of every checklist item.
Stage 5 (drawing interface) was accepted by the hardware owner on 2026-11-18,
including the drawing/wake corrections and expanded effects catalogue.
This records owner acceptance, not independent execution of every checklist item.
Stage 6 (live communication / WebSockets) was accepted by the hardware owner
on 2026-11-18 after reboot, multi-device synchronization, and sleep/wake retests.
HTTP polling remains as a fallback.
Stage 6.5 adds optional analog and binary clocks. Effects remain the boot
default; either clock can be selected in the dashboard only after Wi-Fi
connects and NTP time is valid. Its configured timezone is Europe/Berlin with
automatic DST rules.

The design is intentionally tight and test-driven:
- verify one RGB LED works on the selected data line
- confirm the ESP32-S3 GPIO and level shifter can drive the matrix safely
- validate the 5 V power setup and common ground
- test serpentine or straight-row mapping across all 256 pixels
- enforce a safe maximum brightness so the matrix cannot exceed the 12 A 5 V supply limit
- add a wake/sleep button and idle timeout for the board to enter deep sleep without damaging the panel
- prevent adding Wi‑Fi or browser work until the hardware is proven stable

## Stage 1 goals (completed)
- Test a single RGB LED with red, green, blue, and white output
- Verify the chosen ESP32-S3 GPIO is valid for the LED data line
- Confirm the 3.3 V to 5 V logic conversion path is working
- Connect the full 16x16 panel and validate every pixel
- Check coordinate mapping for row order and serpentine wiring
- Establish a repeatable baseline before Stage 2 power and Stage 3 Wi‑Fi work
- Keep the software brightness below the safe ceiling dictated by the LPV-100-5 5 V / 12 A supply
- Provide a reliable hardware sleep path using a momentary button and a 60-minute idle timer

## Wake and sleep behavior
The effects build retains the validated power-saving behavior:

- A momentary button on GPIO 14 is used as a wake/sleep input.
- Hold the button for 5 seconds, then release it to clear the display and enter ESP32 deep sleep. Waiting for release prevents immediate wake-up from the still-active LOW signal.
- GPIO14 uses the internal input pull-up while awake and the RTC pull-up during deep sleep. Button polling continues during LED pattern display times.
- Press again to wake. The board restores the saved LED frame and selected effect from RTC memory, returns GPIO14 to digital input mode, and resumes animation. Release the wake press before beginning another sleep gesture; this release does not skip effects.
- If the display remains unchanged for more than 60 minutes, the device also enters deep sleep using the same wake routine.

The firmware stores the current frame, effect selection, and animation elapsed
time in RTC memory. This survives deep sleep, not power removal.

## LED effects playground (before Stage 3)

The old repeating hardware test is replaced by nine animated effects:
rainbow, confetti, comet, plasma, bouncing scanner, ocean waves, fireworks,
color-changing fish, and a walking cat. Fireworks and fish were added after
Stage 5 owner testing; the cat was added during Stage 6.

- Effects change automatically every 20 seconds when the playlist is enabled.
- A short GPIO14 button press advances on release and restarts the 20-second timer.
- A 5-second hold requests sleep instead of skipping an effect.
- Rendering is scheduled every 20 ms without blocking animation delays.
- Startup brightness is now 15/255 with a 70/255 operating cap (owner update).
- Animations count as LED activity, so the unchanged 60-minute idle timeout
  does not expire while effects are playing.

See [effects and button testing](README_STAGE1.md) for controls and checks.

All six effects were confirmed working by the hardware owner on 2026-11-17.

### Recorded requirement: web effect selection

The local web interface now allows selection of all nine existing effects
by name: rainbow, confetti, comet, plasma, bouncing scanner, and ocean waves.
The catalogue also includes fireworks, color-changing fish, and a walking cat. It includes a choice between playing one selected effect and the automatic
playlist. Physical button controls, brightness ceiling, and sleep/wake are
retained. Web controls were brought forward from Stage 5 by owner request.

## Stage 3: Wi-Fi and networking - IMPLEMENTED AND ACCEPTED

Goal: connect reliably to the local 2.4 GHz Wi-Fi network without interrupting
LED effects, button input, or sleep/wake. No web server or drawing UI yet.

1. **Local credentials:** use an untracked `include/secrets.h` based on
   [the example](include/secrets.example.h). Never log or commit the password.
   Report missing or placeholder configuration clearly.
2. **Station connection:** connect to the existing router using DHCP and a
   descriptive hostname, `led-drawboard`. Do not wait in a blocking connection
   loop; effects and button handling must work while connecting or offline.
3. **Connection diagnostics:** log connecting, connected, disconnected, and
   retry states at 115200 baud. On connection, print the assigned IP address,
   signal strength, and station MAC address. Show network information over
   serial for this stage; keep the matrix playing effects.
4. **Recovery:** bound connection attempts and use timed reconnect attempts
   with a capped backoff. An unavailable router or incorrect password must
   produce useful diagnostics without freezing effects or continuously rebooting.
5. **Sleep integration:** stop Wi-Fi cleanly before deep sleep and reconnect
   after button wake. Preserve the selected effect, ignore the wake press as
   an effect-skip gesture, and retain the GPIO4 LOW hold while asleep.
6. **Stable address:** document a router-side DHCP reservation using the
   printed station MAC address. Prefer this over hard-coding a static IP in
   firmware. A hostname alone does not guarantee `.local` discovery; mDNS is
   not required for this stage.

### Stage 3 setup and behavior

- Copy `include/secrets.example.h` to the ignored `include/secrets.h` and put
  local credentials there. Keep the example's placeholders in Git. Missing,
  empty, oversized, or placeholder credentials are reported over serial and
  leave the effects running offline.
- The hostname is `led-drawboard`. Use a reachable 2.4 GHz network and DHCP.
  The firmware starts a connection without waiting in a connection loop.
- Each attempt has a 15-second deadline, including acquisition of an IP address.
  Failed attempts turn the radio off between retries, with delays of 5, 10,
  20, 40, then at most 60 seconds. A successful connection resets the backoff.
- At 115200 baud, look for `[WiFi] Connected` followed by the IP, RSSI, and
  station MAC. Disconnection logs include the ESP-IDF reason code when available.
  Credentials are not printed or intentionally saved to Wi-Fi flash storage.
- Sleep stops networking after the LEDs are cleared and GPIO4 is held LOW.
  Button wake boots the station connection again while effects resume.
- In the router's DHCP/LAN settings, find the printed station MAC, reserve an
  unused address on the same subnet, then reboot the board and verify the IP.
  Router menu names vary. No static IP or router settings are changed by firmware.
- Stage 3 itself added no web page. Stage 4 now serves a dashboard at the
  printed IP. `.local` discovery is not implemented.

### Stage 3 acceptance checks

The first hardware test connected successfully and restored the selected effect
after deep sleep, but then reported `TG1WDT_SYS_RST` during Wi-Fi startup.
The saved PC resolved to the double-exception
vector, not a specific application function. The follow-up build disables
FastLED's asynchronous RMT4 worker and explicitly waits for LED transmission
completion before handing GPIO4 to RTC sleep control. The owner confirmed four
successful sleep/wake retests and accepted Stage 3. The exact original watchdog
cause was not proven; do not remove the transmission synchronization.

Original checklist retained for future regression testing (the acceptance report
does not separately document router-outage or DHCP-reservation testing):

- Cold boot connects and prints a usable LAN IP, MAC address, and signal strength.
- LEDs animate and the short/long button gestures work during connection attempts.
- Router off at boot: effects remain responsive and connection retries are logged.
- Router returns: the board connects without a manual reset.
- Router disconnects during operation: the board detects the loss and reconnects.
- Incorrect credentials: explicit diagnostics and bounded retries, no freezes.
- Sleep from a connected or offline state: all LEDs remain dark; button wake
  resumes the selected effect and reconnects when the network is available.
- With a DHCP reservation configured, the same address is assigned after reboot.

## Stage 4: Local web dashboard - IMPLEMENTED AND ACCEPTED

Open `http://<board IP>/` from a phone or computer on the same LAN; use HTTP,
not HTTPS. The serial monitor prints `[HTTP] Dashboard: http://.../`.
No extra libraries, cloud service, filesystem upload, or internet access are
needed. HTML, CSS, and JavaScript are served from firmware flash.

- Dashboard shows current effect and playback mode, IP, hostname, Wi-Fi RSSI,
  station MAC, matrix dimensions, brightness/ceiling, uptime, and free heap.
  These are software/network readings, not electrical health measurements.
- Choose any effect and click **Play selected effect** to enter single-effect
  mode. Enable **Automatic playlist** to cycle every 20 seconds. Changing mode
  restarts the 20-second timer. A physical short press advances one effect
  without changing the mode.
- Effect, mode, frame, and animation time survive deep sleep in RTC memory.
  A power cycle starts at rainbow with the playlist enabled.
- The page polls every 2 seconds and displays errors explicitly. While sleeping
  or disconnected, controls are disabled after a failed request. The page
  resumes polling after wake/reconnect; reopen it at the new IP if DHCP changes.
- The HTTP listener stops for sleep/network loss and starts after reconnection.
  The built-in synchronous Arduino WebServer serves one client at a time;
  slow/incomplete requests can temporarily delay animation/button polling.
  This is a small trusted-LAN dashboard, not a public or high-load server.
- No login or TLS is provided. Do not expose port 80 to the internet or use
  untrusted Wi-Fi. Credentials/SSID are never sent by dashboard endpoints.
  Control requests require a custom header and no cross-origin access is enabled.

### Stage 4 API

- `GET /`: dashboard.
- `GET /api/status`: JSON status plus `{id,name}` effect catalogue.
- `POST /api/effect`: form field `id`, exactly one canonical integer 0 through 8.
  Selects the effect and disables the automatic playlist.
- `POST /api/playlist`: form field `enabled`, exactly `true` or `false`.
- POST controls require `X-Drawboard-Control: 1`. Successful commands return
  the same status JSON. Invalid arguments return 400, missing header returns
  403, and unsupported routes/methods return 404. Errors are JSON with `error`.

### Stage 4 acceptance checks

1. Open the dashboard using the serial IP from desktop and phone browsers.
2. Select each of the nine effects and check the physical panel matches the name.
3. Keep single-effect mode for more than 20 seconds; it must not auto-advance.
4. Enable the playlist and verify 20-second advances and wraparound.
5. Short-press the button in both modes; the page must report the new effect.
6. Sleep/wake in single-effect mode: dark panel during sleep, same effect and
   mode on wake, Wi-Fi reconnects, and the open page recovers without reset.
7. Interrupt Wi-Fi, then restore it: dashboard reports unavailability and recovers.
8. Send invalid control values/missing headers: reject without changing playback.
9. Keep the page open while effects run; check button response and reset-free
   operation over several cycles. Current startup brightness is 15 with cap 70.

### Hardware feedback and Stage 4 UI fix (2026-11-17)

- The owner confirmed the dashboard is reachable on the main LAN at
  `http://192.168.178.152/` and that this address is now reserved in the router.
  The earlier guest-network address was inaccessible from the main LAN.
- Reported UI issue: the effect dropdown sometimes closes while open, making
  selection difficult. Routine 2-second polls were temporarily disabling all
  controls via the same busy flag used for commands.
- Stage 4 fix: background polling no longer disables controls. Only pending
  commands or an unavailable board disable them. Polls do not replace dropdown
  options or pending effect selections, and do not overwrite a focused playlist
  checkbox. Reads started before a command are discarded, including late errors,
  so stale status cannot overwrite a command result.
- Acceptance: keep the dropdown open across several polling intervals, then
  select and apply an effect on desktop and mobile. Verify status continues
  updating, commands are not duplicated, and outage/recovery still works.
- Validation: firmware build passed. An isolated JavaScript test of the embedded
  script passed passive polling without disabled-state writes, pending selection
  retention, all six effect commands, command locking, stale success/error
  rejection, focused checkbox preservation, and offline/recovery handling.
  Native browser click automation timed out; desktop/phone menu interaction and
  physical panel acceptance were left for the owner's hardware test.
- The owner subsequently confirmed "it works" and requested Stage 5 on
  2026-11-17. Stage 4 is accepted, including the dropdown polling fix.

## Stage 5: Drawing interface - IMPLEMENTED AND ACCEPTED

The owner chose browser-local editing followed by an explicit **Apply drawing**
command. No WebSockets or flash persistence are added in this stage.

- A 16 x 16 grid supports mouse/touch strokes, interpolated lines, a color picker,
  1/2/3-pixel square brushes, eraser, clear, and fill. Keyboard users can move
  with arrows and paint with Space/Enter. Top-left is (0, 0), X right, Y down.
  The firmware maps row-major browser colors through the existing serpentine
  coordinate mapping.
- Clear/fill/paint change only the browser draft, even when offline. Apply sends
  one complete frame and switches to static drawing mode with playlist disabled.
  Validate the whole payload before touching any pixel, so malformed frames
  cannot partially change the display. Pending commands are not duplicated.
- Drawing drafts are not synchronized with board status or other browsers and
  are lost on page reload. Applied drawings remain on the board until replaced
  by another drawing or an effect. Keeping a browser draft lets it be reapplied.
- Selecting an effect, enabling the playlist, or a physical short press exits
  drawing mode. A short press advances the remembered effect and preserves the
  disabled playlist. Disabling the playlist alone does not exit drawing mode.
- Brightness is an explicit integer command, allowed from 0 to 70 inclusive.
  The owner requested startup 15 and operating cap 70. Zero hides, but does not erase,
  framebuffer contents. Status reports actual brightness, not the startup constant.
- Current framebuffer, drawing mode, and brightness survive deep sleep via RTC.
  Power removal restores the startup effect/playlist and brightness.
  A static drawing now allows the existing 60-minute idle sleep timer to expire.
  Applied frames/brightness reset that timer; status reads and local edits do not.

### Stage 5 API additions

- `POST /api/frame`: exactly one form field `pixels` with 1536 hexadecimal digits:
  256 consecutive `RRGGBB` colors, no `#`, separators, or whitespace.
  Order is left-to-right, top-to-bottom. Both hex cases are accepted.
- `POST /api/brightness`: exactly one form field `value`, a canonical decimal
  integer from `0` through `70`; no signs, spaces, leading zeros, or fractions.
- Both require the existing `X-Drawboard-Control: 1` header and return status
  JSON on success; missing header is 403 and invalid input is 400.
- `GET /api/status` adds boolean `drawing` and reports the current `brightness`.
  The existing `effect` remains the remembered effect even while drawing.
  Existing effect/playlist endpoints remain available.

### Stage 5 acceptance checks

1. On desktop and phone, paint individual cells, diagonal strokes, brush sizes,
   eraser, clear, and fill. No physical change until Apply.
2. Apply distinct colors to (0,0), (15,0), (0,1), and (15,15); verify panel
   orientation and alternating row mapping. Test red, green, and blue channels.
3. Keep a drawing displayed for more than 20 seconds; no effect should overwrite it.
4. Apply another drawing, select an effect, enable the playlist, and short-press
   the physical button. Verify mode transitions and existing effect playback.
5. Test brightness 0, 15, and 70. Verify
   zero retains the image when brightness is raised again. Reject 71/255,
   negative/fractional values, and malformed frames without display changes.
6. Sleep/wake with a drawing and nondefault brightness: dark while sleeping,
   same image/mode/brightness on wake, browser reconnects, and button works.
7. Interrupt Wi-Fi while editing: draft is retained, Apply is disabled after
   connectivity failure, and reconnect does not discard the draft or selection.
8. Apply repeatedly while status polling; no duplicate commands or stale
   mode/brightness updates. Refreshing the page starts a new blank local draft,
   without clearing the physical image.

### Stage 5 software validation

- ESP32-S3 PlatformIO build passed: RAM 46,948 bytes; flash 835,845 bytes.
- Isolated browser execution of the embedded script passed grid initialization,
  keyboard painting, clipped 2/3-pixel brushes, eraser, line interpolation,
  clear/fill, exact 1536-digit RGB submission, Apply-only transmission, duplicate
  suppression, and retaining edits made while an Apply is pending.
- Script checks passed all six effect commands, brightness 0/32/198 and invalid
  browser values, pending selections through polling, stale read rejection,
  offline editing/recovery, and a 390-pixel layout without horizontal overflow.
- Synthetic pointer checks covered stroke continuation, pointer-up/cancel, and
  row-edge clipping; pointer capture was stubbed for these synthetic events.
  Native click/screenshot automation remained unresponsive in the shared browser.
  Real desktop/phone gestures, HTTP validation on the device, LED mapping,
  brightness output, and sleep/wake require the hardware checks above.

### Stage 5 owner feedback and corrections

- Owner confirmed drawing, switching back to effects, and static display for
  more than 20 seconds. The corrections below preceded final Stage 5 acceptance.
- Reduced startup brightness from 32 to 15 and operating maximum from 198 to
  70, both on the 0-255 scale. The calculated electrical ceiling remains separate.
- Owner observed browser top-left lighting physical top-right. Enabled the
  existing X-mirror setting and applied mirror transforms in the coordinate
  mapper before serpentine indexing. This corrects logical coordinates for
  drawings and coordinate-based effects; Y orientation remains unchanged.
- Wake clarification: the drawing was visible before sleep, but the panel was
  dark on wake. An additional button press then started an effect. This does
  not establish that wake itself changed modes. A trial of repeated static-frame
  transmission did not resolve the fault; owner logs confirmed drawing mode and
  brightness 15 survived, while the panel stayed dark.
- Root cause found in installed FastLED 3.10.9: the non-AVR `CRGB` default
  constructor zeros all channels. The RTC `CRGB` array was reconstructed during
  startup, clearing retained pixels while primitive mode/brightness survived.
  Retained pixels now use a raw byte array sized to the framebuffer, with no
  class constructor. Save/restore copy the same framebuffer bytes.
  Removed the ineffective repeated static refresh. Wake diagnostics now include
  the number of nonblack pixels as well as mode and brightness. Retest a drawing
  with known colored pixels through several sleep/wake cycles.
- Raw-byte retention fix builds successfully (flash 835,969 bytes; RAM 46,948
  bytes). ELF symbols confirm both live and RTC framebuffers are 768 bytes,
  with retained data in RTC memory. Physical wake restoration was subsequently
  confirmed by the owner.
- The owner subsequently confirmed drawing restoration after sleep/wake works.

### Additional effects after Stage 5 testing

- IDs 0-5 are unchanged. Added ID 6 **Fireworks** and ID 7
  **Color-changing fish** to the shared catalogue; the web selector, button
  cycling, playlist, API validation, and RTC effect range use that catalogue.
- Fireworks repeat a 2.8-second sequence: ascending rocket, colorful expanding
  sparks with downward drift and fading brightness, then a short black pause.
  Burst positions and colors vary between launches.
- The fish is a 7 x 5 sprite with a white eye and animated tail. It moves between
  bounded pseudo-random waypoints every 1.8 seconds, faces horizontal travel,
  and shifts body hue slowly over approximately 25.6 seconds.
- Both effects clear the framebuffer each frame. Only the current rocket/sparks
  or fish pixels are lit; empty background is exactly black, with no trails.
- Positions and colors derive from effect elapsed time, preserving motion phase
  through the existing RTC sleep/wake path without new retained class objects.
  Startup 15 and cap 70 remain unchanged. The owner accepted Stage 5 on
  2026-11-18 after these additions.
- Validation: ESP32-S3 build passed (flash 837,061 bytes, RAM 46,948 bytes).
  Catalogue checks preserved IDs 0-5 and verified all eight names. Source checks
  confirmed both renderers clear to black and contain no blocking delay.
  Geometry/timing models sampled 9,000 frames per effect: the fish stayed fully
  within the grid with 17-18 lit pixels, visited 110 positions, and faced both
  directions; rocket heights and spark brightness stayed within their ranges.
  These model checks are not a physical rendering test.
- On the panel, select each new effect individually. Check black background
  with no trails, rising/exploding fireworks, recognizable fish and slow color
  change, movement to both sides, playlist wrap after eight effects, and
  sleep/wake restoration. Keep brightness at the existing 15/70 settings.
- Updated firmware build passed (flash 835,945 bytes, RAM 46,948 bytes).
  Config/UI default checks and an exhaustive 256-coordinate mirrored-serpentine
  model passed. These checks do not replace physical orientation/wake tests.

## Power-limit note
The project uses a Mean Well LPV-100-5 5 V power supply with a 12 A maximum output. A full-white 16x16 WS2812B matrix can theoretically require:

256 LEDs × 60 mA = 15.36 A

The electrical calculation permits at most 198/255 (78% before integer rounding).
The owner's lower operating cap is now `MAX_ALLOWED_BRIGHTNESS = 70/255`,
with startup 15/255. A compile-time check keeps the operating cap below the
calculated electrical ceiling. The lower cap applies to commands and RTC restore.

The design intent is:
- keep the matrix within the safe power envelope of the LPV-100-5
- protect the wiring and power injection from overcurrent stress
- allow a useful brightness range during testing without exceeding the supply limit
- treat the brightness cap as a software safety feature, not a substitute for proper fusing, power injection, and grounding

## Files created for Stage 1
- platformio.ini
- .gitignore
- include/ProjectConfig.h
- include/PinConfig.h
- include/MatrixConfig.h
- include/secrets.example.h
- src/main.cpp
- README_STAGE1.md

## Completed Stage 1 validation sequence (historical)
These are the accepted hardware checks; the current firmware plays effects
instead of repeating these diagnostic patterns.

1. Power the board and LED matrix from a regulated 5 V source with a common ground.
2. Run the single-LED RGB test to confirm the line is driving proper colors.
3. Run the full-matrix sweep to check that every LED can be addressed correctly.
4. Run the coordinate verification pattern to confirm row/column mapping is correct.
5. Test the wake button by holding it for 5 seconds, then releasing it and confirming the matrix clears and sleeps.
6. Wake the board again with the same button and confirm the last LED frame is restored.
7. Keep the matrix brightness at or below the firmware limit during validation and increase only after the wiring is proven stable.

## Hardware notes
- Never power the matrix directly through ESP32-S3 GPIO or board headers.
- Use a proper level shifter for 3.3 V logic driving a 5 V LED matrix.
- Keep the data line close to the first LED and use a 300 to 500 ohm series resistor.
- Treat the matrix as a power system, not just a digital display.
- The firmware must clamp brightness to the configured maximum to prevent exceeding the 12 A supply limit.
- Use a normally-open button to GND with input pull-up so the board can wake reliably from deep sleep.
- Install a fuse on the 5 V rail feeding the matrix and logic circuitry.

## Full roadmap
### Stage 1: Basic LED control - COMPLETED
- one LED smoke test
- GPIO validation
- level shifter verification
- full matrix test
- coordinate mapping validation
- safe brightness limit enforcement
- wake/sleep button and idle shutdown testing

### Stage 2: Power system - COMPLETED
Accepted by the hardware owner: connected fuse, working 5 V supply, and correct
configured maximum brightness. Original power-system checklist for reference:

- 5 V supply and fusing
- bulk capacitor and injection points
- measurement of current and voltage drop
- software and hardware power margin validation

### Stage 3: Wi‑Fi and networking - IMPLEMENTED AND ACCEPTED
- non-blocking router connection with local, untracked credentials
- serial IP address, MAC address, signal strength, and connection diagnostics
- bounded connection attempts and timed reconnect behavior
- preserve effects, button response, and deep-sleep behavior
- router-side DHCP reservation

### Stage 4: Basic web server - IMPLEMENTED AND ACCEPTED
- status page
- device information
- matrix status and Wi‑Fi state
- named effect selection and automatic-playlist toggle (brought forward)

### Stage 5: Drawing interface - IMPLEMENTED AND ACCEPTED
- 16x16 browser grid
- color and brush tools
- clear/fill/erase and brightness control
- select any of the nine LED effects by name
- select a single effect or automatic playlist playback

### Stage 6: Live communication - IMPLEMENTED AND ACCEPTED
- WebSocket status and full-frame updates on port 81
- Live status/brightness/drawing/playlist/effect synchronization
- Full 1536-digit RGB frame synchronization for applied drawings and animated frames
- Added effect ID 8, **Walking cat**: a small side-view cat walks between repeatable pseudo-random waypoints, alternates its legs, and swishes its tail. It fits within the 16x16 matrix and resumes from elapsed animation time after sleep/wake.
- Browser reconnection with exponential backoff and a 15-second heartbeat
- Existing HTTP polling remains as a 10-second fallback and recovery path
- Incoming WebSocket controls validate effect, playlist, brightness, and frame payloads
- Sleep/Wi-Fi disconnect stops both HTTP and WebSocket listeners and restarts them after reconnect

Stage 6 acceptance checks:
1. Open the dashboard and confirm status changes appear immediately without waiting for HTTP polling.
2. Apply a drawing and confirm other open browser sessions receive the full synchronized frame.
3. Change effect, playlist, or brightness and confirm all live sessions update together.
4. Disconnect Wi-Fi or put the board to sleep; confirm live status becomes unavailable and then reconnects.
5. Leave the page open through a wake cycle and verify the board's restored state is reannounced over WebSocket.
6. Use invalid WebSocket frame/brightness/effect payloads and verify the display is unchanged.
7. Select Walking cat and confirm it is recognizable, remains fully on the matrix, faces both directions, and animates through the playlist and sleep/wake cycle.

### Stage 6 owner acceptance (2026-11-18)

- After the network startup-order fix, the board no longer enters the repeated
  reboot/assertion cycle (`tcpip_send_msg_wait_sem: Invalid mbox`).
- The Walking cat effect works on the physical matrix.
- The dashboard reports live LED status correctly in browsers on two separate
  devices at the same time.
- Restarting from deep sleep works, and the browser/network connection recovers.
- These are owner-reported hardware checks; they do not imply that every
  individual Stage 6 checklist item was separately exercised.

### Stage 6.5: Optional analog and binary clocks

- Start SNTP after Wi-Fi connects, using `pool.ntp.org` and
  `time.cloudflare.com`; do not delay startup or the effects while waiting.
- Use the Europe/Berlin timezone with automatic CET/CEST daylight-saving
  transitions.
- Keep the existing Rainbow/playlist startup behavior. Do not select Clock
  automatically at boot or after a power cycle.
- Enable clock style selection and a **Show clock** dashboard control only after the
  device has synchronized a valid time. The option is available only through
  the online dashboard; existing effect and drawing controls remain available.
- Render a 12-hour analog face on the 16x16 matrix with hour markers, hour and
  minute hands, and a moving second indicator. Keep the background black.
- Also offer a binary face with three colored columns for 24-hour hours,
  minutes, and seconds. Each column reads top-to-bottom as 32, 16, 8, 4, 2, 1;
  bright markers are set bits and dim markers are clear bits.
- While Clock is selected, disable automatic-playlist advancement. Selecting
  an effect, enabling the playlist, applying a drawing, or short-pressing the
  physical button exits Clock mode.
- Preserve Clock mode through deep sleep/wake using RTC state. On power-up,
  resume the normal effects startup instead of restoring Clock automatically.
- Live status reports clock availability, mode, and local time.

Stage 6.5 validation:
1. Boot offline and verify effects start as before.
2. Connect to Wi-Fi and verify effects continue during NTP synchronization;
   the dashboard clock control stays disabled until valid time is available.
3. After synchronization, enable Clock from the dashboard and check the time
   advances, hands move, and the second indicator moves.
4. Verify local time after an NTP sync and around a CET/CEST DST transition or
   with an appropriate test clock.
5. Select an effect, enable the playlist, apply a drawing, and short-press the
   physical button; each action should exit Clock mode.
6. Sleep/wake while Clock is active; confirm it resumes with current time.
7. Power-cycle while Clock is active; confirm the normal effects/playlist
   startup behavior is unchanged.

### Stage 6.5 analog clock owner acceptance (2026-11-18)

The hardware owner confirmed that all Stage 6.5 validation points work,
including NTP clock availability and display, clock mode transitions,
sleep/wake, and unchanged effects startup after a power cycle.

The hardware owner reports that the binary clock is functioning and has been
tested on the physical matrix. As a bit-order reference, 12:34:56 should show
hours `001100`, minutes `100010`, and seconds `111000` from top to bottom.

### Stage 6.6: Dashboard Tetris - IMPLEMENTED

- Play on a 10-column by 16-row field at the left of the 16x16 matrix.
- Support all seven tetrominoes, rotation, movement, soft and hard drops,
  collision and line clearing, score, level progression, pause, restart, and
  game-over status.
- Show the next piece and live score, level, and line count in the dashboard.
- Accept on-screen controls and keyboard input (arrows, Space, P, and R).
- Starting a game exits the current display mode and disables the automatic
  effects playlist.

### Stage 6.7: Scrolling Text - IMPLEMENTED

- Enter up to 64 characters and preview the rendered message at matrix
  resolution in the browser.
- Configure foreground/background colors, compact/normal/large font,
  left/right direction, speed from 1 through 10, and before/after pauses up to
  5000 ms.
- Support one pass, a fixed repeat count from 1 through 20, or continuous
  looping, with start, stop, and clear controls.
- Keep locally edited settings intact during live status updates. Settings,
  including speed, are sent to the board when **Start scrolling** is pressed.
- Keep the dashboard preview's glyph sizing and compact-font column mapping
  aligned with the firmware renderer.

Validation notes: Tetris and scrolling text have been built into the firmware.
The dashboard and firmware expose the controls described above. Physical
matrix behavior should still be checked after uploading a new build; no
additional hardware acceptance is claimed here.

### Stage 7: Storage and persistence
- save/restore image state
- configuration persistence
- versioned storage validation

### Stage 8: Enclosure and commissioning
- hardware installation
- safety checks
- final documentation and deployment

## Tools and environment
This Stage 1 implementation is designed for PlatformIO with the Arduino framework and the FastLED library.

## Quick start
1. Install PlatformIO.
2. Open the project folder.
3. Confirm the data pin in `include/PinConfig.h` matches your board and wiring.
4. Build and upload the firmware.
5. Confirm the serial monitor reports the safe brightness ceiling for the LPV-100-5 5 V / 12 A supply.
6. Watch the effects playlist; the serial monitor prints each selected effect.
7. Hold the wake button for 5 seconds, release to sleep, then press again to test wake recovery.
8. Configure local credentials in ignored `include/secrets.h`, rebuild/upload,
   and confirm `[WiFi] Connected` with an IP address at 115200 baud.

## Important design principle
The overall product is ambitious, but the best way to make it reliable is to validate the hardware first. This repository now reflects that principle directly, including a programmable brightness ceiling, proper fusing, and conservative power budgeting for the LPV-100-5 supply.

## Related docs
- README_STAGE1.md
- include/ProjectConfig.h
- include/MatrixConfig.h
- include/PinConfig.h
