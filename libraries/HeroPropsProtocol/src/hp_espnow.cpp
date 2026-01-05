#include "hp_espnow.h"
#include <esp_crc.h> // Hardware CRC support if available, or software fallback

HeroPropsProtocol* HeroPropsProtocol::_instance = nullptr;

HeroPropsProtocol::HeroPropsProtocol() {
    _instance = this;
    _sequenceOut = 0;
    _lastSequenceFromCommander = 0;
}

bool HeroPropsProtocol::begin(uint8_t deviceId, uint8_t groupId, bool isCommander) {
    _deviceId = deviceId;
    _groupId = groupId;
    _isCommander = isCommander;

    // 1. Check WiFi State (Hybrid Mode Support)
    // If WiFi is not active, set it to Station mode.
    // If it IS active (AP or STA), we leave it alone so OTA/WebUI works.
    if (WiFi.getMode() == WIFI_OFF) {
        WiFi.mode(WIFI_STA);
        WiFi.disconnect(); 
    }

    // 2. Init ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("Error initializing ESP-NOW");
        return false;
    }

    // 3. Register Callbacks
    esp_now_register_send_cb(_onDataSent);
    esp_now_register_recv_cb(_onDataRecv);

    // 4. Add Broadcast Peer (Required to send to 0xFF)
    esp_now_peer_info_t peerInfo;
    memset(&peerInfo, 0, sizeof(peerInfo));
    for (int i = 0; i < 6; i++) {
        peerInfo.peer_addr[i] = 0xFF;
    }
    peerInfo.channel = 0; // Use current channel
    peerInfo.encrypt = false;
    
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Failed to add broadcast peer");
        return false;
    }

    Serial.printf("HeroProps Protocol Initialized. ID: %d\n", _deviceId);
    return true;
}

void HeroPropsProtocol::onReceive(HpReceiveCallback callback) {
    _userCallback = callback;
}

bool HeroPropsProtocol::send(uint8_t targetId, HpMsgType type, const uint8_t* data, size_t len) {
    if (len > 200) return false; // Payload too large

    HpPacket packet;
    
    // 1. Fill Header
    packet.header.magic = HP_PROTOCOL_MAGIC;
    packet.header.version = HP_PROTOCOL_VERSION;
    packet.header.msgType = type;
    packet.header.groupID = _groupId;
    packet.header.senderID = _deviceId;
    packet.header.targetID = targetId;
    packet.header.sequence = _sequenceOut++;
    packet.header.payloadLen = len;

    // 2. Copy Payload
    if (data && len > 0) {
        memcpy(packet.payload, data, len);
    }

    // 3. Calculate CRC (Header + Payload)
    // We calculate CRC over the header and the payload bytes
    size_t dataSize = sizeof(HpHeader) + len;
    
    // FIX: Calculate CRC only over the valid data (Header + Payload)
    // Note: We cast to uint8_t* to treat the struct as a byte array
    uint32_t crc = calculateCRC((uint8_t*)&packet, dataSize);

    // FIX: Place the CRC immediately after the payload in the buffer
    // The struct has 'crc' at the end of the 200 byte buffer, but we are sending a shorter packet.
    // We need to copy 'crc' to the byte offset [dataSize]
    memcpy((uint8_t*)&packet + dataSize, &crc, 4);

    // 4. Send
    // For now, we only support Broadcast (0xFF) or we need to add peers dynamically.
    // To keep v1 simple, we will broadcast everything and filter on receiver.
    // In v2, we will manage peer lists.
    uint8_t broadcastAddr[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    
    esp_err_t result = esp_now_send(broadcastAddr, (uint8_t*)&packet, dataSize + 4); // +4 for CRC
    
    return (result == ESP_OK);
}

bool HeroPropsProtocol::sendDrive(uint8_t targetId, int16_t throttle, int16_t turn) {
    HpPayloadDrive payload;
    payload.throttle = throttle;
    payload.turn = turn;
    return send(targetId, HP_MSG_DRIVE, (uint8_t*)&payload, sizeof(payload));
}

bool HeroPropsProtocol::sendAction(uint8_t targetId, uint8_t actionId, uint8_t param) {
    HpPayloadAction payload;
    payload.actionId = actionId;
    payload.parameter = param;
    return send(targetId, HP_MSG_ACTION, (uint8_t*)&payload, sizeof(payload));
}

bool HeroPropsProtocol::sendConfig(uint8_t targetId, uint8_t configId, int16_t value) {
    HpPayloadConfig payload;
    payload.configId = configId;
    payload.value = value;
    return send(targetId, HP_MSG_CONFIG, (uint8_t*)&payload, sizeof(payload));
}

bool HeroPropsProtocol::sendLighting(uint8_t targetId, uint8_t startChannel, const uint8_t* data, uint8_t count) {
    if (count > 16) count = 16; // Safety clamp
    
    HpPayloadLighting payload;
    payload.startChannel = startChannel;
    payload.count = count;
    memcpy(payload.data, data, count);
    
    return send(targetId, HP_MSG_LIGHTING, (uint8_t*)&payload, sizeof(payload));
}

bool HeroPropsProtocol::sendEStop() {
    // E-STOP is a high priority message with no payload
    return send(0xFF, HP_MSG_ESTOP, nullptr, 0);
}

void HeroPropsProtocol::update() {
    // TODO: Implement watchdog checks here for receiver side
}

// -------------------------------------------------------------------------
// Internal Callbacks
// -------------------------------------------------------------------------

void HeroPropsProtocol::_onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    // Optional: Handle delivery confirmation
}

