/******************************************************************************************
 * PROJEKT:   ESP32 32-csatornás relékártya vezérlő
 * MODUL:     EspNowSchedule
 * VERZIÓ:    v0.01
 * DÁTUM:     2026-09-19 22:57
 * HARDVER:   ESP32 (ESP-WROOM-32E)
 *
 * Cél: "buta" ESP-NOW fogadó. Routerhez NEM csatlakozik, csak a rádiót indítja
 *      (WiFi.mode(WIFI_STA)) és várja a KinCony sender csomagjait.
 *
 * Az egyszerű, MOST kért funkció:
 *      kap egy parancsot -> felkapcsol egy relét X mp-re -> majd lekapcsolja.
 *      Ez gyakorlatilag a CMD_IMMEDIATE + duration mező.
 *
 * A relé-kiírást nem ez a modul végzi: egy visszahívás (callback) adja át
 * a kiíró felé (laza csatolás, modul-konvenció).
 ******************************************************************************************/
#pragma once
#include <Arduino.h>
#include <functional>

#include "protocol.h"

class EspNowSchedule {
public:
  // relé be/ki visszahívás: (channel 0..31, on)
  using RelaySetFn = std::function<void(uint8_t, bool)>;

  EspNowSchedule() = default;

  // A relé-kiíró függvény bekötése (pl. RelayBoard32::setRelay)
  void attachRelaySetter(RelaySetFn fn) { _setRelay = fn; }

  // ESP-NOW fogadó indítása. A csatorna a protocol.h ESP_NOW_CHANNEL-je.
  // Visszaad: true, ha sikeres.
  bool begin();

  // A loop()-ból hívandó: lejárt "X mp-re be" időzítők kikapcsolása.
  void loop();

  // Diagnosztika
  uint32_t getPacketCount() const { return _packetCount; }

  // Az utolsó fogadott csomag emberi formában (soros loghoz)
  String   lastPacketInfo() const { return _lastInfo; }

private:
  RelaySetFn  _setRelay;

  // "X mp-re be" időzítők (egy csatornához egy időpont). max 32 csatorna.
  uint32_t    _offAt[32];        // mikor kell KI (millis), 0 = nincs időzítő
  bool        _timerActive[32];

  uint32_t    _packetCount = 0;
  String      _lastInfo    = "(még nincs csomag)";

  void _handlePacket(const SchedulePacket& pkt);
  void _schedule(const RelayEvent& e);
  void _clearAllTimers();

  // ESP-NOW fogadó callback (statikus), a C-API miatt barátságos.
  static void _onRecv(const uint8_t* mac, const uint8_t* data, int len);
  static EspNowSchedule* _instance;
};

// THE END - Kód vége - verzió v0.01
