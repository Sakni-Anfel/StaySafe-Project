#include <SPI.h>
#include <LoRa.h>
#include <WiFi.h>
#include <MQTT.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

#define NSS   5
#define RESET 14
#define DIO0  2

const char SSID[] = "000000";
const char PASS[] = "00000";

struct MqttMsg {
    char topic[32];
    char payload[16];
};

static QueueHandle_t msgQueue;
static WiFiClient    net;
static MQTTClient    mqttClient;

static inline void enqueue(const char* topic, const char* payload) {
    MqttMsg m;
    strlcpy(m.topic,   topic,   sizeof(m.topic));
    strlcpy(m.payload, payload, sizeof(m.payload));
    if (xQueueSend(msgQueue, &m, 0) != pdTRUE) {
        Serial.println("[Q] FULL — msg dropped");
    }
}

static const char* alertToState(int lvl) {
    if (lvl == 2) return "DANGER";
    if (lvl == 1) return "WARNING";   // ← was missing in practice
    return "SAFE";
}
static const char* alertToWater(int lvl) {
    if (lvl == 2) return "CRITICAL";
    if (lvl == 1) return "HIGH";      // ← was missing in practice
    return "NORMAL";
}
static const char* rainToStr(int cat) {
    if (cat == 2) return "HEAVY";
    if (cat == 1) return "LIGHT";     
    return "NONE";
}


static void taskLoRa(void* /*param*/) {
    SPI.begin();
    LoRa.setPins(NSS, RESET, DIO0);

    while (!LoRa.begin(433E6)) {
        Serial.println("[LoRa] Init failed, retrying…");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    LoRa.setSpreadingFactor(7);
    LoRa.setSignalBandwidth(125E3);
    LoRa.setCodingRate4(5);
    Serial.println("[LoRa] RX ready on Core 1");

    for (;;) {
        int pktSize = LoRa.parsePacket();
        if (pktSize == 0) {
            vTaskDelay(pdMS_TO_TICKS(2)); // 2 ms poll — much faster than 10 ms
            continue;
        }

        char buf[128] = {};
        int  i = 0;
        while (LoRa.available() && i < (int)sizeof(buf) - 1) {
            buf[i++] = (char)LoRa.read();
        }
        Serial.printf("[LoRa] RX(%d): %s\n", pktSize, buf);

        if (strncmp(buf, "ALERT:", 6) == 0) {
            int lvl = atoi(buf + 6);                   // 0, 1, or 2
            enqueue("safty/state",      alertToState(lvl));
            enqueue("safty/waterlevel", alertToWater(lvl));
            Serial.printf("[LoRa] ALERT lvl=%d → %s / %s\n",
                          lvl, alertToState(lvl), alertToWater(lvl));
        }

        else if (strncmp(buf, "D:", 2) == 0) {
            char* aPtr  = strstr(buf, "A:");
            char* rfPtr = strstr(buf, "RF:");

            int alertLvl = aPtr  ? atoi(aPtr  + 2) : 0; // 0, 1, or 2
            int rainCat  = rfPtr ? atoi(rfPtr + 3) : 0; // 0, 1, or 2

            enqueue("safty/state",      alertToState(alertLvl));
            enqueue("safty/waterlevel", alertToWater(alertLvl));
            enqueue("safty/rainfall",   rainToStr(rainCat));

            Serial.printf("[LoRa] DATA alertLvl=%d rainCat=%d → %s / %s / %s\n",
                          alertLvl, rainCat,
                          alertToState(alertLvl),
                          alertToWater(alertLvl),
                          rainToStr(rainCat));
        }
    }
}


static void taskMqtt(void* /*param*/) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(SSID, PASS);
    mqttClient.begin("mqtt-dashboard.com", 1883, net);

    Serial.print("[WiFi] Connecting");
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print('.');
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    Serial.printf("\n[WiFi] IP: %s\n", WiFi.localIP().toString().c_str());

    while (!mqttClient.connect("esp32")) {
        Serial.println("[MQTT] Connecting…");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    Serial.println("[MQTT] Connected on Core 0");

    uint32_t lastReconnect = 0;

    for (;;) {
        uint32_t now = millis();

       
        if (now - lastReconnect > 5000) {
            lastReconnect = now;

            if (WiFi.status() != WL_CONNECTED) {
                Serial.println("[WiFi] Lost — reconnecting…");
                WiFi.disconnect();
                WiFi.begin(SSID, PASS);
            } else if (!mqttClient.connected()) {
                Serial.println("[MQTT] Lost — reconnecting…");
                mqttClient.connect("esp32");
            }
        }

        mqttClient.loop();

      
        MqttMsg m;
        while (xQueueReceive(msgQueue, &m, 0) == pdTRUE) {
            if (mqttClient.connected()) {
                mqttClient.publish(m.topic, m.payload);
                Serial.printf("[MQTT] %-22s → %s\n", m.topic, m.payload);
            }
            mqttClient.loop(); // keep TCP stack alive between publishes
        }

        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

void setup() {
    Serial.begin(115200);

    msgQueue = xQueueCreate(20, sizeof(MqttMsg)); // depth 20 for burst tolerance
    configASSERT(msgQueue);

    xTaskCreatePinnedToCore(taskLoRa, "LoRa", 4096, nullptr, 2, nullptr, 1);
    xTaskCreatePinnedToCore(taskMqtt, "MQTT", 8192, nullptr, 1, nullptr, 0);
}

void loop() {
    vTaskDelay(portMAX_DELAY); 
}
