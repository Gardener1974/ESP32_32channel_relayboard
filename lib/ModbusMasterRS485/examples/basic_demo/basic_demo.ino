/*
 * Példa: ModbusMasterRS485 modul önálló használata
 *
 * Demonstrálja a külső funkció-06-os RS485 relékártya vezérlését master-ként.
 * Itt nincs webszerver, csak a modul önmagában -- hogy bármely projektbe
 * beemelhető legyen ez az alap.
 *
 * Bekötés (KinCony KC868-A16):
 *   RS485 TX -> GPIO13, RS485 RX -> GPIO16
 *
 * A vezérelt kártya: slave ID=0x01, 9600 8N1
 */
#include <Arduino.h>
#include "ModbusMasterRS485.h"

// (rx, tx, uart) -- KinCony RS485: RX=GPIO16, TX=GPIO13, UART2
ModbusMasterRS485 rs485(16, 13, 2);

const uint8_t SLAVE = 0x01;

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println("\nRS485 Modbus master demo (funkció 06)");

    rs485.begin(SLAVE, 9600);
    Serial.println("Master inicializálva. slave=0x01, 9600 8N1");
}

void loop() {
    // --- Mindegyik relé kiindulásként KI ---
    rs485.relayCloseAll();
    delay(800);

    // --- Relé #1 BE 1 másodpercre, majd KI ---
    rs485.relayOpen(1);
    delay(1000);
    rs485.relayClose(1);
    delay(800);

    // --- Relé #2 kapcsolgatás (toggle) ---
    rs485.relayToggle(2);
    delay(1000);
    rs485.relayToggle(2);
    delay(500);

    // --- Mindegyik BE (16-csatornás verzió) ---
    rs485.relayOpenAll();
    delay(1200);

    Serial.printf("OK=%d TX_cel=%lu\n", rs485.lastOk() ? 1 : 0,
                  (unsigned long)rs485.getTxCount());

    delay(3000);
}
