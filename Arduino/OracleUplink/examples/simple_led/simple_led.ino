#include <OracleUplink.h>
#include <Adafruit_NeoPixel.h>

#define PIN        33 // Change this to your board's NeoPixel pin! (Feather S3 uses 33 usually)
#define NUMPIXELS  1

Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

// Initialize with BLE transport (or change to "USB")
OracleUplink uplink("BLE");

void setup() {
  Serial.begin(115200);
  pixels.begin();
  pixels.clear();
  pixels.show();

  // Configure your identity
  uplink.setDeviceName("Bat-Tech C++ Core");
  uplink.setBleBroadcastName("BAT-CPP");
  
  // Register Capabilities
  uplink.addCapability("neopixel");

  // Register command to change NeoPixel color (e.g. COLOR:#FF00FF)
  uplink.onCommand("COLOR", [](String val) {
    if (val.startsWith("#")) {
      long number = strtol(&val[1], NULL, 16);
      int r = number >> 16;
      int g = number >> 8 & 0xFF;
      int b = number & 0xFF;
      pixels.setPixelColor(0, pixels.Color(r, g, b));
      pixels.show();
      
      // Send confirmation back to the Dashboard
      JsonDocument doc;
      doc["type"] = "color_ack";
      doc["status"] = "success";
      uplink.sendJson(doc);
    }
  });

  // Start the engine
  uplink.begin();
}

void loop() {
  // Handles BLE handshakes, incoming commands, and watchdog
  uplink.update();
  delay(1);
}
