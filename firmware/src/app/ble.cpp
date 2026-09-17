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
// Horario de reset ja formatado pelo Mac (ex: "15:42", "qua 09:00"); a placa
// nao tem relogio. Escrito na task BLE, lido no loop sob o flag s_has_new.
static char s_sreset_str[16] = "--";
static char s_wreset_str[16] = "--";
static volatile bool s_has_new = false;
static volatile bool s_connected = false;
static volatile bool s_auth_ok = true;   // false quando o Mac avisa auth=0

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
        // Sinal de token expirado ({"auth":0}) tem prioridade: sem uso, so
        // marca o estado para o loop trocar a UI. Ausente => 1 (ok).
        if ((doc["auth"] | 1) == 0) {
            s_auth_ok = false;
            Serial.println("[ble] token expirado (auth=0)");
            return;
        }

        int session = doc["session"] | -1;
        int weekly = doc["weekly"] | -1;
        if (session < 0 && weekly < 0) {
            Serial.println("[ble] payload sem session/weekly");
            return;
        }
        s_auth_ok = true;   // chegou uso valido => token voltou
        if (session >= 0) s_session = session;
        if (weekly >= 0) s_weekly = weekly;
        const char *sr = doc["s_reset_str"] | "--";
        const char *wr = doc["w_reset_str"] | "--";
        snprintf(s_sreset_str, sizeof(s_sreset_str), "%s", sr);
        snprintf(s_wreset_str, sizeof(s_wreset_str), "%s", wr);
        s_has_new = true;
        Serial.printf("[ble] recebido session=%d weekly=%d reset=%s/%s\n",
                      (int)s_session, (int)s_weekly, s_sreset_str,
                      s_wreset_str);
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

bool ble_get_usage(int *session, int *weekly, char *s_reset_str,
                   char *w_reset_str, size_t n) {
    if (!s_has_new) return false;
    *session = s_session;
    *weekly = s_weekly;
    snprintf(s_reset_str, n, "%s", s_sreset_str);
    snprintf(w_reset_str, n, "%s", s_wreset_str);
    s_has_new = false;
    return true;
}

bool ble_is_connected(void) { return s_connected; }

bool ble_auth_ok(void) { return s_auth_ok; }
