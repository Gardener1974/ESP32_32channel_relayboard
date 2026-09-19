# ModbusMasterRS485

Egy tiszta, **újrahasznosítható** Modbus RTU (RS485) **master** modul Arduino/ESP32 környezetre.

A modul képes egy **funkció 06 -os vezérléssel rendelkező**, külső Modbus-slave kapcsoló
kártya vezérlésére — tipikusan olyan 16/32 csatornás, olcsó "kínai" relékártyákra,
amelyek **nem** szabványos (01/05/0F/10) coil- vagy holding-regiszter írást használnak,
hanem egy egyszerűbb, parancs-orientált protokollt, ahol a Modbus *data* mező hordozza a
műveletet (Open=0x0001, Close=0x0002, Toggle=0x0003 stb.).

## Nagy előnye a cél-felhasználásra

A KinCony KC868-A16-os projekt (GardenHub Ultimate) **belső 16 MOSFET kimenete** I2C-s
expanderrel (GPIO4/5) vezérelt és **kész**. Ez a modul **az ESP32 szürke területét** nem
használja (nem nyúl a GPIO4/5-höz), hanem **kizárólag az RS485 vonalhoz** tartozó
GPIO13 (TX) és GPIO16 (RX) porton dolgozik. Így **kockázat nélkül** illeszthető a
meglévő, működő rendszer mellé.

---

## Ki a SLAVE (a vezérelt eszköz)?

A kártya amit ez a modul master-ként vezérel:
- **Slave ID**: DIP kapcsolóval (A0–A5) beállítható, példa: `0x01`
- **Egyen: 9600 baud, 8 data, No parity, 1 stop (9600 8N1)**
- **Protokoll**: Modbus funkció **0x06** (Write Single Holding Register),
  a *register (address)* mezőben a **csatorna**, a *value (data)* mezőben a **parancs**
  található. **Fontos:** ezeknél a kártyáknál a parancs a data-érték **felső bájtjában**
  szerepel (a gyári `ModbusRTU_16RelayBoardTest.ino` szerint):

| Hex value (data) | Jelentés |
|------------------|----------|
| `0x0100` | **Open** (relé BE) |
| `0x0200` | **Close** (relé KI) |
| `0x0300` | **Toggle** (átkapcsolás / önzáró) |
| `0x0400` | **Latch** (inter-locking) |
| `0x0500` | **Momentary** (nem-záró) |
| `0x0600` | **Delay** |
| `0x0700` (addr `0x0000`) | **Open all** (összes BE, csak 16-ch. verzió) |
| `0x0800` (addr `0x0000`) | **Close all** (összes KI, csak 16-ch. verzió) |

A **modbus register (address) mező** a csatorna (channel) szám:
- `0x0001`..`0x0010` = 1..16 relé (relé mellett: a **relé 1 regisztere `0x0001`**),
- `0x0000` = "összes" parancsokhoz.

---

## Hardware bekötés (fizikai)

```
Kulsó relé-kártya  RS485 A+  <-->  KinCony RS485 A
Kulsó relé-kártya  RS485 B-  <-->  KinCony RS485 B
(Közös GND a két kártya között erősen ajánlott!)
```

A KinCony KC868-A16 **RS485 csatlakozóján csak A és B van** (auto-direction). Az
**irányváltást (DE/RE) a kártya hardvere magától elvégzi**, ezért a szoftverben nem kell
külön DE/RE GPIO-val bajlódni. Elég a **hardveres soros port** használata.

---

## GPIO konfiguráció

Alapértelmezés szerint a modul erre az ESP32 HardwareSerial portra ül:

| GPIO | Funkció |
|------|---------|
| `RS485_TX_GPIO` (alap: **13**) | adó (a kártya MS2548 `DI`-je) |
| `RS485_RX_GPIO` (alap: **16**) | vevő (a kártya MS2548 `RO`-ja) |

A port, baud és GPIO-k a konstruktorral / setterekkel felülírhatók, úgyhogy a modul
**más projektekben is használható más porton/GPIO-n** (pl. UART1/2).

---

## Használat (példa)

```cpp
#include "ModbusMasterRS485.h"

// --- Konstruktor: (RX GPIO, TX GPIO, UART port szám) ---
ModbusMasterRS485 rs485(16, 13, 2);   // RX=16, TX=13, UART2

void setup() {
  rs485.begin(0x01);   // slave-cím = 0x01, alapértelmezett 9600 8N1
}

void loop() {
  rs485.relayOn(1);     // relé #1 BE
  delay(500);
  rs485.relayOff(1);    // relé #1 KI
  delay(500);
  rs485.relayToggle(3); // relé #3 átkapcsolás
  delay(500);
}
```

---

## Publikus API

### Konstruktor & konfig
```cpp
ModbusMasterRS485(uint8_t rxPin, uint8_t txPin, uint8_t uartNum = 2);
void begin(uint8_t slaveId, uint32_t baud = 9600);
void setSlaveId(uint8_t slaveId);
void setSerialConfig(uint32_t data /* =SERIAL_8N1 */);
```

### Családi relé műveletek (hivatalos, "parancs-orientált")
```cpp
bool relayOpen    (uint16_t ch);   // BE
bool relayClose   (uint16_t ch);   // KI
bool relayToggle  (uint16_t ch);   // átkapcsolás (self-locking)
bool relayLatch   (uint16_t ch);   // inter-locking
bool relayMomentary(uint16_t ch);  // nem-záró
bool relayDelay   (uint16_t ch, uint8_t delay); // késleltetett
bool relayOpenAll ();              // összes BE (16-ch.)
bool relayCloseAll();              // összes KI (16-ch.)
```

### Működés visszajelzése
```cpp
bool     lastOk();          // az utolsó küldés sikeres volt-e
uint8_t  getError();        // utolsó hiba (lásd ErrorCode enum)
uint32_t getTxCount();      // eddigi sikeres TX üzenetek száma
```

Belsőleg minden `relay*` metódus a saját, `_send06()/*függvény*/` magán utasítást végzi,
kiszámolja a Modbus **CRC16**, és a megfelelő funkció 06 keretet küldi a soros porton.

---

## Hibatípusok (`ErrorCode`)

| Név | Érték | Jelentés |
|-----|-------|----------|
| `OK` | 0 | sikeres küldés |
| `ERR_CHANNEL` | 1 | érvénytelen csatornaszám (nem 1..16) |
| `ERR_SLAVE` | 2 | érvénytelen slave-cím |
| `ERR_INVALID_CMD` | 3 | érvénytelen parancskód |

---

## Moduláris / újrahasznosítható fejlesztés

Ezt a modul önálló könyvtárként (`lib/ModbusMasterRS485/`) lett létrehozva, hogy:
- **más projektekbe** is átmásolható / `lib_deps`-ként hivatkozható legyen,
- a meglévő `main.cpp` **ne** nőjön tovább egy monolitikus kódtömbbé,
- a későbbi input/display/billentyűzet modulokkal együtt konzisztens, újrahasznosítható
  belső ökoszisztémát alkosson.

Lásd a projekt gyökerén található **README.md** "Modul-konvenció" szakaszát.
