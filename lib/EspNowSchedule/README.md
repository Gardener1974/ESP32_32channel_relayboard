# EspNowSchedule

"Buta" ESP-NOW fogadó a 32-csatornás relékártyához. **Routerhez nem csatlakozik** —
csak a rádiót indítja (`WiFi.mode(WIFI_STA)`) és várja a KinCony sender csomagjait.

## Jelenlegi funkció (v0.01)

A MOST kért egyszerű parancs végrehajtása:

> kap egy parancsot → **felkapcsol egy adott relét X mp-re → majd lekapcsolja**

Ez a `CMD_IMMEDIATE` + `duration` mező (lásd `protocol.h`).

Támogatott parancsok a `protocol.h` szerint:
- `CMD_IMMEDIATE` (3) — azonnali végrehajtás, `duration` = X mp (0 = végleges)
- `CMD_OVERWRITE` (0) — időzítők törlése, majd az új események
- `CMD_APPEND` (1) — hozzáfűzés
- `CMD_CLEAR` (2) — összes relé KI + időzítők törlése

> Az **abszolút idejű (epoch) ütemezés egyelőre NINCS** implementálva — az későbbi
> fejlesztés, amikor a sender küldi az időt / RTC tartja.

## Csatlakozás (callback)

A relé-kiírást nem ez a modul végzi, hanem egy callback:

```cpp
EspNowSchedule sched;
sched.attachRelaySetter([&](uint8_t ch, bool on){ board.setRelay(ch, on); });
sched.begin();
// loop(): sched.loop();
```

## Fontos

- A **csatorna** (`ESP_NOW_CHANNEL`) mindkét oldalon ugyanaz kell legyen (`protocol.h`).
- ESP-NOW-hoz **nem kell** SSID/jelszó/router.
- A sender a peer-hez a **fogadó MAC-címét** használja.

## Verzió

v0.01 — 2026-09-19
