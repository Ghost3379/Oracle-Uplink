import sys
import supervisor
import json
from .base import UplinkBase

class USBUplink(UplinkBase):
    """USB Cable transport layer using standard Serial and Web Serial API."""
    def __init__(self):
        super().__init__()
        self.is_connected = True # USB is effectively always connected
        self.rx_buffer = ""
        # We don't print an initialization string here anymore so we don't confuse the website's JSON parser

    def send_json(self, data_dict):
        try:
            # We use standard print to send via USB Serial (sys.stdout)
            # The Base44 Web Serial API will capture this automatically!
            msg = json.dumps(data_dict)
            print(msg)
        except Exception as e:
            pass

    def update(self):
        # Read from standard input (sys.stdin) if bytes are waiting
        if supervisor.runtime.serial_bytes_available:
            # Read exact number of available bytes to prevent blocking
            chars = sys.stdin.read(supervisor.runtime.serial_bytes_available)
            self.rx_buffer += chars
            
            # Process complete lines
            while '\n' in self.rx_buffer:
                line, self.rx_buffer = self.rx_buffer.split('\n', 1)
                line = line.strip()
                if line:
                    self._process_incoming_text(line)
