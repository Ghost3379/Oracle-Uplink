def OracleUplink(transport="BLE", **kwargs):
    """
    Factory function to instantiate the correct transport layer dynamically.
    This prevents heavy libraries (like adafruit_ble) from loading into 
    memory unless they are explicitly requested by the user.
    """
    t = transport.upper()
    
    if t == "BLE":
        from .ble import BLEUplink
        return BLEUplink(**kwargs)
        
    elif t == "USB":
        from .usb import USBUplink
        return USBUplink(**kwargs)
        
    else:
        raise ValueError("Invalid transport. Use 'BLE' or 'USB'.")
