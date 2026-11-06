// Zeptomech globals (included from globals.h)
#ifndef ZEPTOMECH_GLOBALS_H
#define ZEPTOMECH_GLOBALS_H 1

// true while the clock jack receives pulses; midi clock is ignored meanwhile
volatile bool clock_jack_active = false;

// hardware TRS midi
#define UART_ID uart1
#define UART_BAUD 31250
PIO pio_rx = pio2;
uint sm_rx = 1;

#endif
