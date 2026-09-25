/******************************************************************************************
 * PROJEKT:   ESP32 32-csatornás relékártya vezérlő
 * MODUL:     EspNowSchedule (implementáció)
 * VERZIÓ:    v0.02
 * DÁTUM:     2026-09-19 23:48
 * HARDVER:   ESP32 (ESP-WROOM-32E)
 *
 * "Buta" ESP-NOW fogadó. Nem csatlakozik routerhez.
 * Az egyszerű funkció: CMD_IMMEDIATE + duration -> relé X mp-re BE, majd KI.
 * A teljes ütemezés (epoch-os) egyelőre NINCS implementálva (későbbi fejlesztés).
 *
 * VÁLTOZÁSNAPLÓ:
 *   - v0.02 (2026-09-19 23:48): A WiFi.h TELJESEN eltávolítva; helyette ESP-IDF API
 *                              (esp_wifi_set_mode/esp_wifi_start/esp_read_mac). A
 *                              WiFi.disconnect() törölve — ez okozta az "SSID too long
 *                              or missing!" logot. Nincs több SSID/jelszó üzenet.
 *   - v0.01 (2026-09-19 22:57): Első verzió.
 ******************************************************************************************/
#include "EspNowSchedule.h"
#include <esp_now.h>       // ESP-NOW (alsobb szintu, WiFi.h nem kell)
#include <esp_wifi.h>      // csak a radio inditasa
#include <esp_mac.h>       // esp_read_mac() a MAC-cimhez

EspNowSchedule* EspNowSchedule::_instance = nullptr;

/* Az ESP-NOW fogadás callback-je (C aláírás). Továbbítja a példányra. */
void EspNowSchedule::_onRecv(const uint8_t* mac, const uint8_t* data, int len) {
  if (!EspNowSchedule::_instance) return;
  if (len < (int)sizeof(SchedulePacket)) return;    // túl rövid, eldobjuk

  SchedulePacket pkt;
  memcpy(&pkt, data, sizeof(pkt));
  EspNowSchedule::_instance->_handlePacket(pkt);
}

bool EspNowSchedule::begin() {
  _instance = this;

  // Csak a radio: ESP-NOW-hoz nem kell router. A WiFi.h OSZTALYT NEM hasznaljuk,
  // csak az also szintu ESP-IDF API-t -> nem keletkezhet semmilyen SSID/jelszo log.
  esp_wifi_set_mode(WIFI_MODE_STA);   // radio STA modban (nem csatlakozik sehova)
  esp_wifi_start();

  if (esp_now_init() != ESP_OK) {
    Serial.println("[ESP-NOW] init HIBA!");
    return false;
  }
  esp_now_register_recv_cb(_onRecv);

  // MAC-cim kiolvasasa az ESP-IDF-bol (nem a WiFi osztalybol)
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  Serial.printf("[ESP-NOW] Fogado kesz (channel=%d). Sajat MAC: %s\n",
                ESP_NOW_CHANNEL, macStr);
  return true;
}

void EspNowSchedule::_clearAllTimers() {
  for (int i = 0; i < 32; i++) { _timerActive[i] = false; _offAt[i] = 0; }
}

/* Egy esemény végrehajtása az EGYSZERŰ módban:
     action=1 (BE): relé BE; ha duration>0 -> X mp múlva KI (időzítő)
     action=0 (KI): relé KI; az időzítő törlése erre a csatornára */
void EspNowSchedule::_schedule(const RelayEvent& e) {
  if (e.relay > 31) return;

  if (e.action == 1) {
    if (_setRelay) _setRelay(e.relay, true);
    if (e.duration > 0) {
      _offAt[e.relay]     = millis() + (uint32_t)e.duration * 1000UL;
      _timerActive[e.relay] = true;
      Serial.printf("[RELÉ] #%d BE, %u mp-re (KI: %lu)\n",
                    e.relay + 1, (unsigned)e.duration,
                    (unsigned long)_offAt[e.relay]);
    } else {
      _timerActive[e.relay] = false;   // végleges BE
      Serial.printf("[RELÉ] #%d BE (végleges)\n", e.relay + 1);
    }
  } else {
    if (_setRelay) _setRelay(e.relay, false);
    _timerActive[e.relay] = false;
    Serial.printf("[RELÉ] #%d KI\n", e.relay + 1);
  }
}

void EspNowSchedule::_handlePacket(const SchedulePacket& pkt) {
  if (pkt.magic != PACKET_MAGIC) {
    Serial.printf("[ESP-NOW] Hibás magic (0x%02X) -> eldobva\n", pkt.magic);
    return;
  }
  _packetCount++;

  uint8_t n = (pkt.count > MAX_EVENTS) ? MAX_EVENTS : pkt.count;

  switch (pkt.command) {
    case CMD_IMMEDIATE:
      Serial.printf("[ESP-NOW] IMMEDIATE csomag, %u esemény\n", n);
      for (uint8_t i = 0; i < n; i++) _schedule(pkt.events[i]);
      break;

    case CMD_OVERWRITE:
      Serial.printf("[ESP-NOW] OVERWRITE csomag, %u esemény\n", n);
      _clearAllTimers();
      for (uint8_t i = 0; i < n; i++) _schedule(pkt.events[i]);
      break;

    case CMD_APPEND:
      Serial.printf("[ESP-NOW] APPEND csomag, %u esemény\n", n);
      for (uint8_t i = 0; i < n; i++) _schedule(pkt.events[i]);
      break;

    case CMD_CLEAR:
      Serial.println("[ESP-NOW] CLEAR csomag -> összes relé KI + időzítők törölve");
      _clearAllTimers();
      if (_setRelay) for (uint8_t ch = 0; ch < 32; ch++) _setRelay(ch, false);
      break;

    default:
      Serial.printf("[ESP-NOW] Ismeretlen parancs: %u -> eldobva\n", pkt.command);
      return;
  }

  _lastInfo = "cmd=" + String(pkt.command) + " count=" + String(n);
}

void EspNowSchedule::loop() {
  uint32_t now = millis();
  for (int ch = 0; ch < 32; ch++) {
    if (_timerActive[ch] && (int32_t)(now - _offAt[ch]) >= 0) {
      _timerActive[ch] = false;
      if (_setRelay) _setRelay((uint8_t)ch, false);
      Serial.printf("[IDŐZÍTŐ] relé #%d KI (lejárt)\n", ch + 1);
    }
  }
}

// THE END - Kód vége - verzió v0.02
