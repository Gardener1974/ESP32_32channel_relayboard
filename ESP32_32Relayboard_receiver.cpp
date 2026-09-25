/******************************************************************************************
 * PROJEKT:   ESP32 32-csatornás relékártya vezérlő (RECEIVER)
 * VERZIÓ:    v0.01
 * DÁTUM:     2026-09-19 22:57
 * HARDVER:   ESP32 (ESP-WROOM-32E) + 32 csatornás relékártya (4x 74HC595)
 *
 * Szerep:  "BUTA" ESP-NOW FOGADÓ. Routerhez NEM csatlakozik. A KinCony (GardenHub)
 *          sender ESP-NOW csomagjait fogadja, és végrehajtja a parancsokat.
 *
 * Jelenlegi funkció: relé X mp-re BE, majd KI (CMD_IMMEDIATE + duration).
 *
 * Változásnapló:
 *   - v0.01 (2026-09-19): Fogadó váz — ESP-NOW fogadás + 32 relé vezérlés,
 *                         RelayBoard32 és EspNowSchedule modulokkal.
 *
 * Használt pinek (a gemini-code.cpp teszt szerinti kiinduló kiosztás):
 *   GPIO14 -> DATA   (74HC595 szeriális adat)
 *   GPIO12 -> LATCH  (RCLK / tároló)   [FIGYELEM: GPIO12 strap pin!]
 *   GPIO13 -> CLOCK  (SRCLK)
 *   GPIO15 -> OE     (Output Enable, aktív LOW) [FIGYELEM: GPIO15 strap pin!]
 *
 * Szabad pinek: a többi (a végleges bekötés szerint).
 * Kerülendő (strap/boot): GPIO0, GPIO2, GPIO12, GPIO15 — itt tudatosan használt (teszt),
 *                        ha kell, más szabad pinekre áthelyezhető.
 * Input-only: GPIO34..39.
 * ==========================================================================================*/
#include <Arduino.h>

#include "RelayBoard32.h"
#include "EspNowSchedule.h"

#define APP_NAME     "ESP32 32ch Relayboard (receiver)"
#ifndef APP_VERSION
#define APP_VERSION  "v0.01"
#endif

// ---- Relé-kiíró (konfigurálható pinek) ----
// A 4 pin a valós hardver-teszt (gemini-code.cpp) nyertes kombinációja szerint módosítható.
RelayBoard32 relayBoard(14, 12, 13, 15);   // DATA, LATCH, CLOCK, OE

// ---- ESP-NOW fogadó + egyszerű időzítés ----
EspNowSchedule scheduler;

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.printf("\n=== %s %s ===\n", APP_NAME, APP_VERSION);

  // Relék: pinek be + induláskor minden relé KI
  relayBoard.begin();

  // ESP-NOW fogadó indítása (router nélkül) + a relé-kiíró bekötése
  scheduler.attachRelaySetter([](uint8_t ch, bool on) {
    relayBoard.setRelay(ch, on);
  });

  if (!scheduler.begin()) {
    Serial.println("[HIBA] ESP-NOW fogadó indítása nem sikerült!");
  } else {
    Serial.println("Kész a fogadásra. Írd be a senderbe a fenti MAC-címet!");
  }
}

void loop() {
  scheduler.loop();   // lejárt "X mp-re BE" időzítők kikapcsolása
  delay(2);
}

// THE END - Kód vége - verzió v0.01
