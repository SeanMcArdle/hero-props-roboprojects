#ifndef WEB_INTERFACE_H
#define WEB_INTERFACE_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>

struct WebInput {
    int driveX;      // -100 to 100
    int driveY;      // -100 to 100
    int domeX;       // -100 to 100
    int domeY;       // -100 to 100
    int actionId;    // 0 = None
    bool eStop;      // Emergency Stop
};

class WebInterface {
public:
    void begin();
    WebInput getInput(); // Returns current state and clears one-shot events (like actionId)

private:
    static void handleRoot(AsyncWebServerRequest *request);
    static void handleNotFound(AsyncWebServerRequest *request);
    static void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
    static void handleWebSocketMessage(void *arg, uint8_t *data, size_t len);

    static WebInput _input; // Shared state
};

#endif
