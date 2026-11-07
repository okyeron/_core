// Zeptomech parts of main.c (included from it)
#ifndef ZEPTOMECH_MAIN_H
#define ZEPTOMECH_MAIN_H 1

#ifdef SD_BOOT_CACHE
#ifdef SIMULATOR
// the simulator has its own disk layer
static inline void sd_boot_cache_enable(bool on) { (void)on; }
#else
// lib/sd_boot_cache.h, built into lib/sdio/src/glue.c
void sd_boot_cache_enable(bool on);
#endif
#endif

// zeptomech shows the zeptocore led states on its ws2812 strip
// (called from LEDS_render)
void LEDS_render_forward_zeptomech(LEDS *leds) {
#ifdef INCLUDE_RGBLED
  // sdcard_startup() shows LED text before main() creates the strip
  if (ws2812 == NULL) {
    return;
  }
  int k = 0;
  for (uint8_t i = 0; i < LEDS_ROWS; i++) {
    for (uint8_t j = 0; j < LEDS_COLS; j++) {
      // LED_NONE / LED_DIM / LED_BRIGHT scale directly
      int val = leds->state[i][j] * 75;

      // blink leds
      int blinkval = 3 * 75;
      blink_time = 70;
      if (leds->state[i][j] == LED_BLINK) {
        leds->rgb_leds_counter[k]++;
        if (leds->rgb_leds_counter[k] == blink_time) {
          // set led
          WS2812_fill(ws2812, k, blinkval, blinkval, blinkval);
        } else if (leds->rgb_leds_counter[k] >= blink_time * 2) {
          // clear led
          WS2812_fill(ws2812, k, 0, 0, 0);
          leds->rgb_leds_counter[k] = 0;
        }
      } else {
        WS2812_fill(ws2812, k, val, val, val);
      }
      k++;
    }
  }
  WS2812_show(ws2812);
#endif
}

// the ws2812 strip, called from main() before sdcard_startup() so the startup
// text ("M", "NO SDCARD") shows on it
void zeptomech_leds_setup() {
#ifdef INCLUDE_RGBLED
  ws2812 = WS2812_new(NEOPIXPIN, pio0, 2);
  WS2812_set_brightness(ws2812, global_brightness);
  sleep_ms(1);
  WS2812_fill(ws2812, 0, 0, 0, 0);
  sleep_ms(1);
  WS2812_show(ws2812);
  sleep_ms(1);
  LEDS_clear(leds);
#endif
}

// zeptomech hardware, called from main() before the leds and audio start
void zeptomech_setup() {
  is_zeptomech = true;
  // the ADS7830 pots are the only i2c device on zeptomech; main() sets up the
  // bus at 50 kHz, where reading all 8 pots took ~6.7 ms of every main loop
  i2c_set_baudrate(i2c_default, 400 * 1000);
  // midi comes in on the TRS jack
  use_onewiremidi = true;

  // red led on
  gpio_init(REDLED);
  gpio_set_dir(REDLED, GPIO_OUT);
  gpio_put(REDLED, true);
  // 5v enable
  gpio_init(FIVEVENABLE);
  gpio_set_dir(FIVEVENABLE, GPIO_OUT);
  gpio_put(FIVEVENABLE, true);

  // hardware TRS midi in (uart1); onewiremidi3 reads it
  uart_init(UART_ID, UART_BAUD);
  gpio_set_function(UART1_RX, UART_FUNCSEL_NUM(UART_ID, UART1_RX));
  uart_set_hw_flow(UART_ID, false, false);
  uart_set_format(UART_ID, 8, 1, UART_PARITY_NONE);

  // hardware TRS midi out (pio uart_tx); see send_midi_trs
  uint offset_tx = pio_add_program(pio_tx, &uart_tx_program);
  uart_tx_program_init(pio_tx, sm_tx, offset_tx, UART1_TX, UART_BAUD);
  // the ws2812 strip is set up by zeptomech_leds_setup()
}

#endif
