import wifi
import socketpool
import time
import json
from .base import UplinkBase

# Note: Requires adafruit_httpserver to be installed in lib/
try:
    from adafruit_httpserver import Server, Request, Websocket, GET
except ImportError:
    print("WARNING: adafruit_httpserver library is missing. WiFi Uplink will not work.")

class WiFiUplink(UplinkBase):
    """WiFi transport layer hosting a WebSocket server for real-time bi-directional JSON."""
    def __init__(self, ssid=None, password=None):
        super().__init__()
        self.ssid = ssid
        self.password = password
        
        if self.ssid and self.password:
            self._connect_wifi()

        self.pool = socketpool.SocketPool(wifi.radio)
        self.server = Server(self.pool, "/static", debug=True)
        self.websocket = None
        
        # Watchdog timer
        self.last_msg_time = time.monotonic()
        self.timeout_seconds = 30

        self._setup_routes()
        self.server.start(str(wifi.radio.ipv4_address), port=80)
        print(f">>> WIFI UPLINK ACTIVE AT ws://{wifi.radio.ipv4_address}/ws <<<")

    def _connect_wifi(self):
        print(f"Connecting to {self.ssid}...")
        try:
            wifi.radio.connect(self.ssid, self.password)
            print("Connected to Network!")
        except Exception as e:
            print(f"Failed to connect to WiFi: {e}")

    def _setup_routes(self):
        @self.server.route("/ws", GET)
        def connect_websocket(request: Request):
            if self.websocket is not None:
                self.websocket.close()
                
            self.websocket = Websocket(request)
            self.is_connected = True
            print(">>> WEBSOCKET SECURE LINK ESTABLISHED <<<")
            self.last_msg_time = time.monotonic()
            
            # Delay slightly for browser to catch up, then send handshake
            time.sleep(1.0)
            self.send_json(self._get_handshake_dict())
            return self.websocket

    def send_json(self, data_dict):
        if self.is_connected and self.websocket is not None:
            try:
                msg = json.dumps(data_dict) + "\n"
                self.websocket.send_message(msg)
            except Exception as e:
                print(f"Error sending JSON via WiFi: {e}")
                self.is_connected = False
                self.websocket = None

    def update(self):
        # Allow the server to process incoming network requests
        try:
            self.server.poll()
        except Exception as e:
            pass
            
        # Check for Zombie Connection
        if self.is_connected and self.websocket is not None:
            if time.monotonic() - self.last_msg_time > self.timeout_seconds:
                print(">>> WATCHDOG TIMEOUT. Forcing disconnect... <<<")
                self.websocket.close()
                self.websocket = None
                self.is_connected = False
                return
                
            # Check for incoming WebSocket messages
            if self.websocket.in_waiting:
                self.last_msg_time = time.monotonic()
                try:
                    text = self.websocket.receive()
                    if text:
                        self._process_incoming_text(text.strip())
                except Exception as e:
                    self.is_connected = False
                    self.websocket = None
