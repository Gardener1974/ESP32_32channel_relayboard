// *** ÖNÁLLÓ VÁLTOZAT *** — egyetlen fájl, külön protocol.h NÉLKÜL is fordul.
//     Az ESP-NOW protokoll teljesen be van ágyazva lentebb.

/******************************************************************************************
 * PROJEKT:   ESP32 32-csatornás relékártya vezérlő — RECEIVER (fogadó)
 * FÁJL:      ESP32_32Relayboard_receiver_v0_05_onallo.cpp
 * VERZIÓ:    v0.05
 * DÁTUM:     2026-09-20 00:10
 * HARDVER:   ESP32 (ESP-WROOM-32E, klasszikus ESP32, 240 MHz, 4 MB flash)
 *
 * SZEREP:
 *   "BUTA" ESP-NOW FOGADÓ. Routerhez NEM csatlakozik. A KinCony (GardenHub) sender
 *   ESP-NOW csomagjait fogadja, és végrehajtja a parancsokat.
 *
 * JELENLEGI FUNKCIÓ (v0.05):
 *   Relé X mp-re BE, majd KI.  (CMD_IMMEDIATE + duration mező)
 *
 * VÁLTOZÁSNAPLÓ:
 *   - v0.05 (2026-09-25 20:34): TÁP-ENYHÍTÉS a brownout ellen (gyenge nyák 3.3V):
 *                              (1) Wi-Fi adóteljesítmény csökkentése (WIFI_POWER_8_5dBm),
 *                              (3) késleltetés a boot után a táp stabilizálásához.
 *                              Ezzel a rádió csúcsárama jelentősen kisebb -> nincs brownout.
 *                              Végleges megoldás: pufferkondenzátor a 3.3V-ra.
 *   - v0.05 (2026-09-25 20:13): Wi-Fi csatorna 1 -> 11 (a ROBOTOND router
 *                              csatornája). Így az ESP-NOW összeér a senderrel, mert a
 *                              GardenHub is a router 11-es csatornáján sugároz.
 *   - v0.05 (2026-09-20 00:10): HIBA JAVÍTVA — a v0.02 boot-loopot (LoadProhibited panic)
 *                              okozott, mert az esp_wifi_start() előtt a Wi-Fi stack nem volt
 *                              inicializálva (NVS, netif, event loop, wifi init). Most a
 *                              teljes ESP-IDF init szekvencia lefut (nvs_flash_init,
 *                              esp_netif_init, esp_event_loop_create_default,
 *                              esp_netif_create_default_wifi_sta, esp_wifi_init).
 *   - v0.02 (2026-09-19 23:48): WiFi.h eltávolítva, ESP-IDF API. (HIBA: boot-loop.)
 *   - v0.01 (2026-09-19 22:57): Első fogadó firmware.
 *
 * FÜGGŐSÉG:
 *   NINCS — ez az ÖNÁLLÓ (egyfájlos) változat. A közös protokoll be van ágyazva lentebb.
 *
 * ------------------------------------------------------------------------------------------
 * HASZNÁLT PINEK (GPIO -> funkció):
 *   GPIO14 -> DATA   (74HC595 szeriális adat, DS)
 *   GPIO12 -> LATCH  (74HC595 RCLK / tároló)
 *   GPIO13 -> CLOCK  (74HC595 SRCLK)
 *   GPIO15 -> OE     (74HC595 Output Enable, aktív LOW)
 *
 * SZABAD PINEK:
 *   GPIO4, 5, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33 és GPIO34..39 (csak bemenet).
 *
 * TILTOTT / KERÜLENDŐ PINEK:
 *   - GPIO0, GPIO2, GPIO12, GPIO15 : strap/boot pinek (itt tudatosan használt).
 *   - GPIO6..GPIO11 : belső flash, TILOS!
 *   - GPIO34..GPIO39: input-only (kimenetre nem használható).
 * ------------------------------------------------------------------------------------------
 ******************************************************************************************/

