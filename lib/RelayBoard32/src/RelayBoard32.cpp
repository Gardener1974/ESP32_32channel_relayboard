/******************************************************************************************
 * PROJEKT:   ESP32 32-csatornás relékártya vezérlő
 * MODUL:     RelayBoard32 (implementáció)
 * VERZIÓ:    v0.02
 * DÁTUM:     2026-09-19 22:57
 * HARDVER:   ESP32 (ESP-WROOM-32E) + 32 csatornás relékártya (4x 74HC595)
 *
 * A kiíró struktúra a működő pin-teszt mintakódból (gemini-code.cpp):
 *   - latch LOW -> 4x shiftOut(MSBFIRST) -> latch HIGH
 *   - OE aktív LOW (engedélyezés), ha van ilyen láb
 ******************************************************************************************/
#include "RelayBoard32.h"

RelayBoard32::RelayBoard32(int dataPin, int latchPin, int clockPin, int oePin)
  : _dataPin(dataPin), _latchPin(latchPin), _clockPin(clockPin), _oePin(oePin) {}

void RelayBoard32::begin() {
  pinMode(_dataPin, OUTPUT);
  pinMode(_latchPin, OUTPUT);
  pinMode(_clockPin, OUTPUT);

  if (_oePin != -1) {
    pinMode(_oePin, OUTPUT);
    digitalWrite(_oePin, LOW);   // OE aktív LOW = kimenet engedélyezve
  }
  allOff();
}

void RelayBoard32::_send32(uint32_t value) {
  digitalWrite(_latchPin, LOW);
  shiftOut(_dataPin, _clockPin, MSBFIRST, (value >> 24) & 0xFF);
  shiftOut(_dataPin, _clockPin, MSBFIRST, (value >> 16) & 0xFF);
  shiftOut(_dataPin, _clockPin, MSBFIRST, (value >> 8)  & 0xFF);
  shiftOut(_dataPin, _clockPin, MSBFIRST,  value        & 0xFF);
  digitalWrite(_latchPin, HIGH);
}

void RelayBoard32::setRelay(uint8_t ch, bool on) {
  if (ch >= CHANNEL_COUNT) return;
  if (on) _state |=  (1UL << ch);
  else    _state &= ~(1UL << ch);
  _send32(_state);
}

void RelayBoard32::writeAll(uint32_t mask) {
  _state = mask;
  _send32(_state);
}

void RelayBoard32::allOff() {
  writeAll(0);
}

// THE END - Kód vége - verzió v0.02
