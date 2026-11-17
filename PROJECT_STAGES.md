# LED_drawboard_fun - ESP32-S3 16x16 RGB LED Matrix

## Project overview
This project is a 16x16 RGB LED matrix controlled by an ESP32-S3. The long-term goal is a local, browser-driven display that can draw and animate without any cloud dependency.

This repository is now intentionally following a phased implementation plan. The first milestone is not the full app; it is a hardware validation stage that proves the LED driver, GPIO wiring, level-shifting path, power distribution, and matrix mapping before any later network features are added.

## Current implementation status
**Stage 1: Basic LED Control - COMPLETED.**
**Stage 2: Power System - COMPLETED.**
**Stage 3: Wi-Fi and Networking - IMPLEMENTED AND ACCEPTED.**

Completion confirmed by the hardware owner on 2026-11-17: the LED panel and
sleep/wake work, the fuse and 5 V supply are connected and working, and the
existing brightness limit is accepted. This records the owner's acceptance,
not new current or voltage-drop measurements by the firmware.

Stage 3 accepted by the hardware owner on 2026-11-17 after four successful
sleep/wake retests with Wi-Fi. The current firmware adds Stage 4: a local status
dashboard, effect selection, and playlist toggle. Stage 4 is implemented,
pending on-device browser and regression tests. Drawing and WebSockets remain
later milestones.

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

The old repeating hardware test is replaced by six animated effects:
rainbow, confetti, comet, plasma, bouncing scanner, and ocean waves.

- Effects change automatically every 20 seconds when the playlist is enabled.
- A short GPIO14 button press advances on release and restarts the 20-second timer.
- A 5-second hold requests sleep instead of skipping an effect.
- Rendering is scheduled every 20 ms without blocking animation delays.
- Startup brightness remains 32/255; the existing 198/255 ceiling is unchanged.
- Animations count as LED activity, so the unchanged 60-minute idle timeout
  does not expire while effects are playing.

See [effects and button testing](README_STAGE1.md) for controls and checks.

All six effects were confirmed working by the hardware owner on 2026-11-17.

### Recorded requirement: web effect selection

The Stage 4 local web interface now allows selection of all six existing effects
by name: rainbow, confetti, comet, plasma, bouncing scanner, and ocean waves.
It includes a choice between playing one selected effect and the automatic
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

## Stage 4: Local web dashboard - IMPLEMENTED, HARDWARE CHECKS PENDING

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
- `POST /api/effect`: form field `id`, exactly one canonical integer 0 through 5.
  Selects the effect and disables the automatic playlist.
- `POST /api/playlist`: form field `enabled`, exactly `true` or `false`.
- POST controls require `X-Drawboard-Control: 1`. Successful commands return
  the same status JSON. Invalid arguments return 400, missing header returns
  403, and unsupported routes/methods return 404. Errors are JSON with `error`.

### Stage 4 acceptance checks

1. Open the dashboard using the serial IP from desktop and phone browsers.
2. Select each of six effects and check the physical panel matches the name.
3. Keep single-effect mode for more than 20 seconds; it must not auto-advance.
4. Enable the playlist and verify 20-second advances and wraparound.
5. Short-press the button in both modes; the page must report the new effect.
6. Sleep/wake in single-effect mode: dark panel during sleep, same effect and
   mode on wake, Wi-Fi reconnects, and the open page recovers without reset.
7. Interrupt Wi-Fi, then restore it: dashboard reports unavailability and recovers.
8. Send invalid control values/missing headers: reject without changing playback.
9. Keep the page open while effects run; check button response and reset-free
   operation over several cycles. Brightness remains 32 with ceiling 198.

### Hardware feedback and next-stage UI follow-up (2026-11-17)

- The owner confirmed the dashboard is reachable on the main LAN at
  `http://192.168.178.152/` and that this address is now reserved in the router.
  The earlier guest-network address was inaccessible from the main LAN.
- Reported UI issue: the effect dropdown sometimes closes while open, making
  selection difficult. Record this for the next stage; no UI behavior change
  is included in this documentation update.
- Code inspection identifies a likely cause: each 2-second status poll sets
  the shared `busy` flag and calls `enableControls()`, temporarily disabling
  the effect selector and other controls even for a routine background read.
  Disabling an open native select can dismiss its menu.
- Next-stage fix: separate background polling from control-command state.
  Healthy status refreshes must not disable focused/open controls, reset
  pending selections, or overwrite in-progress edits. Retain appropriate
  command-in-flight protection and explicit offline/error feedback.
- Acceptance: keep the dropdown open across several polling intervals, then
  select and apply an effect on desktop and mobile. Verify status continues
  updating, commands are not duplicated, and outage/recovery still works.

## Power-limit note
The project uses a Mean Well LPV-100-5 5 V power supply with a 12 A maximum output. A full-white 16x16 WS2812B matrix can theoretically require:

256 LEDs × 60 mA = 15.36 A

This is above the supply capability, so the firmware enforces a software brightness limit of 78% of full scale. The configured maximum brightness is `MAX_ALLOWED_BRIGHTNESS = 198/255`, which keeps the matrix below the 12 A ceiling while still providing strong output.

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

### Stage 4: Basic web server - IMPLEMENTED, HARDWARE CHECKS PENDING
- status page
- device information
- matrix status and Wi‑Fi state
- named effect selection and automatic-playlist toggle (brought forward)

### Stage 5: Drawing interface
- 16x16 browser grid
- color and brush tools
- clear/fill/erase and brightness control
- select any of the six validated LED effects by name
- select a single effect or automatic playlist playback

### Stage 6: Live communication
- WebSocket updates
- frame synchronization
- connection health handling

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