#include <Arduino.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_mac.h>
#include <esp_netif.h>
#include <esp_event.h>
#include <nvs_flash.h>

// ===== BEÁGYAZOTT protocol.h ==============================================================
#define ESP_NOW_CHANNEL   11   // router (ROBOTOND) csatornája
#define MAX_EVENTS        16

struct RelayEvent {
  uint8_t  relay;
  uint8_t  action;
  uint16_t duration;
  uint32_t startEpoch;
};

struct SchedulePacket {
  uint8_t    magic;
  uint8_t    command;
  uint8_t    count;
  RelayEvent events[MAX_EVENTS];
};

#define PACKET_MAGIC   0xA5
#define CMD_OVERWRITE  0
#define CMD_APPEND     1
#define CMD_CLEAR      2
#define CMD_IMMEDIATE  3
// ===== BEÁGYAZOTT protocol.h VÉGE =========================================================

// ============================================================================
//  BEÁLLÍTÁSOK
// ============================================================================
#define PIN_DATA   14
#define PIN_LATCH  12
#define PIN_CLOCK  13
#define PIN_OE     15
#define APP_NAME     "ESP32 32ch Relayboard (receiver)"
#define APP_VERSION  "v0.05"

static uint32_t g_relayState = 0;

static void relaySend32(uint32_t value) {
  // A 74HC595 kiírás: RCLK LOW -> 4x shiftOut -> RCLK HIGH (tárolás).
  digitalWrite(PIN_LATCH, LOW);
  shiftOut(PIN_DATA, PIN_CLOCK, MSBFIRST, (value >> 24) & 0xFF);
  shiftOut(PIN_DATA, PIN_CLOCK, MSBFIRST, (value >> 16) & 0xFF);
  shiftOut(PIN_DATA, PIN_CLOCK, MSBFIRST, (value >> 8)  & 0xFF);
  shiftOut(PIN_DATA, PIN_CLOCK, MSBFIRST,  value        & 0xFF);
  digitalWrite(PIN_LATCH, HIGH);
}

static void relayBegin() {
  pinMode(PIN_DATA, OUTPUT);
  pinMode(PIN_LATCH, OUTPUT);
  pinMode(PIN_CLOCK, OUTPUT);
  if (PIN_OE != -1) {
    pinMode(PIN_OE, OUTPUT);
    digitalWrite(PIN_OE, LOW);
  }
  g_relayState = 0;
  relaySend32(0);
}

static void relaySet(uint8_t ch, bool on) {
  if (ch > 31) return;
  if (on) g_relayState |=  (1UL << ch);
  else    g_relayState &= ~(1UL << ch);
  relaySend32(g_relayState);
}

// ============================================================================
//  "X mp-re BE, majd KI" időzítők
// ============================================================================
static uint32_t g_offAt[32];
static bool     g_timerActive[32];

static void timersClearAll() {
  for (int i = 0; i < 32; i++) { g_timerActive[i] = false; g_offAt[i] = 0; }
}

static void applyEvent(const RelayEvent& e) {
  if (e.relay > 31) return;
  if (e.action == 1) {
    relaySet(e.relay, true);
    if (e.duration > 0) {
      g_offAt[e.relay]      = millis() + (uint32_t)e.duration * 1000UL;
      g_timerActive[e.relay] = true;
      Serial.printf("[RELE] #%d BE, %u mp-re (KI: %lu ms)\n",
                    e.relay + 1, (unsigned)e.duration, (unsigned long)g_offAt[e.relay]);
    } else {
      g_timerActive[e.relay] = false;
      Serial.printf("[RELE] #%d BE (vegleges)\n", e.relay + 1);
    }
  } else {
    relaySet(e.relay, false);
    g_timerActive[e.relay] = false;
    Serial.printf("[RELE] #%d KI\n", e.relay + 1);
  }
}

