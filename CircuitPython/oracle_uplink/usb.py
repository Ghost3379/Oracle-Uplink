import sys
import supervisor
import json
from .base import UplinkBase

class USBUplink(UplinkBase):
    """USB Cable transport layer using standard Serial and Web Serial API."""
    def __init__(self):
        super().__init__()
        self.is_connected = True # USB is effectively always connected
        print(">>> USB UPLINK INITIALIZED - Awaiting Base44 Web Serial Connection <<<")

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
            text = sys.stdin.readline().strip()
            
            # Since standard REPL commands might sneak in, we only process valid uplink commands
            if text:
                self._process_incoming_text(text)
