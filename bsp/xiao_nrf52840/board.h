/* metadata:
   name: Seeed XIAO nRF52840
   url: https://wiki.seeedstudio.com/XIAO_BLE/
*/
#ifndef BOARD_H_
#define BOARD_H_

#define _PINNUM(port, pin)    ((port)*32 + (pin))

// Red LED, active low
#define LED_PIN         _PINNUM(0, 26)
#define LED_STATE_ON    0

// No user button: an unused pin (VBAT sense divider)
#define BUTTON_PIN      _PINNUM(0, 31)
#define BUTTON_STATE_ACTIVE   0

// UART on D6/D7, clear of the Wio-SX1262
#define UART_TX_PIN     _PINNUM(1, 11)
#define UART_RX_PIN     _PINNUM(1, 12)

#endif
