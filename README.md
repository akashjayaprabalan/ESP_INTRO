# Arduino Nano ESP32 iPhone LED Web App

This project lets an iPhone 13 control Arduino Nano ESP32 outputs through WiFi. The Arduino Nano ESP32 creates its own WiFi network and serves a simple web page, so you do not need to install an iPhone app.

The single-LED Arduino sketch is in:

```text
NanoEsp32LedWeb/NanoEsp32LedWeb.ino
```

The four-LED Arduino sketch is in:

```text
NanoEsp32FourLedWeb/NanoEsp32FourLedWeb.ino
```

The older ESP-IDF files are still in this repository, but the beginner workflow below uses Arduino IDE.

## Four LED Controller Sketch

Use `NanoEsp32FourLedWeb/NanoEsp32FourLedWeb.ino` for the newer iPhone web app with four steady LED controls.

### What It Controls

After upload, the Nano ESP32 hosts:

| Setting | Value |
| --- | --- |
| WiFi network | `NanoESP32_FOUR_LED` |
| WiFi password | `fourled123` |
| Web page | `http://192.168.4.1` |
| Red LED | `D2` |
| Blue LED | `D3` |
| Yellow LED | `D4` |
| Green LED | `D5` |

The iPhone page has ON/OFF buttons for each LED color and an `ALL OFF` button. A color turns on when you tap `ON` and stays on until you tap `OFF` or `ALL OFF`.

### Wire The Four LEDs

Unplug the Nano ESP32 from USB before wiring.

For this sketch, use 4 small LEDs and 4 resistors, ideally 220-330 ohm.

For each LED:

1. Connect the Arduino pin to one leg of a 220-330 ohm resistor.
2. Connect the other resistor leg to the LED anode.
3. Connect the LED cathode to `GND`.

Use this table:

| LED color | Arduino pin |
| --- | --- |
| Red | `D2` |
| Blue | `D3` |
| Yellow | `D4` |
| Green | `D5` |

LED polarity matters. The anode is usually the longer leg. The cathode is usually the shorter leg and often lines up with the flat side of the LED body.

If the green LED on `D5` does not turn on, reupload the latest sketch first. The sketch uses the Nano ESP32 board labels `D2`, `D3`, `D4`, and `D5` directly instead of raw numbers, which avoids Arduino IDE pin-numbering confusion. If it still does not work, unplug USB and test the same LED, resistor, and jumper wire on a known-working LED pin such as `D2`.

### Upload The Four-LED Sketch

1. In Arduino IDE, open `NanoEsp32FourLedWeb/NanoEsp32FourLedWeb.ino`.
2. Select `Tools > Board > Arduino ESP32 Boards > Arduino Nano ESP32`.
3. Select `Tools > Pin Numbering > By Arduino pin (default)`.
4. Select the Nano ESP32 port in `Tools > Port`.
5. Click the checkmark button to verify/compile.
6. Click the arrow button to upload.
7. Open `Tools > Serial Monitor`.
8. Set the baud rate to `115200`.
9. Wait for messages like:

```text
Nano ESP32 LED web app is ready.
WiFi network: NanoESP32_FOUR_LED
WiFi password: fourled123
Open this address on your iPhone: http://192.168.4.1
```

### Use The Four-LED Web App From Your iPhone

1. On the iPhone, open `Settings > Wi-Fi`.
2. Join `NanoESP32_FOUR_LED`.
3. Enter the password `fourled123`.
4. iOS may say the network has no internet. Stay connected to it.
5. Open Safari.
6. Go to `http://192.168.4.1`.
7. Tap `ON` for any LED color and confirm that color turns on.
8. Turn on multiple colors and confirm they stay on together.
9. Tap `OFF` for a color and confirm it turns off.
10. Tap `ALL OFF` and confirm every LED turns off.

## What You Need

- Arduino Nano ESP32.
- USB-C cable for programming and power.
- Breadboard.
- 1 small LED.
- 1 resistor, ideally 220 ohm to 330 ohm.
- A few jumper wires.
- iPhone 13.
- Arduino IDE 2.x.

## How It Works

After upload, the Nano ESP32 hosts:

| Setting | Value |
| --- | --- |
| WiFi network | `NanoESP32_LED` |
| WiFi password | `ledcontrol` |
| Web page | `http://192.168.4.1` |
| LED control pin | `D2` |

Open the web page from Safari while your iPhone is connected to `NanoESP32_LED`. Tap `ON` to turn the LED on and `OFF` to turn it off.

