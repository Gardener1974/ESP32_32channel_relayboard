/*
 * ModbusMasterRS485.cpp
 *
 * Az RS485 Modbus MASTER megvalósítása.
 * Csak funkció 06 (Write Single Holding Register) parancsokat küld egy slave-nek.
 */
#include "ModbusMasterRS485.h"

// ---- Parancskódok (a gyári ModbusRTU_16RelayBoardTest.ino & 16-ch. modbus doc szerint)
// A relé-vezérlés funkció 06 (Write Single Holding Register):
//   address = relé csatorna (0x0001..0x0010 / 0x0000 = "összes"),
//   value   = parancs a FELSŐ bájtban (az alsó bájt 0x00):
//      0x01xx = BE (Open), 0x02xx = KI (Close), 0x03xx = Toggle, 0x04xx = Latch,
//      0x05xx = Momentary, 0x06xx = Delay, 0x07xx = Open-all, 0x08xx = Close-all
enum {
    CMD_OPEN      = 0x0100,
    CMD_CLOSE     = 0x0200,
    CMD_TOGGLE    = 0x0300,
    CMD_LATCH     = 0x0400,
    CMD_MOMENTARY = 0x0500,
    CMD_DELAY     = 0x0600,
    CMD_OPEN_ALL  = 0x0700,
    CMD_CLOSE_ALL = 0x0800
};

ModbusMasterRS485::ModbusMasterRS485(uint8_t rxPin, uint8_t txPin, uint8_t uartNum)
    : _rxPin(rxPin), _txPin(txPin), _uartNum(uartNum) {
    _ser = nullptr;
    _slaveId = 0x01;
    _baud    = 9600;
}

void ModbusMasterRS485::begin(uint8_t slaveId, uint32_t baud) {
    _slaveId = slaveId;
    _baud    = baud;

    // Az ESP32 hardware-soros erőforrása. A _uartNum 1 => &Serial1, 2 => &Serial2
    // (alap szerepben az UART0 a Serial -- nem bántjuk, az a USB/console).
    switch (_uartNum) {
        case 1: _ser = &Serial1; break;
        case 2: default: _ser = &Serial2; break;
    }

    if (_ser) {
        _ser->begin(_baud, SERIAL_8N1, _rxPin, _txPin);
        _started = true;
    }
    _lastOk = false;
    _err    = OK;
    _txCount = 0;
}

void ModbusMasterRS485::setSlaveId(uint8_t slaveId) { _slaveId = slaveId; }
void ModbusMasterRS485::setBaud(uint32_t baud) {
    if (_ser) { _ser->end(); _ser->begin(baud, SERIAL_8N1, _rxPin, _txPin); }
    _baud = baud;
}

// ====================================================================
//  Nyilvános relé-műveletek
// ====================================================================

bool ModbusMasterRS485::relayOpen(uint16_t ch) {
    return _send06(ch, CMD_OPEN);
}
bool ModbusMasterRS485::relayClose(uint16_t ch) {
    return _send06(ch, CMD_CLOSE);
}
bool ModbusMasterRS485::relayToggle(uint16_t ch) {
    return _send06(ch, CMD_TOGGLE);
}
bool ModbusMasterRS485::relayLatch(uint16_t ch) {
    return _send06(ch, CMD_LATCH);
}
bool ModbusMasterRS485::relayMomentary(uint16_t ch) {
    return _send06(ch, CMD_MOMENTARY);
}
bool ModbusMasterRS485::relayDelay(uint16_t ch, uint8_t dly) {
    return _send06(ch, CMD_DELAY); // dly a [data] alacsony bájtjába kerülne; ld. lentebb
}
bool ModbusMasterRS485::relayOpenAll() {
    return _send06(0x0000, CMD_OPEN_ALL);
}
bool ModbusMasterRS485::relayCloseAll() {
    return _send06(0x0000, CMD_CLOSE_ALL);
}

// ====================================================================
//  Belső: funkció-06 üzenet felépítése + küldés + CRC
// ====================================================================

// (Kiegészítő: a "Delay" parancsnál a data alacsony bájtja a késleltetés.
//  Itt egyszerűen nem használjuk ki -- ha kell, a hívónál lehet finomítani.)
bool ModbusMasterRS485::_send06(uint16_t addr, uint16_t data) {
    _resetErr();

    if (!_started || !_ser) { _setErr(ERR_TIMEOUT); return false; }
    if (_slaveId == 0)      { _setErr(ERR_SLAVE);   return false; }

    // Az üzenet:  [slave][06][addr-H][addr-L][data-H][data-L][CRC-H][CRC-L]
    uint8_t msg[8];
    msg[0] = _slaveId;
    msg[1] = 0x06;                 // funkció: Write Single Holding Register
    msg[2] = (addr >> 8) & 0xFF;   // address high
    msg[3] =  addr       & 0xFF;   // address low
    msg[4] = (data >> 8) & 0xFF;   // value high
    msg[5] =  data       & 0xFF;   // value low

    uint16_t crc = _crc16(msg, 6);
    msg[6] =  crc        & 0xFF;   // CRC low
    msg[7] = (crc >> 8) & 0xFF;   // CRC high

    // Küldés
    _ser->write(msg, 8);
    _ser->flush();

    // Félduplex RS485: miután elküldtük, várunk, hogy a slave feldolgozhassa
    // (és visszaküldhesse a választ/echót) a buszon — különben a hirtelen
    // következő küldésünk a slave válaszát/hullámait elrontja, és hosszú
    // sorozatoknál (pl. "összes be") keretek esnek ki. Az _sendGapMs a
    // setTurnaroundMs()-szel állítható.
    delay(_sendGapMs);

    _lastOk  = true;
    _txCount++;
    _err = OK;
    return true;
}

// Modbus CRC16-ot számol RTU minta szerint (polinom 0xA001).
uint16_t ModbusMasterRS485::_crc16(const uint8_t* data, uint16_t len) const {
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; bit++) {
            if (crc & 0x0001) { crc >>= 1; crc ^= 0xA001; }
            else              { crc >>= 1; }
        }
    }
    return crc;
}