void HeroPropsProtocol::_onDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
    if (!_instance) return;
    _instance->handlePacket(incomingData, len);
}

void HeroPropsProtocol::handlePacket(const uint8_t* data, size_t len) {
    if (len < sizeof(HpHeader)) return; // Too short

    HpPacket* packet = (HpPacket*)data;

    // 1. Validate Magic
    if (packet->header.magic != HP_PROTOCOL_MAGIC) return;

    // 1b. Validate Version (The "Version Trap" Fix)
    if (packet->header.version != HP_PROTOCOL_VERSION) {
        Serial.printf("⚠️ Protocol Version Mismatch: Got %d, Expected %d\n", packet->header.version, HP_PROTOCOL_VERSION);
        return;
    }

    // 2. Validate CRC
    // The CRC is at the end of the received data.
    // len includes header + payload + crc (4 bytes)
    if (len < sizeof(HpHeader) + 4) return;
    
    uint32_t receivedCRC;
    memcpy(&receivedCRC, data + (len - 4), 4);
    
    uint32_t calculatedCRC = calculateCRC(data, len - 4);
    
    if (receivedCRC != calculatedCRC) {
        Serial.println("CRC Mismatch! Dropping packet.");
        return;
    }

    // 3. Filter Target ID
    // If it's not for us and not a broadcast, ignore it.
    if (packet->header.targetID != 0xFF && packet->header.targetID != _deviceId) {
        return;
    }

    // 3b. Filter Group ID (The "Classroom Problem" Fix)
    // If GroupID is 0 (Universal), we accept it.
    // If GroupID matches our group, we accept it.
    // Otherwise, ignore.
    if (packet->header.groupID != 0 && packet->header.groupID != _groupId) {
        return;
    }

    // FIX: Sequence Number Check (Replay Attack Prevention)
    // If the sequence number is old, ignore it.
    // We allow 0 to reset (Commander reboot).
    if (packet->header.sequence > 0 && packet->header.sequence <= _lastSequenceFromCommander) {
        // Potential duplicate or out-of-order packet
        return;
    }
    _lastSequenceFromCommander = packet->header.sequence;

    // 4. Dispatch to User Callback
    if (_userCallback) {
        _userCallback(packet->header, packet->payload, packet->header.payloadLen);
    }
}

uint32_t HeroPropsProtocol::calculateCRC(const uint8_t* data, size_t len) {
    // Simple CRC32 implementation or use ESP32 hardware CRC
    // Using a basic software CRC32 for portability if hardware fails
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1) crc = (crc >> 1) ^ 0xEDB88320;
            else crc >>= 1;
        }
    }
    return ~crc;
}
