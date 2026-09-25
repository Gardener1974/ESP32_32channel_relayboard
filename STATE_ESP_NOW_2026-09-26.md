# ESP-NOW Rendszer — ÁLLAPOT ÉS EMLÉKEZTETŐ

**Dátum:** 2026-09-26
**Cél:** GardenHub (sender) → 32CH relékártya (receiver) ESP-NOW vezérlés webről

---

## 🎯 HOL TARTUNK

| Elem | Állapot |
|------|---------|
| **Sender: Wi-Fi + web** | ✅ Működik (ROBOTOND router, fix IP 10.1.1.55) |
| **Sender: ESP-NOW küldés** | ✅ Működik (Verzió C: a router csatornáján küld, Wi-Fi nem kapcsol le) |
| **Levegő (rádió)** | ✅ Bizonyított (WROOM sniffer minden csomagot vett) |
| **Receiver: vétel** | ✅ Működik (csatorna-szinkron megvan) |
| **Receiver: relé kattanás** | ❌ **NEM KATTAN** — EZ A KÖVETKEZŐ FELADAT! |

---

## 🔑 KULCS-TÉNYEK (bizonyított)

- **Router (ROBOTOND) csatornája = 11** (a `WiFi.channel()` 11-et ad, de a tényleges rádió-csatorna lehet más — a szinkron 9-est is "talált" egyszer).
- **A sender MAC-je (GardenHub):** `D4:8C:49:EF:77:BC`
- **A receiver MAC-je (32CH):** `20:9B:A9:8D:0B:6C` ← a sender peer erre mutat (`RECEIVER_MAC_0..5` a sender `main.cpp`-ben)
- **A WROOM (sniffer) MAC-je:** `24:0A:C4:12:8A:A0`
- **A sender nem kap ACK-t** (`send NEM OK`), DE a csomag kimegy — az ACK nem kritikus.
- **A routert NEM szabad piszkálni** (ügyfélnél sem) — a rendszernek bármilyen router-csatornán működnie kell!

---

## ⚠️ A MEGOLDANDÓ PROBLÉMA: a relék nem kattannak

**Ami biztos:**
- A receiver FOGADJA a csomagot (webről küldve a soros kiírja: `[ESP-NOW] IMMEDIATE`, `[RELE] #1 BE`)
- A `relaySet()` LEFUT (soros kiírás megvan)
- DE a relék fizikailag NEM kattannak

**Amit tudunk:**
- A relé-vezérlés kódja (`relaySend32`, `relaySet`) **MINDEN verzióban AZONOS**:
  - Pinek: DATA=14, LATCH=12, CLOCK=13, OE=15
  - `relaySend32`: LATCH LOW → 4x `shiftOut(MSBFIRST)` → LATCH HIGH
  - `relayBegin`: pinMode OUTPUT, `digitalWrite(OE, LOW)` (aktív LOW engedélyezés), `relaySend32(0)`
  - `relaySet(ch,true)`: `g_relayState |= (1<<ch)` → kimenet HIGH
- A 6 receiver verzió (v0_02, v0_02_onallo, v0_03_onallo, v0_03_onallo_new, v0_04_onallo, v0_04_onallo_new)
  MIND ugyanezt a relé-kódot használja → **kódban nincs verziókülönbség**.

**Gyanúk (amit a következő sessionben ki kell zárni):**
1. **Aktív-HIGH vs aktív-LOW relé:** ha a relék aktív-LOW-ak, akkor `relaySend32(0)`=minden BE, és a `relaySet(true)`=KI → INVERTÁLNI kell!
2. **GPIO12 (MTDI) és GPIO15 (MTDO) STRAP pinek** — boot-kor nem ideálisak; a 74HC595 terheli őket → rossz boot-állapot / nem hajt rendesen.
3. **Relé-táp:** a relék külön 5V/12V-ot kapnak-e? (A kártya "folyamatos újraindulása" brownout jele lehet!)
4. **Driver a 74HC595 és a relék között** (ULN2003 / tranzisztor-tömb)?

**KÖVETKEZŐ LÉPÉSEK a relé-hez:**
- A) Boot-teszt firmware: sorban kapcsolja az összes relét → látni, mozdul-e bármelyik.
- B) Ha bootkor a `relaySend32(0)` hatására a relék BEMENNEK → aktív-LOW → invertálni a `relaySet`-et.
- C) Ellenőrizni a relé-tápot és a drivert a kártyán.

---

## 🛠️ A MÓDOSÍTOTT FÁJLOK (ebben a sessionben)

