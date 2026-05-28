#ifndef ORACLE_UPLINK_H
#define ORACLE_UPLINK_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>
#include <map>
#include <functional>

class OracleUplink {
public:
    OracleUplink(String transport = "BLE");
    ~OracleUplink();

    void setDeviceName(String name);
    void setBleBroadcastName(String name);
    void addCapability(String capability);
    
    // Register callback functions for commands
    void onCommand(String command, std::function<void(String)> callback);
    void onCommand(String command, std::function<void()> callback);

    void begin();
    void update();
    void sendJson(JsonDocument& doc);

private:
    String _transport;
    String _deviceName;
    String _bleBroadcastName;
    std::vector<String> _capabilities;
    std::map<String, std::function<void(String)>> _callbacksWithValue;
    std::map<String, std::function<void()>> _callbacksNoValue;
    
    bool _isConnected;
    unsigned long _lastMsgTime;
    const unsigned long _timeoutMs = 30000; // 30 second watchdog

    void _sendHandshake();
    void _processIncomingText(String text);

    // Hardware specific sub-routines
    void _beginBLE();
    void _updateBLE();
    void _sendJsonBLE(String jsonStr);
};

#endif
