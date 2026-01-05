#ifndef HP_ESPNOW_H
#define HP_ESPNOW_H

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include "hp_structs.h"

// Callback function types
typedef std::function<void(const HpHeader& header, const uint8_t* payload, size_t len)> HpReceiveCallback;

class HeroPropsProtocol {
public:
    HeroPropsProtocol();

    // Initialization
    bool begin(uint8_t deviceId, uint8_t groupId = 1, bool isCommander = false);
    
    // Sending
    bool send(uint8_t targetId, HpMsgType type, const uint8_t* data, size_t len);
    bool sendDrive(uint8_t targetId, int16_t throttle, int16_t turn);
    bool sendAction(uint8_t targetId, uint8_t actionId, uint8_t param);
    bool sendConfig(uint8_t targetId, uint8_t configId, int16_t value);
    bool sendLighting(uint8_t targetId, uint8_t startChannel, const uint8_t* data, uint8_t count);
    bool sendEStop(); // Broadcasts E-STOP immediately

    // Receiving
    void onReceive(HpReceiveCallback callback);
    
    // Maintenance
    void update(); // Call in loop() for watchdog checks
    
    // Getters
    uint8_t getDeviceId() const { return _deviceId; }
    uint8_t getGroupId() const { return _groupId; }
    bool isConnected(uint8_t peerId);

private:
    uint8_t _deviceId;
    uint8_t _groupId;
    bool _isCommander;
    uint16_t _sequenceOut;
    HpReceiveCallback _userCallback;
    
    // Internal ESP-NOW callbacks (static wrappers)
    static void _onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);
    static void _onDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len);
    
    // Instance pointer for static callbacks
    static HeroPropsProtocol* _instance;
    
    // Helpers
    uint32_t calculateCRC(const uint8_t* data, size_t len);
    void handlePacket(const uint8_t* data, size_t len);
};

#endif // HP_ESPNOW_H
