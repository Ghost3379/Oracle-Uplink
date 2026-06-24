import json

class UplinkBase:
    """Base class for Oracle Network transports. Handles common JSON and callback logic."""
    def __init__(self):
        self.device_name = "Unknown Gadget"
        self.capabilities = []
        self.callbacks = {}
        self.is_connected = False

    def set_device_name(self, name):
        """Sets the internal gadget name sent during the JSON handshake."""
        self.device_name = name

    def set_ble_broadcast_name(self, name):
        """Sets the Bluetooth scanner name. Only applies if transport is BLE."""
        pass

    def start_advertising(self):
        """Starts BLE broadcasting. Ignored if using USB."""
        pass
        
    def stop_advertising(self):
        """Stops BLE broadcasting. Ignored if using USB."""
        pass

    def add_capability(self, feature_name):
        """Registers a capability flag to tell the dashboard to render a specific widget."""
        if feature_name not in self.capabilities:
            self.capabilities.append(feature_name)

    def remove_capability(self, feature_name):
        if feature_name in self.capabilities:
            self.capabilities.remove(feature_name)

    def on_command(self, cmd_key, callback_func):
        """Registers a function to trigger when a specific command string is received."""
        self.callbacks[cmd_key.upper()] = callback_func

    def _get_handshake_dict(self):
        return {
            "type": "handshake",
            "device_name": self.device_name,
            "capabilities": self.capabilities
        }

    def _process_incoming_text(self, text):
        """Standardized routing for all incoming text from any transport."""
        if text.upper() == "INIT":
            self.send_json(self._get_handshake_dict())
        elif text.upper() == "PING":
            self.send_json({"type": "pong"})
        else:
            if ":" in text:
                parts = text.split(":", 1)
                cmd = parts[0].upper()
                val = parts[1]
                if cmd in self.callbacks:
                    self.callbacks[cmd](val)
                return
            
            cmd = text.upper()
            if cmd in self.callbacks:
                self.callbacks[cmd]()

    # ----------------------------------------------------------------
    # Methods that MUST be implemented by specific Transport Subclasses
    # ----------------------------------------------------------------
    def send_json(self, data_dict):
        raise NotImplementedError("Subclass must implement send_json")

    def update(self):
        raise NotImplementedError("Subclass must implement update")
