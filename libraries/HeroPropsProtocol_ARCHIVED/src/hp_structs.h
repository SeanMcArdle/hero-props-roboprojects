#ifndef HP_STRUCTS_H
#define HP_STRUCTS_H

#include <Arduino.h>

// -------------------------------------------------------------------------
// 🔮 Magic Constants
// -------------------------------------------------------------------------
#define HP_PROTOCOL_MAGIC   0x4850  // 'H' 'P'
#define HP_PROTOCOL_VERSION 0x01

// -------------------------------------------------------------------------
// 📨 Message Types
// -------------------------------------------------------------------------
enum HpMsgType : uint8_t {
    HP_MSG_HEARTBEAT    = 0x01, // Keep-alive
    HP_MSG_DRIVE        = 0x02, // Movement (Throttle/Turn)
    HP_MSG_ACTION       = 0x03, // Sound/Light triggers
    HP_MSG_ESTOP        = 0x04, // CRITICAL STOP
    HP_MSG_TELEMETRY    = 0x05, // Bot status report
    HP_MSG_CONFIG       = 0x06, // Wireless Configuration (Trim, etc.)
    HP_MSG_LIGHTING     = 0x07, // DMX/Lighting Data (RGB, Dimmer)
    HP_MSG_PAIR_REQ     = 0x10, // Pairing request
    HP_MSG_PAIR_ACK     = 0x11  // Pairing accepted
};

// -------------------------------------------------------------------------
// 📦 Packet Structures
// -------------------------------------------------------------------------
#pragma pack(push, 1) // Ensure no padding bytes for over-the-air transmission

struct HpHeader {
    uint16_t magic;      // 0x4850
    uint8_t  version;    // 0x01
    uint8_t  msgType;    // HpMsgType
    uint8_t  groupID;    // 0=Universal, 1-255=Specific Group
    uint8_t  senderID;   // 0=Commander, 1-15=Bots
    uint8_t  targetID;   // 0xFF=Broadcast
    uint16_t sequence;   // Packet counter
    uint8_t  payloadLen; // Length of following data
};

// Generic Packet Container
struct HpPacket {
    HpHeader header;
    uint8_t  payload[200]; // Max buffer
    uint32_t crc;          // Checksum
};

// -------------------------------------------------------------------------
// 🚚 Payload Definitions
// -------------------------------------------------------------------------

// HP_MSG_CONFIG
struct HpPayloadConfig {
    uint8_t configId;   // 1=TrimLeft, 2=TrimRight, 3=MaxSpeed
    int16_t value;      // The new value
};

// HP_MSG_LIGHTING
struct HpPayloadLighting {
    uint8_t startChannel; // Internal mapping index (0=Main Color, etc.)
    uint8_t count;        // Number of bytes (Max 16 recommended)
    uint8_t data[16];     // RGB / Dimmer values
};

// HP_MSG_DRIVE
struct HpPayloadDrive {
    int16_t throttle; // -100 to 100
    int16_t turn;     // -100 to 100
};

// HP_MSG_ACTION
struct HpPayloadAction {
    uint8_t actionId;  // e.g., 1=Scream, 2=Whistle
    uint8_t parameter; // e.g., Volume or Variant
};

// HP_MSG_HEARTBEAT
struct HpPayloadHeartbeat {
    uint32_t timestamp;
    uint8_t  statusFlags; // Bitmask: 0=OK, 1=LowBat, 2=Error
};

// HP_MSG_TELEMETRY
struct HpPayloadTelemetry {
    float   batteryVoltage;
    uint8_t rssi;
    uint8_t state;        // Current FSM state
};

#pragma pack(pop)

#endif // HP_STRUCTS_H