## Wire The LED

Unplug the Nano ESP32 from USB before wiring.

1. Put the LED on the breadboard.
2. Connect Nano ESP32 `D2` to one leg of the resistor.
3. Connect the other leg of the resistor to the LED anode.
4. Connect the LED cathode to Nano ESP32 `GND`.
5. Plug the Nano ESP32 back into USB.

LED polarity matters:

- The anode is usually the longer LED leg.
- The cathode is usually the shorter LED leg and often has the flat side of the LED body.
- If the web page works but the LED never lights, unplug USB and reverse the LED.

Important safety notes:

- Do not connect the LED directly to `D2`; always use the resistor.
- Nano ESP32 GPIO pins are 3.3 V pins. Do not connect 5 V or higher to `D2`.
- This guide is for a small LED only, not an AC mains bulb or high-current lamp.

## Install Arduino IDE Support

1. Install Arduino IDE from the Arduino website.
2. Open Arduino IDE.
3. Connect the Nano ESP32 with a USB-C cable.
4. Go to `Tools > Board > Boards Manager`.
5. Search for `Arduino ESP32 Boards`.
6. Install the Arduino ESP32 board package.
7. Go to `Tools > Board > Arduino ESP32 Boards > Arduino Nano ESP32`.
8. Go to `Tools > Pin Numbering` and choose `By Arduino pin (default)`.
9. Go to `Tools > Port` and select the Nano ESP32 port.

The sketch uses `D2`, which is the safest style for Nano ESP32 pin labels because it works with the board's Arduino pin mapping.

## Upload The Sketch

1. In Arduino IDE, open `NanoEsp32LedWeb/NanoEsp32LedWeb.ino`.
2. Click the checkmark button to verify/compile.
3. Click the arrow button to upload.
4. Open `Tools > Serial Monitor`.
5. Set the baud rate to `115200`.
6. Wait for messages like:

```text
Nano ESP32 LED web app is ready.
WiFi network: NanoESP32_LED
WiFi password: ledcontrol
Open this address on your iPhone: http://192.168.4.1
```

## Use It From Your iPhone

1. On the iPhone, open `Settings > Wi-Fi`.
2. Join `NanoESP32_LED`.
3. Enter the password `ledcontrol`.
4. iOS may say the network has no internet. Stay connected to it.
5. Open Safari.
6. Go to `http://192.168.4.1`.
7. Tap `ON` and confirm the LED lights.
8. Tap `OFF` and confirm the LED turns off.

Optional: in Safari, tap Share, then `Add to Home Screen`. That gives you an app-like icon for the control page.

## Troubleshooting

### Arduino IDE cannot find the board

- Try another USB-C cable. Some cables are power-only.
- Try another USB port.
- Make sure `Arduino Nano ESP32` is selected in `Tools > Board`.
- Check `Tools > Port` again after unplugging and reconnecting the board.

### Upload fails

- Press the reset button on the Nano ESP32 and try Upload again.
- Close Serial Monitor before uploading.
- If the board still will not upload, double-tap reset to put it into bootloader mode, then upload again.

### The iPhone connects but Safari cannot open the page

- Confirm the iPhone is still connected to `NanoESP32_LED`.
- Type the address exactly as `http://192.168.4.1`.
- Turn off iPhone cellular data temporarily if Safari keeps trying to use mobile internet.
- Press reset on the Nano ESP32 and wait 10 seconds.

### The page opens but the LED does not turn on

- Confirm the resistor is connected between `D2` and the LED anode.
- Confirm the LED cathode goes to `GND`.
- Reverse the LED if it may be backwards.
- Make sure you used pin `D2`, not a nearby pin.

## Project Structure

```text
.
|-- NanoEsp32FourLedWeb
|   `-- NanoEsp32FourLedWeb.ino
|-- NanoEsp32LedWeb
|   `-- NanoEsp32LedWeb.ino
|-- README.md
|-- CMakeLists.txt
`-- main
    |-- CMakeLists.txt
    `-- main.c
```

## References

- Arduino Nano ESP32 getting started: https://docs.arduino.cc/tutorials/nano-esp32/getting-started-nano-esp32
- Arduino Nano ESP32 pin numbering: https://support.arduino.cc/hc/en-us/articles/10483225565980-Select-pin-numbering-for-Nano-ESP32-in-Arduino-IDE
- Arduino Nano ESP32 datasheet: https://docs.arduino.cc/resources/datasheets/ABX00083-datasheet.pdf
