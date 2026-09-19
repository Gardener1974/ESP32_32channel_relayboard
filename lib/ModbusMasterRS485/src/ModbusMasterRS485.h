/*
 * ModbusMasterRS485.h
 *
 * Újrahasznosítható Modbus RTU (RS485) MASTER modul.
 *
 * Típusos cél: külső funkció-06-os vezérlésű relékártya vezérlése
 * (pl. olcsó 16-csatornás "kínai" RS485 reléboard).
 *
 * Konfigurálható port/GPIO — ezért bármely ESP32/Arduino projektbe áttehető.
 */
#ifndef MODBUS_MASTER_RS485_H
#define MODBUS_MASTER_RS485_H

#include <Arduino.h>

class ModbusMasterRS485 {
public:
    // Hibakódok az utolsó művelethez
    enum ErrorCode : uint8_t {
        OK          = 0,
        ERR_CHANNEL = 1,   // érvénytelen csatorna (nem 1..16)
        ERR_SLAVE   = 2,   // érvénytelen slave-cím (0)
        ERR_INVALID_CMD = 3,
        ERR_TIMEOUT = 4
    };

    // Alapértelmezett GPIO-k (KinCony KC868-A16 / RS485)
    static const uint8_t DEFAULT_RX = 16;
    static const uint8_t DEFAULT_TX = 13;

    // ---- Konstruktor ----
    // rxPin, txPin: az RS485 transceiverhez kötött RX és TX GPIO.
    // uartNum: az ESP32 HardwareSerial port száma (1=görbített UART1, 2=UART2 -- ajánlott).
    explicit ModbusMasterRS485(uint8_t rxPin = DEFAULT_RX,
                                uint8_t txPin = DEFAULT_TX,
                                uint8_t uartNum = 2);

    // Inicializálás: slaveId = a vezérelt kártya Modbus címe (pl. 0x01).
    void begin(uint8_t slaveId, uint32_t baud = 9600);

    // ---- Konfiguráció később is ----
    void setSlaveId(uint8_t slaveId);
    void setBaud(uint32_t baud);
    // Mennyit várjon két egymást követő Modbus-küldés között (ms), hogy a slave
    // feldolgozza az előzőt és a félduplex busz "forduljon" (turnaround).
    // A gyári példák enélkül "csend"-ben lőnek, de hosszú sorozatoknál (all-on)
    // a túl rövid idő miatt keretek eshetnek ki. Alap 40 ms.
    void setTurnaroundMs(uint16_t ms) { _sendGapMs = ms; }

    // ===== Relé (csatorna) műveletek =====
    // ch: 1..16. true = siker, false = hiba (nézd meg getError()).
    bool relayOpen    (uint16_t ch);              // 0x0001 BE
    bool relayClose   (uint16_t ch);              // 0x0002 KI
    bool relayToggle  (uint16_t ch);              // 0x0003 átkapcsolás
    bool relayLatch   (uint16_t ch);              // 0x0004 inter-locking
    bool relayMomentary(uint16_t ch);             // 0x0005 nem-záró
    bool relayDelay   (uint16_t ch, uint8_t dly); // 0x0006 + késleltetés

    // 16-csatornás verzióban: összes relé
    bool relayOpenAll ();                          // addr 0x0000, data 0x0007
    bool relayCloseAll();                          // addr 0x0000, data 0x0008

    // Visszajelzés
    bool     lastOk() const     { return _lastOk; }
    uint8_t  getError() const   { return _err; }
    uint32_t getTxCount() const { return _txCount; }

private:
    // A modbus funkció-06-os keret elküldése
    // addr = csatorna (0..N), data = parancs (0x0001..0x0008), és adott esetben kieg.
    bool _send06(uint16_t addr, uint16_t data);

    void _resetErr() { _err = OK; _lastOk = false; }
    void _setErr(uint8_t e) { _err = e; _lastOk = false; }

    // Modbus CRC16 (A001 polinom, hagyományos RTU)
    uint16_t _crc16(const uint8_t* data, uint16_t len) const;

    HardwareSerial* _ser;
    uint8_t  _rxPin;
    uint8_t  _txPin;
    uint8_t  _uartNum;
    uint8_t  _slaveId;
    uint32_t _baud;
    uint16_t _sendGapMs = 40;        // gap két küldés között (félduplex turnaround)

    bool        _started = false;
    bool        _lastOk  = false;
    uint8_t     _err     = OK;
    uint32_t    _txCount = 0;
};

#endif // MODBUS_MASTER_RS485_H
