import time
import board
import neopixel
from oracle_uplink import OracleUplink

# Init Hardware
pixel_pin = board.NEOPIXEL
pixel = neopixel.NeoPixel(pixel_pin, 1, brightness=1, auto_write=False)

# Init Uplink
uplink = OracleUplink(transport="BLE")
uplink.set_ble_broadcast_name("Oracle Node")
uplink.set_device_name("Non-Blocking Example")
uplink.add_capability("neopixel")

# State Variables
current_color = (0, 0, 255)
current_mode = "SOLID"

def handle_color(hex_color):
    global current_color, current_mode
    hex_color = hex_color.lstrip('#')
    r, g, b = int(hex_color[0:2], 16), int(hex_color[2:4], 16), int(hex_color[4:6], 16)
    current_color = (r, g, b)
    current_mode = "SOLID"
    pixel.brightness = 1.0
    pixel.fill(current_color)
    pixel.show()

def handle_pulse():
    global current_mode
    current_mode = "PULSE"

def handle_blink():
    global current_mode
    current_mode = "BLINK"

# Register Commands
uplink.on_command("COLOR", handle_color)
uplink.on_command("PULSE", handle_pulse)
uplink.on_command("BLINK", handle_blink)

uplink.start_advertising()

# Timer Tracking
last_anim_update = 0
blink_state = False

while True:
    uplink.update()
    
    now = time.monotonic()
    
    if current_mode == "BLINK":
        # Toggle every 0.5 seconds
        if now - last_anim_update >= 0.5:
            last_anim_update = now
            blink_state = not blink_state
            
            if blink_state:
                pixel.fill(current_color)
            else:
                pixel.fill((0, 0, 0))
            
            pixel.brightness = 1.0
            pixel.show()
            
    elif current_mode == "PULSE":
        # Render frame every ~20ms (50 FPS)
        if now - last_anim_update >= 0.02:
            last_anim_update = now
            
            t = now * 1.5  # Adjust multiplier for speed
            brightness = abs((t % 2) - 1)
            
            pixel.fill(current_color)
            pixel.brightness = brightness
            pixel.show()
            
    # Very small sleep to yield CPU
    time.sleep(0.01)
