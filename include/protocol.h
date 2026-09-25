#pragma once
#include <stdint.h>

// ============================================================================
//  KÖZÖS ESP-NOW PROTOKOLL  (relé modul <-> vezérlő modul)
//
//  A vezérlő ABSZOLÚT időpontokat küld (UTC epoch másodperc), a relé modul
//  az NTP-ről szinkronizált pontos órához képest pontosan akkor kapcsol.
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
// command:
//   0 = OVERWRITE : a meglévő ütemezést eldobja, ezt teszi be
//   1 = APPEND    : hozzáfűzi a meglévőkhöz
//   2 = CLEAR     : törli az összes ütemezett eseményt (count figyelmen kívül)
//   3 = IMMEDIATE : azonnali végrehajtás (startEpoch figyelmen kívül, most))
struct SchedulePacket {
  uint8_t    magic;      // 0xA5 - csomag-azonosító
  uint8_t    command;
  uint8_t    count;      // érvényes események száma (0..MAX_EVENTS)
  RelayEvent events[MAX_EVENTS];
};

#define PACKET_MAGIC   0xA5
#define CMD_OVERWRITE  0
#define CMD_APPEND     1
#define CMD_CLEAR      2
#define CMD_IMMEDIATE  3
