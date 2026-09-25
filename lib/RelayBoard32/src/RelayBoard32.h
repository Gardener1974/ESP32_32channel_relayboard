/******************************************************************************************
 * PROJEKT:   ESP32 32-csatornás relékártya vezérlő
 * MODUL:     RelayBoard32
 * VERZIÓ:    v0.01
 * DÁTUM:     2026-09-19 22:57
 * HARDVER:   ESP32 (ESP-WROOM-32E) + 32 csatornás relékártya, 4x 74HC595 léptetőregiszter
 *
 * Cél: 32 relé (0..31) ki-/bekapcsolása egy 32-bites érték kiküldésével.
 *      A kiíró réteg a működő pin-teszt mintakód (gemini-code.cpp) struktúráját
 *      használja: DATA / LATCH / CLOCK (+ opcionális OE, aktív LOW).
 *
 * A pinek NEM fix #define-ok: a begin()-ben paraméterként adhatók (modul-konvenció).
 ******************************************************************************************/
#pragma once
#include <Arduino.h>

class RelayBoard32 {
public:
  // oePin = -1, ha nincs külön Output-Enable láb
  RelayBoard32(int dataPin, int latchPin, int clockPin, int oePin = -1);

  // Pinek beállítása és valamennyi relé KI állapotba tétele
  void begin();

  // Egy relé be/ki. ch = 0..31. on = true (BE) / false (KI)
  void setRelay(uint8_t ch, bool on);

  // A 32 csatorna teljes állapotának kiírása egyszerre
  void writeAll(uint32_t mask);

  // Az összes relé KI (mask = 0)
  void allOff();

  // Aktuális állapot-maszk (bit i = relé i állapota)
  uint32_t getState() const { return _state; }

  // Egy csatorna állapota a szoftveres tükör szerint
  bool getRelay(uint8_t ch) const { return (_state & (1UL << ch)) != 0; }

  static const uint8_t CHANNEL_COUNT = 32;

private:
  int      _dataPin;
  int      _latchPin;
  int      _clockPin;
  int      _oePin;
  uint32_t _state = 0;

  void _send32(uint32_t value);
};

// THE END - Kód vége - verzió v0.01
