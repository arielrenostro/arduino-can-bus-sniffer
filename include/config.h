#pragma once

// Configuração central do sniffer. Troque aqui o que depende do hardware.

// --- Controlador CAN (MCP2515) ---
// Cristal do módulo: marcação "EAS8000" assumida como 8,000 MHz.
// Se nenhum quadro aparecer, tente MCP_16MHZ.
#define CFG_CAN_CLOCK MCP_8MHZ
#define CFG_CAN_CLOCK_MHZ 8
// HS.IS do Peugeot 206 (AEE2001): 250 kbit/s, conforme architectures.yml do PSA-RE.
// (Outras arquiteturas PSA usam 500 kbit/s no HS.)
#define CFG_CAN_SPEED CAN_250KBPS
#define CFG_CAN_SPEED_KBPS 250

// --- Pinos (Arduino Nano) ---
// SI=D11 (MOSI), SO=D12 (MISO), SCK=D13 são os pinos SPI padrão.
#define CFG_PIN_CS 10
#define CFG_PIN_INT 3

// --- Serial ---
// 115200 não comporta o tráfego do HS.IS em texto. Se o adaptador USB-serial
// não aceitar 500000, use 250000 (e ajuste monitor_speed no platformio.ini).
#define CFG_SERIAL_BAUD 500000UL

// --- Capacidades ---
#define CFG_RING_CAPACITY 32   // quadros no ring buffer (potência de 2)
#define CFG_FILTER_CAPACITY 16 // IDs na lista de filtro
