#include "ble.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <NimBLEDevice.h>

// UUIDs proprios (128-bit) do servico e da caracteristica de dados.
#define SVC_UUID "c0de0001-feed-4d61-9a11-0123456789ab"
#define CHR_UUID "c0de0002-feed-4d61-9a11-0123456789ab"

// Estado compartilhado entre a task BLE e a task do LVGL. volatile porque e
// escrito num contexto e lido noutro; os valores sao ints simples (leitura/
// escrita atomica no ESP32).
static volatile int s_session = 0;
static volatile int s_weekly = 0;
static volatile bool s_has_new = false;
static volatile bool s_connected = false;

// Recebe o JSON escrito pelo Mac e atualiza o estado. NAO toca no LVGL.
class DataCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic *chr) override {
        std::string v = chr->getValue();
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, v);
        if (err) {
            Serial.printf("[ble] JSON invalido: %s\n", err.c_str());
            return;
        }
        int session = doc["session"] | -1;
        int weekly = doc["weekly"] | -1;
        if (session < 0 && weekly < 0) {
            Serial.println("[ble] payload sem session/weekly");
            return;
        }
        if (session >= 0) s_session = session;
        if (weekly >= 0) s_weekly = weekly;
        s_has_new = true;
        Serial.printf("[ble] recebido session=%d weekly=%d\n", (int)s_session,
                      (int)s_weekly);
    }
};

class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer *server) override {
        s_connected = true;
        Serial.println("[ble] cliente conectado");
    }
    void onDisconnect(NimBLEServer *server) override {
        s_connected = false;
        Serial.println("[ble] cliente desconectado; volta a anunciar");
        NimBLEDevice::startAdvertising();
    }
};

void ble_begin(void) {
    NimBLEDevice::init("TokenMeter");

    NimBLEServer *server = NimBLEDevice::createServer();
    server->setCallbacks(new ServerCallbacks());

    NimBLEService *svc = server->createService(SVC_UUID);
    NimBLECharacteristic *chr = svc->createCharacteristic(
        CHR_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
    chr->setCallbacks(new DataCallbacks());
    svc->start();

    NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();
    adv->addServiceUUID(SVC_UUID);
    adv->setScanResponse(true);
    NimBLEDevice::startAdvertising();

    Serial.println("[ble] anunciando como 'TokenMeter'");
}

bool ble_get_usage(int *session, int *weekly) {
    if (!s_has_new) return false;
    *session = s_session;
    *weekly = s_weekly;
    s_has_new = false;
    return true;
}

bool ble_is_connected(void) { return s_connected; }
