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
int connectionState = 0; // 0=Disconnected, 1=Waiting, 2=Ready
unsigned long connectionTimer = 0;

// Safe ISR Buffer (Zero Dynamic Memory)
#define RX_BUF_SIZE 256
volatile char isrRxBuffer[RX_BUF_SIZE];
volatile int isrRxLen = 0;
volatile bool isrNewData = false;

class MyServerCallbacks: public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer) {
      deviceConnected = true;
    };
    void onConnect(NimBLEServer* pServer, ble_gap_conn_desc* desc) {
      deviceConnected = true;
      pServer->updateConnParams(desc->conn_handle, 24, 48, 0, 60); // Stabilize Windows WebBLE
    }
    void onDisconnect(NimBLEServer* pServer) {
      deviceConnected = false;
    }
};

class MyCallbacks: public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic *pCharacteristic) {
      std::string rxValue = pCharacteristic->getValue();
      int len = rxValue.length();
      if (len > 0 && (isrRxLen + len) < RX_BUF_SIZE - 1) {
          memcpy((void*)(isrRxBuffer + isrRxLen), rxValue.c_str(), len);
          isrRxLen += len;
          isrRxBuffer[isrRxLen] = '\0';
          isrNewData = true;
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
    Serial.println("[MAIN] Initializing NimBLE Stack (V3 Safe)...");
    NimBLEDevice::init(_bleBroadcastName.c_str());
    NimBLEDevice::setMTU(512); // Vital for Web Bluetooth compatibility
    
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
        connectionState = 1; // Waiting state
        connectionTimer = millis();
        Serial.println("[MAIN] >>> BLE SECURE LINK ESTABLISHED <<<");
        Serial.println("[MAIN] Stopping Advertising (Python Parity)");
        pServer->getAdvertising()->stop();
        Serial.flush();
    }
    
    if (!deviceConnected && oldDeviceConnected) {
        _isConnected = false;
        connectionState = 0;
        delay(500); 
        pServer->startAdvertising();
        Serial.println("[MAIN] >>> BLE LINK SEVERED. Re-advertising... <<<");
        oldDeviceConnected = deviceConnected;
    }

    // State Machine: 2-second proactive handshake
    if (connectionState == 1) {
        if (millis() - connectionTimer > 2000) {
            connectionState = 2; // Ready state
            Serial.println("[MAIN] 2-second wait complete. Sending Proactive Handshake...");
            _sendHandshake();
        }
    }

    // Process chunked data safely
    if (isrNewData) {
        String msg = String((char*)isrRxBuffer);
        isrRxLen = 0; // Clear buffer
        isrRxBuffer[0] = '\0';
        isrNewData = false;
        
        Serial.println("[MAIN] Extracted command from ISR: " + msg);
        
        int newlineIdx = msg.indexOf('\n');
        while (newlineIdx != -1) {
            _lastMsgTime = millis(); // Reset watchdog
            String completeMessage = msg.substring(0, newlineIdx);
            msg = msg.substring(newlineIdx + 1);
            
            _processIncomingText(completeMessage);
            newlineIdx = msg.indexOf('\n');
        }
        
        // Put any leftover fragment back into the buffer
        if (msg.length() > 0) {
            memcpy((void*)isrRxBuffer, msg.c_str(), msg.length());
            isrRxLen = msg.length();
            isrRxBuffer[isrRxLen] = '\0';
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
