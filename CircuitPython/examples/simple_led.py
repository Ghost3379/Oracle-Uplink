import time
import board
import neopixel
from oracle_uplink import OracleUplink

# 1. Setup Hardware (Using the onboard NeoPixel)
pixel = neopixel.NeoPixel(board.NEOPIXEL, 1, brightness=0.2)

# 2. Initialize the Uplink Bridge
# Change transport to "USB" or "WIFI" depending on how you want to connect
uplink = OracleUplink(transport="BLE")

# 3. Configure your Gadget Identity
uplink.set_ble_broadcast_name("BAT-GADGET")
uplink.set_device_name("Standard Batarang V1")

# Tell the website to render a NeoPixel color picker
uplink.add_capability("neopixel")

# 4. Handle incoming commands from the Website
def handle_color(hex_color):
    try:
        hex_color = hex_color.lstrip('#')
        r = int(hex_color[0:2], 16)
        g = int(hex_color[2:4], 16)
        b = int(hex_color[4:6], 16)
        pixel.fill((r, g, b))
        print(f"Color changed to R:{r} G:{g} B:{b}")
    except Exception as e:
        print(f"Error parsing color: {e}")

uplink.on_command("COLOR", handle_color)

# 5. Main Engine
uplink.start_advertising()
print("Awaiting Oracle Network connection...")

while True:
    # This single line handles the connection, handshake, and incoming data routing
    uplink.update() 
    time.sleep(0.05)