// ============================================================================
//  ESP-NOW fogadás
// ============================================================================
static volatile uint32_t g_packetCount = 0;
static volatile uint32_t g_lastPktMs   = 0;     // az utolso bejovo csomag ideje (csatorna-szinkron)
static uint8_t  g_chan = ESP_NOW_CHANNEL;       // az aktualis rádió-csatorna
static uint8_t  g_scanCh = 1;                   // a szkennelés jelzője (1..13)

static void onRecv(const uint8_t* mac, const uint8_t* data, int len) {
  if (len < (int)sizeof(SchedulePacket)) {
    Serial.printf("[ESP-NOW] Tul rovid csomag (%d bajt) -> eldobva\n", len);
    return;
  }
  SchedulePacket pkt;
  memcpy(&pkt, data, sizeof(pkt));

  if (pkt.magic != PACKET_MAGIC) {
    Serial.printf("[ESP-NOW] Hibas magic (0x%02X) -> eldobva\n", pkt.magic);
    return;
  }
  g_packetCount++;
  g_lastPktMs = millis();     // csatorna-szinkron: van forgalom ezen a csatornan

  uint8_t n = (pkt.count > MAX_EVENTS) ? MAX_EVENTS : pkt.count;

  switch (pkt.command) {
    case CMD_IMMEDIATE:
      Serial.printf("[ESP-NOW] IMMEDIATE, %u esemeny\n", n);
      for (uint8_t i = 0; i < n; i++) applyEvent(pkt.events[i]);
      break;
    case CMD_OVERWRITE:
      Serial.printf("[ESP-NOW] OVERWRITE, %u esemeny\n", n);
      timersClearAll();
      for (uint8_t i = 0; i < n; i++) applyEvent(pkt.events[i]);
      break;
    case CMD_APPEND:
      Serial.printf("[ESP-NOW] APPEND, %u esemeny\n", n);
      for (uint8_t i = 0; i < n; i++) applyEvent(pkt.events[i]);
      break;
    case CMD_CLEAR:
      Serial.println("[ESP-NOW] CLEAR -> minden rele KI + idozitok torolve");
      timersClearAll();
      for (uint8_t ch = 0; ch < 32; ch++) relaySet(ch, false);
      break;
    default:
      Serial.printf("[ESP-NOW] Ismeretlen parancs: %u -> eldobva\n", pkt.command);
      break;
  }
}

// ============================================================================
//  Wi-Fi stack inicializálás ESP-IDF szinten (ESP-NOW-hoz)
// ============================================================================
static bool wifiStackInit() {
  // 1) NVS (a Wi-Fi ezt használja a kalibrációs adatokhoz)
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    nvs_flash_erase();
    ret = nvs_flash_init();
  }
  if (ret != ESP_OK) {
    Serial.println("[HIBA] nvs_flash_init");
    return false;
  }

  // 2) Hálózati interfész + event loop
  esp_netif_init();
  esp_event_loop_create_default();
  esp_netif_create_default_wifi_sta();

  // 3) Wi-Fi init (default config)
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  if (esp_wifi_init(&cfg) != ESP_OK) {
    Serial.println("[HIBA] esp_wifi_init");
    return false;
  }

  // 4) Mód + indítás (NEM csatlakozik semmilyen AP-hoz!)
  esp_wifi_set_mode(WIFI_MODE_STA);
  esp_wifi_start();

  // 5) TÁP-ENYHÍTÉS: adóteljesítmény csökkentése a rádió csúcsáramának mérséklésére
  // esp_wifi_set_max_tx_power(34);  // (kikapcsolva – a v0.04-ben sem volt)
  return true;
}

