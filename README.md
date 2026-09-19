# ESP32 32-csatornás relékártya vezérlő

**32 csatornás relékártya vezérlése ESP32 (ESP-WROOM-32E) alaplapon, moduláris felépítéssel.**

- **Verzió:** v0.01
- **Dátum:** 2026-09-19 22:00
- **Platform:** PlatformIO (Arduino framework)

---

## Cél

Egy 32 csatornás (kimeneti) relékártya vezérlése ESP32-vel. A projekt a
**GardenHub Ultimate** ökoszisztéma tapasztalataira és újrahasznosítható moduljaira épül
(pl. `lib/ModbusMasterRS485`), de önálló, külön projektként fejlődik.

> **Állapot:** a kód éppen indul — ez a **moduláris váz**. A pontos csatorna-kiosztás és
> vezérlési logika a hardver bekötésének pontosítása után készül el.

---

## Felépítés (moduláris — kötelező)

```
ESP32_32channel_relayboard/
├── platformio.ini
├── README.md
├── src/
│   └── main.cpp                 # csak kompozíció (modulok bekötése)
├── include/
├── test/
└── lib/
    └── ModbusMasterRS485/       # újrahasznosítható RS485 Modbus master modul
        ├── library.json
        ├── README.md
        ├── src/
        │   ├── ModbusMasterRS485.h
        │   └── ModbusMasterRS485.cpp
        └── examples/basic_demo/
```

**Konvenció:** minden új funkció önálló, újrahasznosítható modulba kerül a `lib/<ModulNév>/`
alá (`library.json` + `README.md` + `src/<Modul>.h/.cpp`). A `main.cpp` csak az összerakást
és az app-specifikus logikát tartalmazza — nem halmozódik fel benne a teljes kód.

---

## Hardver

| Elem | Típus | Megjegyzés |
|------|-------|------------|
| Kontroller | ESP32 (ESP-WROOM-32E) | — |
| Kimenetek | 32 relé/csatorna | pontos kártya típus egyeztetés alatt |

### Használt / tervezett pinek

| GPIO | Funkció | Megjegyzés |
|------|---------|------------|
| GPIO13 | RS485 TX | a `ModbusMasterRS485` modul felé |
| GPIO16 | RS485 RX | a `ModbusMasterRS485` modul felé |

**Kerülendő (ESP32):** GPIO0, GPIO2, GPIO12, GPIO15 (strap/boot) · GPIO34–39 (csak input).

---

## Építés / feltöltés

```bash
pio run            # fordítás
pio run -t upload  # feltöltés
pio device monitor # soros monitor (115200)
```

---

## Verziózás

Minden módosításkor a verziószám +0.01-et lép (formátum: `X.YY` vagy `X.YY.ZZ`).
Két azonos verziószám nem lehet a projektben.
