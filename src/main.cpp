/******************************************************************************************
 * PROJEKT:   ESP32 32-csatornás relékártya vezérlő
 * VERZIÓ:    v0.01
 * DÁTUM:     2026-09-19 22:00
 * HARDVER:   ESP32 (ESP-WROOM-32E), 32 csatornás relékártya (RS485 Modbus / belső I2C)
 *
 * Változásnapló:
 *   - v0.01 (2026-09-19): Projekt inicializálás — moduláris váz, ModbusMasterRS485 modul
 *                         átvéve a GardenHub Ultimate projektből (újrahasznosítható).
 *   - v0.01: main.cpp csak a kompozíció (modulok bekötése), a logika a lib/ modulokban.
 *
 * Használt pinek (terv — pontosítás a hardver bekötése után):
 *   GPIO13 -> RS485 TX (a ModbusMasterRS485 modul felé)
 *   GPIO16 -> RS485 RX (a ModbusMasterRS485 modul felé)
 *
 * Szabad pinek: a többség (a végleges bekötés szerint pontosítandó).
 * Kerülendő pinek (ESP32 strap/boot): GPIO0, GPIO2, GPIO12, GPIO15.
 * Input-only: GPIO34..39 (nem használhatók kimenetként).
 * ==========================================================================================*/
#include <Arduino.h>

#include "ModbusMasterRS485.h"

#define APP_NAME     "ESP32 32ch Relayboard"
#ifndef APP_VERSION
#define APP_VERSION  "v0.01"
#endif

// A 32 csatornás relékártya RS485 Modbus master modulja.
// (RX GPIO, TX GPIO, UART port) — a végleges bekötés szerint módosítandó.
ModbusMasterRS485 relayBoard(16, 13, 2);

// A külső relékártya Modbus slave címe (DIP-kapcsolóval beállítva)
static const uint8_t RELAY_SLAVE_ID = 0x01;

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.printf("\n=== %s %s ===\n", APP_NAME, APP_VERSION);

  relayBoard.begin(RELAY_SLAVE_ID, 9600);
  Serial.printf("[RS485] Modbus master indítva: slave=0x%02X @9600 8N1\n", RELAY_SLAVE_ID);

  // Biztonságos alaphelyzet: minden relé KI
  relayBoard.relayCloseAll();
}

void loop() {
  // A vezérlési logika modulokba kerül (pl. web/RS485/ütemező).
  // Ez a váz egyelőre csak a master modult élesíti.
  //
  // Példa (teszt):
  //   relayBoard.relayOpen(1);  delay(1000);
  //   relayBoard.relayClose(1); delay(1000);
}

// THE END - Kód vége - verzió v0.01
