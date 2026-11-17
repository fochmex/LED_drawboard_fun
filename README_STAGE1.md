# Stage 1 hardware validation

## Expected LED sequence

The firmware repeats these tests, with a 2.5-second blank gap between tests:

1. First LED only: red, green, blue, then white (0.5 seconds per color).
2. Full panel: color gradient for 1 second, then off.
3. Full panel: cyan/magenta checkerboard for 1 second, then off.

The two full-panel patterns followed by the first LED blinking are intentional.
This is not a reset unless the serial startup banner also repeats.

## GPIO14 wake/sleep button

Connect a normally-open momentary button between **GPIO14 and ESP32 GND**.
The firmware enables the internal pull-up while awake and the RTC pull-up during
deep sleep: released reads HIGH, pressed reads LOW. Do not connect this input to 5 V.
On a four-leg tactile switch, use contacts that are disconnected when released;
some pairs of legs are permanently connected.

1. Open the serial monitor at 115200 baud.
2. Press the button. Confirm `Button pressed` appears, including during LED tests.
3. Hold continuously for at least 5 seconds. At `Sleep requested`, the current
   pattern pauses while the firmware waits for release.
4. Release. Confirm `Entering deep sleep`, the panel turns off, and tests stop.
5. Press again to wake. Confirm `Wake reason: external wake button`. The saved
   frame is restored; release the button before starting another sleep gesture.
   The validation cycle restarts after wake and replaces the restored frame.

EXT0 wake is level-triggered, so sleep must wait for release to avoid immediate
wake-up. After wake, GPIO14 is returned from RTC mode to normal digital input.
Button polling continues during pattern display times.

If no press is logged, check the GPIO14 header label, common ground, and switch
contacts. A startup raw level of LOW with the button released indicates a short
to ground or incorrect switch contacts. Repeated startup banners instead of test
headers suggest resets; capture the boot log and check power and reset wiring.

The 60-minute idle timeout uses the same sleep path. Repeating tests continually
change the display, so they prevent the idle timeout from expiring.
