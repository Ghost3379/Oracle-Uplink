# 🦇 Oracle Uplink

> **The official hardware bridge library for the Oracle Network.**
> 
> *Seamlessly integrate your DIY microcontrollers and tactical gadgets into the Oracle Network ecosystem via BLE, USB, or WiFi.*

---

## 📡 What is this?
**Oracle Uplink** is a modular bridge library designed for both **CircuitPython** and **Arduino (C++)**. It acts as a universal "Post Office" between your custom hardware and the Oracle Network web dashboard. 

Instead of writing complex Bluetooth or WebSocket code for every new gadget you build, you simply include this library on your board, declare what features your gadget has (e.g. `neopixel`, `gyroscope`), and the library automatically handles the secure handshake and JSON telemetry streaming.

### 🌐 Multi-Transport Support
The library supports two highly secure connection methods without changing any of your data payloads:
- **BLE (Bluetooth Low Energy):** Connect wirelessly via Web Bluetooth.
- **USB (Web Serial):** Connect instantly using a standard USB-C data cable.

---

## ⚙️ Installation

Because this library supports both languages, please navigate to the specific folder for your setup:
- [CircuitPython Installation Guide](./CircuitPython/README.md)
- [Arduino (C++) Installation Guide](./Arduino/README.md)

---

## 🚀 Quick Start

Here is a bare-minimum example to connect your board to the Oracle Network and control a NeoPixel via Bluetooth.

```python
import time
import board
import neopixel
from oracle_uplink import OracleUplink

# 1. Setup Hardware
pixel = neopixel.NeoPixel(board.NEOPIXEL, 1, brightness=0.2)

# 2. Initialize the Uplink Bridge
# You can change transport to "USB" or "WIFI" instantly.
uplink = OracleUplink(transport="BLE")

# 3. Configure your Gadget Identity
uplink.set_ble_broadcast_name("BAT-GADGET")
uplink.set_device_name("Standard Batarang V1")

# Tell the website to render a NeoPixel color picker
uplink.add_capability("neopixel")

# 4. Handle incoming commands from the Website
def handle_color(hex_color):
    hex_color = hex_color.lstrip('#')
    r, g, b = int(hex_color[0:2], 16), int(hex_color[2:4], 16), int(hex_color[4:6], 16)
    pixel.fill((r, g, b))

uplink.on_command("COLOR", handle_color)

# 5. Main Engine
uplink.start_advertising()
print("Awaiting Oracle Network connection...")

while True:
    uplink.update() # This single line handles the connection and incoming data
    time.sleep(0.05)
```

---

## 🛠️ Dynamic Capabilities Architecture
When your device connects, the library automatically sends a JSON "Handshake" to the website. 
By using `uplink.add_capability("battery")`, the website reads this handshake and dynamically spawns a Battery widget on your screen. You do not need to hardcode the website dashboard!

---

## 🏗️ Roadmap
- [x] CircuitPython BLE Transport
- [x] CircuitPython USB (Web Serial) Transport
- [x] Arduino (C++) Library Port
- [ ] Test Wifi & USB Connection
- [ ] Test Arduino Library

---
*Wayne Tech Enterprises / Applied Science Division*
