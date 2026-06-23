#include <OracleUplink.h>
#include <Adafruit_NeoPixel.h>

#define PIN        2
#define NUMPIXELS  1

Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);
OracleUplink uplink;

String currentMode = "SOLID";
uint8_t currentR = 0;
uint8_t currentG = 0;
uint8_t currentB = 255;

unsigned long lastAnimUpdate = 0;
bool blinkState = false;

void handleColor(String hexColor) {
    hexColor.replace("#", "");
    long number = strtol(&hexColor[0], NULL, 16);
    currentR = number >> 16;
    currentG = number >> 8 & 0xFF;
    currentB = number & 0xFF;
    
    currentMode = "SOLID";
    pixels.setBrightness(255);
    pixels.setPixelColor(0, pixels.Color(currentR, currentG, currentB));
    pixels.show();
}

void handlePulse() {
    currentMode = "PULSE";
}

void handleBlink() {
    currentMode = "BLINK";
}

void setup() {
    Serial.begin(115200);
    pixels.begin();
    
    uplink.setDeviceName("Non-Blocking Node");
    uplink.setBleBroadcastName("Oracle Node");
    uplink.addCapability("neopixel");
    
    uplink.onCommand("COLOR", handleColor);
    uplink.onCommand("PULSE", handlePulse);
    uplink.onCommand("BLINK", handleBlink);
    
    uplink.begin();
}

void loop() {
    uplink.update();
    
    unsigned long now = millis();
    
    if (currentMode == "BLINK") {
        if (now - lastAnimUpdate >= 500) {
            lastAnimUpdate = now;
            blinkState = !blinkState;
            pixels.setBrightness(255);
            if (blinkState) {
                pixels.setPixelColor(0, pixels.Color(currentR, currentG, currentB));
            } else {
                pixels.setPixelColor(0, pixels.Color(0, 0, 0));
            }
            pixels.show();
        }
    } 
    else if (currentMode == "PULSE") {
        if (now - lastAnimUpdate >= 20) {
            lastAnimUpdate = now;
            
            // Create a triangle wave (1000ms full cycle)
            float t = (now % 1000) / 1000.0;
            float brightness = abs((t * 2.0) - 1.0);
            
            pixels.setBrightness((uint8_t)(brightness * 255));
            pixels.setPixelColor(0, pixels.Color(currentR, currentG, currentB));
            pixels.show();
        }
    }
}
