import time
import board
import busio
import sdcardio
import storage
import os
from oracle_uplink import OracleUplink

# Initialize the Uplink Bridge
uplink = OracleUplink(transport="BLE")

uplink.set_ble_broadcast_name("BAT-STORAGE")
uplink.set_device_name("External SD Server")
uplink.add_capability("storage")

# Mount SD Card
sd_mounted = False
try:
    spi = busio.SPI(board.SCK, MOSI=board.MOSI, MISO=board.MISO)
    cs = board.IO38 # Adjust Chip Select pin for your board
    sd = sdcardio.SDCard(spi, cs)
    vfs = storage.VfsFat(sd)
    storage.mount(vfs, "/sd")
    sd_mounted = True
    uplink.add_capability("sd_card")
    print("SD Card mounted successfully.")
except Exception as e:
    print(f"SD Card mount failed: {e}")

def handle_status():
    sd_total_mb = 0
    sd_free_mb = 0
    if sd_mounted:
        try:
            sd_stat = os.statvfs('/sd')
            sd_total_mb = (sd_stat[0] * sd_stat[2]) / 1048576
            sd_free_mb = (sd_stat[0] * sd_stat[3]) / 1048576
        except Exception:
            pass

    uplink.send_json({
        "type": "status",
        "sd_used_mb": round(sd_total_mb - sd_free_mb, 2) if sd_mounted else 0,
        "sd_free_mb": round(sd_free_mb, 2) if sd_mounted else 0,
        "sd_mounted": sd_mounted
    })

uplink.on_command("STATUS", handle_status)

# Start Engine
uplink.start_advertising()
print("Awaiting Oracle Network connection...")

while True:
    uplink.update() 
    time.sleep(0.05)
