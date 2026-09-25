# RelayBoard32

32 csatornás relékártya vezérlése ESP32-ről, **4x 74HC595 léptetőregiszterrel**
(DATA / LATCH / CLOCK, opcionális OE aktív-LOW).

A kiíró struktúra a működő pin-teszt mintakódból származik (`gemini-code.cpp`).

## Használat

```cpp
#include "RelayBoard32.h"

RelayBoard32 board(14, 12, 13, 15);   // DATA, LATCH, CLOCK, OE

void setup() {
  board.begin();          // pinek be + minden relé KI
  board.setRelay(0, true);  // relé 1 BE
  delay(1000);
  board.setRelay(0, false); // relé 1 KI
}
```

## API

| Metódus | Leírás |
|---------|--------|
| `RelayBoard32(data, latch, clock, oe=-1)` | konstruktor, `oe=-1` ha nincs láb |
| `begin()` | pinek beállítása + összes relé KI |
| `setRelay(ch, on)` | `ch=0..31`, relé be/ki |
| `writeAll(mask)` | 32-bites maszk kiírása egyszerre |
| `allOff()` | minden relé KI |
| `getState()` / `getRelay(ch)` | szoftveres állapot-tükör |

## Megjegyzés a hardverhez

A **működő pin-kombináció** a valós tesztből derül ki. Alapérték a konstruktorban
adható meg; ha más kiosztás kell, csak a `main.cpp`-ben a négy pin módosul.

## Verzió

v0.01 — 2026-09-19
