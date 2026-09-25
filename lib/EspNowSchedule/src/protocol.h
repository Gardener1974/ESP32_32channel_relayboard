/******************************************************************************************
 * PROJEKT:   ESP32 32-csatornás relékártya vezérlő — KÖZÖS ESP-NOW PROTOKOLL
 * FÁJL:      protocol.h
 * VERZIÓ:    v0.02
 * DÁTUM:     2026-09-19 23:51
 * HARDVER:   ESP32 (ESP-WROOM-32E) — receiver; a sender a KinCony (GardenHub) modul
 *
 * SZEREP:
 *   Közös szerkezet-leírás (relé modul <-> vezérlő modul). A vezérlő ABSZOLÚT
 *   időpontokat küld (UTC epoch másodperc), a fogadó pedig ehhez kapcsol.
 *
 * VÁLTOZÁSNAPLÓ:
 *   - v0.02 (2026-09-19 23:51): Verzió egységesítve a receiver firmware-rel (v0.02).
 *                              Tartalom változatlan (a protokoll stabil).
 *   - v0.01 (2026-09-19 22:57): Első verzió.
 *
 * FÜGGŐSÉG:
 *   #include <stdint.h> (Arduino alatt automatikusan elérhető)
 * ------------------------------------------------------------------------------------------
 ******************************************************************************************/
#pragma once
#include <stdint.h>

// ============================================================================
//  KÖZÖS ESP-NOW PROTOKOLL  (relé modul <-> vezérlő modul)
// ============================================================================

#define ESP_NOW_CHANNEL   1        // Wi-Fi csatorna (mindkét modulon ugyanaz!)
#define MAX_EVENTS        16       // max ennyi esemény egy csomagban

// ---- Egyetlen ütemezett esemény -------------------------------------------
struct RelayEvent {
  uint8_t  relay;        // 0..31  -> melyik relé
  uint8_t  action;       // 0 = KI, 1 = BE
  uint16_t duration;     // ha action=1: meddig maradjon BE (másodperc), 0 = végleges
  uint32_t startEpoch;   // UTC epoch (másodperc): mikor hajtódjon végre
};

// ---- A teljes ütemezés-csomag ---------------------------------------------
struct SchedulePacket {
  uint8_t    magic;      // 0xA5 - csomag-azonosító
  uint8_t    command;    // lásd lent
  uint8_t    count;      // érvényes események száma (0..MAX_EVENTS)
  RelayEvent events[MAX_EVENTS];
};

#define PACKET_MAGIC   0xA5
#define CMD_OVERWRITE  0
#define CMD_APPEND     1
#define CMD_CLEAR      2
#define CMD_IMMEDIATE  3

// THE END - Kód vége - verzió v0.02
