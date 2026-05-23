# ESP32 WiFi Haptic and LED Controller

This project controls 4 coin vibration motors and 4 LEDs from a phone browser. The ESP32 creates its own WiFi network, serves a mobile-friendly web page, and turns on only the selected output. The ALL OFF button disables every output instantly.

The implementation uses ESP-IDF and literal C. This matches the final project decision that pure C takes priority over the Arduino framework, because Arduino sketches are compiled as C++.

## Pin Mapping

| Output | ESP32 GPIO | Hardware connection |
| --- | ---: | --- |
| Vibrator 1 | GPIO 25 | Motor driver input 1 |
| Vibrator 2 | GPIO 26 | Motor driver input 2 |
| Vibrator 3 | GPIO 27 | Motor driver input 3 |
| Vibrator 4 | GPIO 33 | Motor driver input 4 |
| LED 1 | GPIO 16 | LED 1 anode through resistor |
| LED 2 | GPIO 17 | LED 2 anode through resistor |
| LED 3 | GPIO 18 | LED 3 anode through resistor |
| LED 4 | GPIO 19 | LED 4 anode through resistor |

## Wiring

### Coin vibration motors

Each coin vibration motor must be switched by a transistor or logic-level MOSFET driver circuit. Do not connect a motor directly to an ESP32 GPIO pin.

Typical low-side MOSFET wiring:

1. ESP32 GPIO goes to the MOSFET gate through a small resistor, such as 100 ohms to 220 ohms.
2. Add a gate pulldown resistor, such as 10 kOhm, from gate to ground so the motor stays off during boot.
3. MOSFET source connects to ground.
4. MOSFET drain connects to the motor negative wire.
5. Motor positive wire connects to the external motor supply positive terminal.
6. External motor supply ground connects to ESP32 ground.

Typical NPN transistor wiring:

1. ESP32 GPIO goes to the transistor base through a resistor, such as 1 kOhm.
2. Transistor emitter connects to ground.
3. Transistor collector connects to the motor negative wire.
4. Motor positive wire connects to the external motor supply positive terminal.
5. External motor supply ground connects to ESP32 ground.

### LEDs

Wire each LED with a current-limiting resistor:

1. ESP32 GPIO connects to one side of a resistor, such as 220 ohms to 1 kOhm.
2. The resistor connects to the LED anode.
3. The LED cathode connects to ground.

## Circuit Safety Notes

- Never power coin motors directly from ESP32 GPIO pins.
- Use transistor or MOSFET drivers rated for the motor current.
- Use an external motor power supply if the motors need more current than USB can safely provide.
- Connect ESP32 ground and motor supply ground together.
- Add flyback diodes if your motor type or driver board requires them.
- Keep motor wiring short where possible to reduce electrical noise.
- Avoid ESP32 flash pins GPIO 6 through GPIO 11.
- Avoid input-only pins GPIO 34 through GPIO 39 for outputs.

## Web UI

After startup, the ESP32 hosts:

- WiFi SSID: `ESP32_HAPTIC_LED`
- WiFi password: `esp32control`
- Web page: `http://192.168.4.1`

The web page has:

- Vibrator 1
- Vibrator 2
- Vibrator 3
- Vibrator 4
- LED 1
- LED 2
- LED 3
- LED 4
- ALL OFF

Pressing any motor or LED button turns all outputs off first, then activates only the selected output. Pressing ALL OFF disables all motors and LEDs.

## Uploading To ESP32

Install ESP-IDF first, then open a terminal in this project folder.

```sh
idf.py set-target esp32
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

On macOS, the serial port often looks like:

```sh
idf.py -p /dev/cu.usbserial-0001 flash monitor
```

If you are unsure which port to use, connect the ESP32 and run:

```sh
ls /dev/cu.*
```

## Phone Browser Workflow

1. Upload the firmware to the ESP32.
2. Open the serial monitor and wait for the startup messages.
3. On your phone, open WiFi settings.
4. Join `ESP32_HAPTIC_LED`.
5. Enter password `esp32control`.
6. Open a browser and visit `http://192.168.4.1`.
7. Tap a vibrator, LED, or ALL OFF button.

## Debug Messages

The serial monitor prints:

- WiFi access point startup.
- SSID, password, and browser URL.
- Button press events.
- Active motor or LED.
- ALL OFF events.

## Project Structure

```text
.
├── CMakeLists.txt
├── README.md
└── main
    ├── CMakeLists.txt
    └── main.c
```

