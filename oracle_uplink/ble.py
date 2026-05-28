import time
import json
from adafruit_ble import BLERadio
from adafruit_ble.advertising.standard import ProvideServicesAdvertisement
from adafruit_ble.services.nordic import UARTService
from .base import UplinkBase

class BLEUplink(UplinkBase):
    """Bluetooth Low Energy transport layer using WebBLE."""
    def __init__(self):
        super().__init__()
        self.ble = BLERadio()
        self.ble.name = "ORACLE-UPLINK"
        self.uart = UARTService()
        self.advertisement = ProvideServicesAdvertisement(self.uart)
        
        # Watchdog timer
        self.last_msg_time = time.monotonic()
        self.timeout_seconds = 30

    def set_ble_broadcast_name(self, name):
        """Sets the short 8-10 char name shown in the Bluetooth scanner."""
        self.ble.name = name

    def start_advertising(self):
        print(f"Advertising as {self.ble.name}...")
        self.ble.start_advertising(self.advertisement)

    def stop_advertising(self):
        self.ble.stop_advertising()

    def send_json(self, data_dict):
        if self.ble.connected:
            try:
                msg = json.dumps(data_dict) + "\n"
                self.uart.write(msg.encode("utf-8"))
            except Exception as e:
                print(f"Error sending JSON via BLE: {e}")

    def update(self):
        # Handle connection state changes
        if self.ble.connected and not self.is_connected:
            self.is_connected = True
            print(">>> BLE SECURE LINK ESTABLISHED <<<")
            self.last_msg_time = time.monotonic()
            self.stop_advertising()
            
            time.sleep(2.0) 
            self.send_json(self._get_handshake_dict())
            
        elif not self.ble.connected and self.is_connected:
            self.is_connected = False
            print(">>> BLE LINK SEVERED. Re-advertising... <<<")
            self.start_advertising()

        # Check for Zombie Connection
        if self.ble.connected and self.is_connected:
            if time.monotonic() - self.last_msg_time > self.timeout_seconds:
                print(">>> WATCHDOG TIMEOUT. Forcing disconnect... <<<")
                for connection in self.ble.connections:
                    connection.disconnect()
                return

        # Handle incoming data
        if self.ble.connected and self.uart.in_waiting:
            self.last_msg_time = time.monotonic()
            raw_bytes = self.uart.read(self.uart.in_waiting)
            text = raw_bytes.decode("utf-8").strip()
            self._process_incoming_text(text)
