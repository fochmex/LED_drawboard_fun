# Completed hardware validation and LED effects

Stages 1 and 2 were accepted by the hardware owner on 2026-11-17. LED control,
sleep/wake, the connected fuse and 5 V supply, and the existing brightness
limit are confirmed working. Stage 3 Wi-Fi networking is accepted after four
sleep/wake retests. Stage 4 adds a local web dashboard, pending device acceptance.
See [network and dashboard setup](PROJECT_STAGES.md);
effects and button controls remain available while connecting or offline.

## LED effects and controls

The firmware now plays this repeating playlist instead of the hardware tests:

1. **Rainbow:** moving diagonal rainbow bands.
2. **Confetti:** colored sparkles fading into black.
3. **Comet:** a rainbow head and fading trail snaking across the panel.
4. **Plasma:** flowing two-dimensional color waves.
5. **Bouncing scanner:** a colored vertical bar sweeping left and right.
6. **Ocean waves:** moving blue/green brightness waves.

Effects change automatically every **20 seconds** by default. The web dashboard
can select a single effect or toggle the automatic playlist. **Short-press and release**
the GPIO14 button to skip to the next effect and restart that timer. The serial
monitor prints the effect name. Frames are scheduled every 20 ms; button polling
continues between frames.

Brightness is unchanged: startup 32/255, maximum 198/255. To adjust animation
timing, use `EFFECT_FRAME_INTERVAL_MS` and `EFFECT_DURATION_MS` in
[`include/ProjectConfig.h`](include/ProjectConfig.h).

## GPIO14 wake/sleep button

Connect a normally-open momentary button between **GPIO14 and ESP32 GND**.
The firmware enables the internal pull-up while awake and the RTC pull-up during
deep sleep: released reads HIGH, pressed reads LOW. Do not connect this input to 5 V.
On a four-leg tactile switch, use contacts that are disconnected when released;
some pairs of legs are permanently connected.

1. Open the serial monitor at 115200 baud.
2. Press the button. Confirm `Button pressed` appears during animation.
3. Hold continuously for at least 5 seconds. At `Sleep requested`, the current
   pattern pauses while the firmware waits for release.
4. Release. Confirm `Entering deep sleep`, the panel turns off, and tests stop.
5. Press again to wake. Confirm `Wake reason: external wake button`. The saved
   frame, effect, and playlist mode are restored; release before another gesture.
   The wake press does not skip an effect. Animation resumes with a fresh
   20-second automatic-cycle timer. A power cycle starts again at rainbow.

EXT0 wake is level-triggered, so sleep must wait for release to avoid immediate
wake-up. After wake, GPIO14 is returned from RTC mode to normal digital input.
Button polling continues during animation.

Before sleep, the firmware sends an all-off frame, waits for it to latch, and
holds GPIO4 LOW using the RTC GPIO hold. The hold is released before FastLED
initializes on wake. This prevents a floating level-shifter input from sending
spurious data to the still-powered LEDs during sleep.

The PlatformIO build selects `FL_RMT4_ASYNC=0` so FastLED completes each frame
without a background RMT transmission worker. Sleep also explicitly checks
`FastLED.wait(1000)` before rerouting GPIO4; a failed completion leaves the
device awake and reports an error. Do not replace this check with a 1 ms delay:
transmitting 256 RGB pixels alone takes about 7.7 ms.

Check several sleep/wake cycles, starting from different effects.
All 256 LEDs should remain dark throughout sleep, then the saved frame
should return on wake. If stray LEDs still appear, check the common ground and
level-shifter wiring. A 10 kohm pull-down from the level shifter's data input
(ESP32 side) to GND also keeps that input defined during reset, before firmware
can control the pin. Deep sleep does not disconnect the matrix's 5 V supply.

If no press is logged, check the GPIO14 header label, common ground, and switch
contacts. A startup raw level of LOW with the button released indicates a short
to ground or incorrect switch contacts. Repeated startup banners instead of test
headers suggest resets; capture the boot log and check power and reset wiring.

The 60-minute idle timeout uses the same sleep path. Animations continually
change the display, so they prevent the idle timeout from expiring.

## Effects acceptance checks

- Observe all six effects over two minutes; the playlist wraps back to rainbow.
- Short-press during each effect: advance once on release, without a reset.
- Skip just before an automatic transition: the next effect should remain for
  a fresh 20 seconds, not immediately skip again.
- Hold for 5 seconds and release: sleep without a short-press effect change.
- Wake while holding the button, then release: resume the same effect without
  skipping or immediately requesting sleep again.
- Repeat sleep/wake from confetti and comet: trails resume and all LEDs stay
  dark while asleep.