### Sender (`GardenHub Ultimate`):
- **`lib/EspNowSender/src/EspNowSender.cpp`** — `_sendRaw()` átírva **Verzió C-re**:
  - A router aktuális csatornáján (`WiFi.channel()`) küld, `esp_wifi_set_channel`-nel
  - A peer csatornáját is a router csatornájára állítja (`esp_now_mod_peer`)
  - **NEM kapcsolja le a Wi-Fi-t** → a web él!
  - (A korábbi Verzió A/B próbák: A=csatornaváltós, B=Wi-Fi leválasztós — a C a helyes.)
- **`lib/EspNowSender/src/EspNowSender.h`** — `_peerChannel` tag hozzáadva.
- **`src/main.cpp`** — `RECEIVER_MAC_0..5` = `0x20,0x9B,0xA9,0x8D,0x0B,0x6C` (a receiver MAC-re!).
  - FIGYELEM: egy diagnosztikai teszt idejére a WROOM MAC-re (`24:0A:C4:12:8A:A0`) volt állítva, de VISSZAÁLLÍTVA a receiverre. A senderre a tiszta (receiver MAC-es) verzió van feltöltve.

### Receiver (`ESP32_32channel_relayboard`):
- **`src/main.cpp`** — Javított, ÉLES verzió:
  - **`setup`:** `esp_wifi_set_channel(ESP_NOW_CHANNEL=11, ...)` hozzáadva (a v0.04 nem állította!)
  - **`loop`:** csatorna-szinkron állapotgép:
    - Boot után **szkennel 1..13** (200 ms/csatorna), **rálockol** az első csomagra (CHAN_LOCKED)
    - Újraszkennelés csak **60 mp csend** után (SYNC_LOST_MS)
  - Globálisok: `g_lastPktMs`, `g_chan`, `g_scanCh`, `g_mode`
  - Az `onRecv`-ben `g_lastPktMs = millis()` frissítés
  - `esp_wifi_set_max_tx_power(34)` kikommentelve (a működő v0.04-ben sem volt)
- **Backupok:** `src/main.cpp.bak_before_csatornasync`, `src/main.cpp.bak_v05_before_eles`, `src/main.cpp.bak2_20260920_0010`, `src/main.cpp.bak_20260919_2357`
- **Régi verziók:** `GardenHub Ultimate/receiver/ESP32_32Relayboard_receiver_v0_0X*.cpp`

### WROOM Sniffer (`ESP32_espnow_sniffer`):
- `src/main.cpp` — diagnosztikai szkennelő / fix-11 figyelő (csak teszt-eszköz).

---

## 📋 ESZKÖZÖK / PORTEK

- **`/dev/ttyUSB0`** = CP2104 → **RECEIVER (32CH kártya)** — időnként brownout/újraindulás; feltöltéskor néha **DOWNLOAD_BOOT** módban ragad → **USB ki/be** segít.
- **`/dev/ttyUSB1`** = CH340 → **SENDER (GardenHub / KinCony KC868-A16)**
- **A WROOM sniffer** portja változó (CP2102 volt).

**Feltöltés:**
```
cd "GardenHub Ultimate" && ~/.platformio/penv/bin/pio run -t upload --upload-port /dev/ttyUSB1   # sender
cd "ESP32_32channel_relayboard" && ~/.platformio/penv/bin/pio run -t upload --upload-port /dev/ttyUSB0   # receiver
```

---

## ⚠️ FONTOS FIGYELMEZTETÉSEK A KÖVETKEZŐ SESSIONHEZ

1. **A heredoc-os Python szerkesztésnél** a `\n` a printf stringben **valódi újsorrá** törhet → build hiba!
   Mindig ellenőrizni: `grep -n "printf"` és a fájl szintaxisa.
2. **A szerkesztő tool néha nem működik** a workspace-en kívüli fájlokon — Python heredoc a biztos megoldás.
3. **A routert SOHA nem piszkáljuk** — bármilyen router-csatornán működni kell.
4. **A receiver feltöltés után** gyakran **download módban ragad** → **USB ki/be** kell.
5. **A relé-kód a kulcs** — a vétel bizonyítottan jó.

---

## 🎯 A LEGFONTOSABB NYITOTT KÉRDÉSEK (a relé miatt)

1. Aktív-HIGH vagy aktív-LOW a relé? (bootkor a `relaySend32(0)` bekapcsolja-e őket?)
2. Kapnak-e a relék külön tápot?
3. Van-e driver (ULN2003) a 74HC595 és a relék között?
4. A boot-viselkedés: kattannak-e a relék ESP32 bootkor?

**→ A következő lépés: relé-boot-teszt firmware, és a fenti 4 kérdés tisztázása.**

---

**Készítette:** az AI asszisztens, 2026-09-26
