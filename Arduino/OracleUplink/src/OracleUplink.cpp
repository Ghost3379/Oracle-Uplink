#include "OracleUplink.h"

// NimBLE Includes for memory efficient Bluetooth on ESP32
#include <NimBLEDevice.h>
#include <NimBLEServer.h>
#include <NimBLEUtils.h>

#define SERVICE_UUID           "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define CHARACTERISTIC_UUID_RX "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
#define CHARACTERISTIC_UUID_TX "6e400003-b5a3-f393-e0a9-e50e24dcca9e"

// NimBLE Globals
NimBLEServer *pServer = NULL;
NimBLECharacteristic *pTxCharacteristic = NULL;
volatile bool deviceConnected = false;
volatile bool oldDeviceConnected = false;
String rxBuffer = "";

class MyServerCallbacks: public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer) {
      Serial.println("[BLE] Client Connected!");
      deviceConnected = true;
    };
    void onDisconnect(NimBLEServer* pServer) {
      Serial.println("[BLE] Client Disconnected!");
      deviceConnected = false;
    }
};

class MyCallbacks: public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic *pCharacteristic) {
      std::string rxValue = pCharacteristic->getValue();
      if (rxValue.length() > 0) {
        for (int i = 0; i < rxValue.length(); i++) {
          rxBuffer += (char)rxValue[i]; // Buffer incoming chunks
        }
      }
    }
};

OracleUplink::OracleUplink(String transport) {
    _transport = transport;
    _transport.toUpperCase();
    _deviceName = "Unknown Gadget";
    _bleBroadcastName = "ORACLE-UPLINK";
    _isConnected = false;
    _lastMsgTime = millis();
}

OracleUplink::~OracleUplink() {}

void OracleUplink::setDeviceName(String name) { _deviceName = name; }
void OracleUplink::setBleBroadcastName(String name) { _bleBroadcastName = name; }

void OracleUplink::addCapability(String capability) {
    _capabilities.push_back(capability);
}

void OracleUplink::onCommand(String command, std::function<void(String)> callback) {
    command.toUpperCase();
    _callbacksWithValue[command] = callback;
}

void OracleUplink::onCommand(String command, std::function<void()> callback) {
    command.toUpperCase();
    _callbacksNoValue[command] = callback;
}

void OracleUplink::_sendHandshake() {
    JsonDocument doc;
    doc["type"] = "handshake";
    doc["device_name"] = _deviceName;
    JsonArray caps = doc["capabilities"].to<JsonArray>();
    for(String cap : _capabilities) {
        caps.add(cap);
    }
    sendJson(doc);
}

void OracleUplink::sendJson(JsonDocument& doc) {
    String output;
    serializeJson(doc, output);
    output += "\n";

    if (_transport == "BLE") {
        _sendJsonBLE(output);
    } else if (_transport == "USB") {
        Serial.print(output); // Send over USB via standard print
    }
}

void OracleUplink::_processIncomingText(String text) {
    text.trim();
    if(text.length() == 0) return;

    Serial.println("[UPLINK RX] " + text);
    Serial.flush();

    String textUpper = text;
    textUpper.toUpperCase();

    if (textUpper == "INIT") {
        Serial.println("[UPLINK] Processing INIT request...");
        Serial.flush();
        _sendHandshake();
    } else if (textUpper == "PING") {
        JsonDocument doc;
        doc["type"] = "pong";
        sendJson(doc);
    } else {
        // Parse "COMMAND:VALUE" format
        int colonIdx = text.indexOf(':');
        if (colonIdx != -1) {
            String cmd = text.substring(0, colonIdx);
            cmd.toUpperCase();
            String val = text.substring(colonIdx + 1);
            if (_callbacksWithValue.count(cmd) > 0) {
                _callbacksWithValue[cmd](val);
                return;
            }
        }
        
        // Exact command match
        if (_callbacksNoValue.count(textUpper) > 0) {
            _callbacksNoValue[textUpper]();
        }
    }
}

void OracleUplink::begin() {
    if (_transport == "BLE") {
        _beginBLE();
    } else if (_transport == "USB") {
        Serial.begin(115200);
        Serial.println(">>> USB UPLINK ACTIVE <<<");
    }
}

void OracleUplink::update() {
    if (_transport == "BLE") {
        _updateBLE();
    } else if (_transport == "USB") {
        if(Serial.available()) {
            String line = Serial.readStringUntil('\n');
            _processIncomingText(line);
        }
    }
}

void OracleUplink::_beginBLE() {
    NimBLEDevice::init(_bleBroadcastName.c_str());
    
    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    NimBLEService *pService = pServer->createService(SERVICE_UUID);

    pTxCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_TX,
        NIMBLE_PROPERTY::NOTIFY
    );

    NimBLECharacteristic *pRxCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_RX,
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR
    );
    pRxCharacteristic->setCallbacks(new MyCallbacks());

    pService->start();
    NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->setName(_bleBroadcastName.c_str());
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->start();
    
    Serial.println("Advertising as " + _bleBroadcastName);
}

void OracleUplink::_updateBLE() {
    // Handle Connection State
    if (deviceConnected && !oldDeviceConnected) {
        oldDeviceConnected = deviceConnected;
        _isConnected = true;
        _lastMsgTime = millis();
        Serial.println(">>> BLE SECURE LINK ESTABLISHED <<<");
        Serial.flush();
    }
    
    if (!deviceConnected && oldDeviceConnected) {
        _isConnected = false;
        delay(500); 
        pServer->startAdvertising();
        Serial.println(">>> BLE LINK SEVERED. Re-advertising... <<<");
        oldDeviceConnected = deviceConnected;
    }

    // Process chunked data in rxBuffer by splitting on newline
    if (rxBuffer.length() > 0) {
        int newlineIdx = rxBuffer.indexOf('\n');
        while (newlineIdx != -1) {
            _lastMsgTime = millis(); // Reset watchdog
            String completeMessage = rxBuffer.substring(0, newlineIdx);
            rxBuffer = rxBuffer.substring(newlineIdx + 1);
            _processIncomingText(completeMessage);
            newlineIdx = rxBuffer.indexOf('\n');
        }
    }

    // Watchdog Timer for Zombie connections
    if (_isConnected) {
        if (millis() - _lastMsgTime > _timeoutMs) {
            Serial.println(">>> WATCHDOG TIMEOUT. Forcing disconnect... <<<");
            // Disconnect peer 0 (Web Bluetooth)
            pServer->disconnect(pServer->getPeerInfo(0).getConnHandle());
        }
    }
}

void OracleUplink::_sendJsonBLE(String jsonStr) {
    if (deviceConnected) {
        Serial.println("[UPLINK TX] " + jsonStr);
        int length = jsonStr.length();
        int offset = 0;
        int maxChunkSize = 20; // Safe BLE MTU limit
        while (offset < length) {
            int chunkSize = length - offset;
            if (chunkSize > maxChunkSize) {
                chunkSize = maxChunkSize;
            }
            pTxCharacteristic->setValue((uint8_t*)(jsonStr.c_str() + offset), chunkSize);
            pTxCharacteristic->notify();
            offset += chunkSize;
            delay(10); // Prevent internal BLE queue overflow
        }
    }
}
