# Web Tools

**English** | [中文](../zh/web-tools.md)

The RT-Claw website provides browser-based firmware flashing and serial
debugging tools — no software installation required.

> Requires Chrome or Edge (Web Serial API support).

## Firmware Flash

### Preparation

1. Connect the dev board to your computer via USB
2. Open [rt-claw.com](https://www.rt-claw.com) or local `website/index.html`
3. Scroll to the **Flash** section

### Steps

**1. Select Platform and Board**

| Platform | Board | Description |
|----------|-------|-------------|
| ESP32-C3 | Generic DevKit (4 MB) | Standard ESP32-C3 dev board |
| ESP32-C3 | Xiaozhi XMini (16 MB) | XMini board with OLED and audio |
| ESP32-S3 | Generic DevKit (16 MB + 8 MB PSRAM) | Standard ESP32-S3 dev board |

Board options update automatically when you change the platform.

**2. Select Version**

| Option | Source | Description |
|--------|--------|-------------|
| latest (main) | GitHub Pages | Most recent CI build from main branch |
| v0.2.0, v0.1.0, ... | GitHub Release | Tagged release (requires Release with firmware assets) |

Tags without a GitHub Release show as disabled with "(no firmware)".

**3. Select Baud Rate**

Default is 921600 (fastest). Lower to 460800 or 115200 if flashing is
unstable.

**4. Click "Install RT-Claw"**

The browser opens a serial port picker. Select the matching COM port or
`/dev/ttyUSB*`.

**5. Wait for Completion**

Flashing output appears directly in the Serial Monitor panel on the right.
During flashing, all serial controls are disabled (except the expand button)
to prevent conflicts. The device reboots automatically after flashing.

### Erase Flash

Click **Erase** to wipe the entire flash (factory reset). You must
re-flash firmware afterwards.

### Troubleshooting

| Problem | Cause | Fix |
|---------|-------|-----|
| No serial port found | Missing driver or bad cable | Install CH340/CP2102 driver; try a data cable |
| Failed to open serial port | Port in use by another program | Close idf.py monitor / Arduino IDE / other tabs |
| Chip mismatch | Wrong platform selected | Verify the platform matches your actual chip |
| Firmware not available | CI hasn't built this target yet | Build locally and flash manually |

## Serial Terminal

The serial terminal is next to the Flash panel (or below it on mobile),
providing real-time communication with the device.

### Connect

1. Select baud rate (default 115200)
2. Click **Connect**
3. Pick the serial port in the browser dialog

The status LED turns green and the button changes to **Disconnect**.

### Send Messages

Type text in the input bar at the bottom, press **Enter** or click
**Send**. Messages are sent with `\r\n` line ending for embedded
terminal compatibility.

Example AI conversation:

```
Hello
```

Device responses appear in the output area in real time.

### ANSI Color Rendering

The terminal supports ANSI escape code coloring:

- Standard 8 colors and bright variants (SGR 30-37 / 90-97)
- 256-color extended (SGR 38;5;N)
- 24-bit RGB (SGR 38;2;R;G;B)
- Bold and dim attributes

System messages are color-coded:
- Green: connected
- Red: error
- Yellow: disconnected
- Gray: reset

### Expand Terminal

Click the **⬜** button in the titlebar to expand the terminal over the
flash panel. Click again to restore original size.

### Control Buttons

| Button | Function |
|--------|----------|
| Connect / Disconnect | Open or close the serial connection |
| Reset | Toggle DTR signal to reset the device |
| Clear | Clear terminal output |
| ⬜ | Expand / collapse the terminal |

### Notes

- Serial port is automatically released on page refresh
- Only one program can use a serial port at a time
- Chinese and emoji display correctly (streaming UTF-8 decoding)
- `\r` (carriage return) supports in-line overwrite animation (e.g. thinking...)

## Documentation Center

Click **Docs** in the navigation bar or go to `docs.html` directly.

### Features

- Left sidebar lists all documents
- Click to switch docs without page reload
- In-document links (e.g. `[Coding Style](coding-style.md)`) are
  automatically rewritten to in-app navigation
- EN / 中文 toggle in the top-right reloads the doc in the selected
  language

### URL Format

```
docs.html?doc=getting-started    # Getting Started
docs.html?doc=usage              # Usage Guide
docs.html?doc=architecture       # Architecture
docs.html?doc=porting            # Porting & Extension
docs.html?doc=tuning             # Tuning & Optimization
docs.html?doc=web-tools          # Web Tools
docs.html?doc=coding-style       # Coding Style
docs.html?doc=contributing       # Contributing
```

You can share parameterized URLs directly — recipients see the
corresponding document immediately.