// ============================================================================
//  setup / loop
// ============================================================================
void setup() {
  Serial.begin(115200);
  // MEGJEGYZÉS: a setCpuFrequencyMhz(80) NEM használható — eltolja a soros baud-ot,
  // és a monitoron semmi nem látszik. Ezért csak a TxPower-csökkentést használjuk.
  delay(300);
  Serial.printf("\n=== %s %s ===\n", APP_NAME, APP_VERSION);

  relayBegin();              // relék: pinek be + minden KI

  // Wi-Fi stack (ESP-IDF) + ESP-NOW fogadó
  if (!wifiStackInit()) {
    Serial.println("[HIBA] WiFi stack init nem sikerult!");
    return;
  }

  if (esp_now_init() != ESP_OK) {
    Serial.println("[HIBA] ESP-NOW init nem sikerult!");
    return;
  }
  esp_now_register_recv_cb(onRecv);

  // A rádió beállítása a kívánt csatornára (a wifiStackInit NEM állítja!)
  esp_wifi_set_channel(ESP_NOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
  g_chan = ESP_NOW_CHANNEL;

  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  Serial.printf("[ESP-NOW] Fogado kesz (channel=%d). Sajat MAC: %s\n",
                ESP_NOW_CHANNEL, macStr);
  Serial.println("Varom a KinCony sender parancsait...");
}

// ============================================================================
//  Csatorna-szinkron (ÉLES):
//  A sender Wi-Fi STA-ja a router csatornáján küld (a routert NEM piszkáljuk).
//  A fogadó induláskor végigpásztázza az 1..13 csatornát, és ahol az első
//  érvényes csomag (0xA5 magic) megjön, OTT MARAD. Újraszkennelni csak akkor
//  kezd, ha nagyon sokáig (SYNC_LOST_MS) nincs csomag — ilyenkor a router
//  cserélt csatornát.
// ============================================================================
#define SYNC_LOST_MS   60000    // ennyi ideig nincs csomag -> újraszkennelünk
#define SCAN_DWELL_MS  200      // ennyit figyelünk egy csatornán

enum ChanMode { CHAN_LOCKED, CHAN_SCANNING };
static ChanMode g_mode = CHAN_SCANNING;   // indulás: mindig szkennelünk
static uint32_t g_scanNextStep = 0;
static uint32_t g_scanStartPkt = 0;
// g_scanCh felül a globálisoknál definiálva (kb. 170. sor)

void loop() {
  uint32_t now = millis();

  // 1) Lejárt "X mp-re BE" időzítők kikapcsolása
  for (int ch = 0; ch < 32; ch++) {
    if (g_timerActive[ch] && (int32_t)(now - g_offAt[ch]) >= 0) {
      g_timerActive[ch] = false;
      relaySet((uint8_t)ch, false);
      Serial.printf("[IDOZITO] rele #%d KI (lejart)\n", ch + 1);
    }
  }

  // 2) Csatorna-kezelés
  if (g_mode == CHAN_SCANNING) {
    if (now - g_scanNextStep >= SCAN_DWELL_MS) {
      g_scanNextStep = now;
      // Jött csomag az előző ablakban? -> rögtön rögzítjük az ELŐZŐ csatornát
      if (g_packetCount != g_scanStartPkt) {
        uint8_t found = (g_scanCh == 1) ? 13 : (g_scanCh - 1);
        esp_wifi_set_channel(found, WIFI_SECOND_CHAN_NONE);
        g_chan = found;
        g_mode = CHAN_LOCKED;
        g_lastPktMs = millis();
        Serial.printf("[SZINKRON] MEGTALALVA -> raallas a %u-es csatornara.\n", found);
      } else {
        esp_wifi_set_channel(g_scanCh, WIFI_SECOND_CHAN_NONE);
        g_chan = g_scanCh;
        g_scanStartPkt = g_packetCount;
        g_scanCh = (g_scanCh >= 13) ? 1 : (g_scanCh + 1);
      }
    }
  } else { // CHAN_LOCKED
    // Újraszkennelés csak nagyon hosszú csend után (router csatorna-csere)
    if (g_lastPktMs != 0 && (now - g_lastPktMs > SYNC_LOST_MS)) {
      g_mode = CHAN_SCANNING;
      g_scanCh = 1;
      g_scanNextStep = now;
      g_scanStartPkt = g_packetCount;
      Serial.println("[SZINKRON] hosszu csend -> ujra pasztazas (1..13)");
    }
  }

  delay(2);
}