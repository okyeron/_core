// Copyright 2023-2025 Zack Scholl, GPLv3.0
// Zeptomech version of zeptocore.h (main.c includes it instead).
// Kept in step with upstream by scripts/sync_zeptomech_copies.sh.
#include "clockhandling.h"
//
#include "break_knob.h"
#include "midicallback.h"
#include "onewiremidi3.h"
#ifdef INCLUDE_MIDI
#include "midi_comm_callback.h"
#endif
#ifdef INCLUDE_SSD1306
#include "image.h"
#include "ssd1306.h"
#endif

// zeptomech follows the clock jack while it receives pulses, and midi clock
// (usb or TRS) otherwise; see clock_jack_active in midi_timing
uint32_t clock_jack_last_pulse = 0;

void clock_jack_handling_up(int time_diff)
{
  clock_jack_last_pulse = time_us_32();
  clock_jack_active = true;
  clock_handling_up(time_diff);
}

void clockPulseUpdate(ClockInput *clockinput)
{
#ifdef INCLUDE_CLOCKINPUT
  // clock input handler
  ClockInput_update(clockinput);
  if (!clock_jack_active)
  {
    return;
  }
  if (time_us_32() - clock_jack_last_pulse > 1000000)
  {
    // pulses stopped: hand the tempo back to midi clock
    clock_jack_active = false;
    clock_input_absent_zeptocore = true;
    return;
  }
  clock_input_absent_zeptocore = false;
  if (!clock_start_stop_sync && clock_in_do)
  {
    if ((time_us_32() - clock_in_last_time) > 2 * clock_in_diff_2x)
    {
      clock_in_ready = false;
      clock_in_do = false;
      clock_in_activator = 0;
    }
  }
#endif
}

// KnobChange only reports a pot after it moves 6 steps (of 0-255), so a slow
// turn or the end stops can stop short: with 112 samples on the sample knob
// that missed samples and sometimes the first one. A smaller move of at least
// ZEPTOMECH_KNOB_SETTLE_STEPS is reported once it has held for
// ZEPTOMECH_KNOB_SETTLE_MS; one-step jitter is ignored.
#define ZEPTOMECH_KNOB_SETTLE_STEPS 2
#define ZEPTOMECH_KNOB_SETTLE_MS 80
static int16_t zeptomech_knob_settle(KnobChange *filter, uint32_t *since)
{
  if (filter->count == 0 || filter->last_output == INT32_MIN)
  {
    return -1;
  }
  int smoothed = filter->sum / filter->count;
  if (abs(smoothed - filter->last_output) < ZEPTOMECH_KNOB_SETTLE_STEPS)
  {
    *since = 0;
    return -1;
  }
  uint32_t now = to_ms_since_boot(get_absolute_time());
  if (*since == 0)
  {
    *since = now ? now : 1;
    return -1;
  }
  if (now - *since < ZEPTOMECH_KNOB_SETTLE_MS)
  {
    return -1;
  }
  *since = 0;
  filter->last_output = smoothed;
  return smoothed;
}

// The main loop runs much faster than on zeptocore or v6 (no 1 ms sleep per
// button read, 400 kHz pots), but button combos, LED animations, the led
// blink and the knob/repeat counters below count loop passes or
// button_handler calls. Those run at the old pace instead: one pass per
// 12.3 ms with button_handler 4 times per pass.
#define ZEPTOMECH_PASS_US 12300
#define ZEPTOMECH_BUTTON_US (ZEPTOMECH_PASS_US / 4)
static bool zeptomech_tick(uint32_t *last, uint32_t period_us)
{
  uint32_t now = time_us_32();
  if (now - *last < period_us)
  {
    return false;
  }
  // keep the average rate, without a burst after a long pass
  *last = (now - *last < 2 * period_us) ? *last + period_us : now;
  return true;
}

static bool zeptomech_button_handler(ButtonMatrix *bm)
{
  static uint32_t last = 0;
  if (!zeptomech_tick(&last, ZEPTOMECH_BUTTON_US))
  {
    return false;
  }
  return button_handler(bm);
}

// sleep that keeps the usb stack running once it has started, so its event
// queue does not overflow while the host enumerates
static void zeptomech_wait_ms(uint32_t ms)
{
#ifdef INCLUDE_MIDI
  if (tud_inited())
  {
    uint32_t t0 = time_us_32();
    while (time_us_32() - t0 < ms * 1000)
    {
      tud_task();
    }
    return;
  }
#endif
  sleep_ms(ms);
}

void printStringWithDelay(char *str)
{
  int len = strlen(str);
  for (int i = 0; i < len; i++)
  {
    char currentChar = str[i];
    for (int j = 0; j < sizeof(led_text_5x4) / sizeof(led_text_5x4[0]); j++)
    {
      if (led_text_5x4[j].character == currentChar)
      {
        int led = 0;
        for (int row = 0; row < 5; row++)
        {
          for (int col = 0; col < 4; col++)
          {
            bool is_one = (led_text_5x4[j].dots[row] >> (3 - col)) & 1;
            if (is_one)
            {
              LEDS_set(leds, led, LED_BRIGHT);
            }
            else
            {
              LEDS_set(leds, led, 0);
            }
            led++;
          }
        }
        LEDS_render(leds);
        if (currentChar == '.')
        {
          zeptomech_wait_ms(50);
        }
        else
        {
          zeptomech_wait_ms(200);
        }
        break;
      }
    }
  }
}

void clear_debouncers()
{
  for (uint8_t i = 0; i < DEBOUNCE_UINT8_NUM; i++)
  {
    DebounceUint8_clear(debouncer_uint8[i]);
  }
  DebounceDigits_clear(debouncer_digits);
}

void make_random_sequence(uint8_t adcValue)
{
  if (adcValue > 255)
    adcValue = 255;
  if (adcValue < 32)
  {
    // normal
    do_retrig_at_end_of_phrase = false;
    random_sequence_length = 0;
  }
  else if (adcValue < 255 - 32)
  {
    do_retrig_at_end_of_phrase = false;
    uint8_t sequence_lengths[11] = {
        1,
        2,
        4,
        6,
        8,
        12,
        16,
        24,
        32,
        48,
        64,
    };
    random_sequence_length =
        sequence_lengths[((int16_t)(adcValue - 32) * 11 / (255 - 32)) % 11];
  }
  else
  {
    // new random sequence
    regenerate_random_sequence_arr();
    random_sequence_length = 8;
    do_retrig_at_end_of_phrase = true;
  }
  clear_debouncers();
  DebounceUint8_set(debouncer_uint8[DEBOUNCE_UINT8_LED_TRIANGLE], adcValue,
                    200);
}

void __not_in_flash_func(input_handling)()
{
#ifdef INCLUDE_MIDI
  // manually init USB instead of tusb_init(); the version text below keeps
  // the usb stack running while the host enumerates
  tud_init(0);
#endif
  printStringWithDelay("zv8.1.0");

  LEDS_clear(leds);
  LEDS_render(leds);

#ifdef BTN_COL_START
  ButtonMatrix *bm;
  // initialize button matrix
  bm = ButtonMatrix_create(BTN_ROW_START, BTN_COL_START);
#endif

#ifdef INCLUDE_MIDI
  // initialize the midi
  for (uint8_t i = 0; i < 3; i++)
  {
    midiout[i] = MidiOut_malloc(i, false);
  }
  for (uint8_t i = 3; i < MIDIOUTS; i++)
  {
    midiout[i] = MidiOut_malloc(i, true);
  }
#endif

  uint8_t new_vol;
  //   (
  // a=Array.fill(72,{ arg i;
  //   (i+60).midicps.round.asInteger
  // });
  // a.postln;
  // )
  ClockInput *clockinput = NULL;
  Onewiremidi *onewiremidi = NULL;
  // hardware TRS midi and the clock jack have their own pins, so both run
  onewiremidi = Onewiremidi_new(pio_rx, sm_rx, UART1_RX, midi_note_on,
                                midi_note_off, midi_start, midi_continue,
                                midi_stop, midi_timing, midi_control_change);
  clockinput = ClockInput_create(CLOCK_INPUT_GPIO, clock_jack_handling_up,
                                 clock_handling_down, clock_handling_start);

  FilterExp *adcs[3];
  int adc_last[3] = {0, 0, 0};
  int adc_debounce[3] = {0, 0, 0};
  const int adc_threshold_const = 200;
  int adc_threshold = adc_threshold_const;
  const int adc_debounce_max = 25;
  uint16_t adc_startup = 300;
  // TODO add debounce for the adc detection
  for (uint8_t i = 0; i < 3; i++)
  {
    adcs[i] = FilterExp_create(10);
  }
  // adc channels for knobs X/Y/Z (reversed from zeptocore's 2/1/0)
  const uint8_t knobX = 0;
  const uint8_t knobY = 1;
  const uint8_t knobZ = 2;

  uint8_t debounce_beat_repeat = 0;
  uint8_t sample_selection_index_last = 0;
  uint8_t debounce_sample_selection = 0;
  uint8_t sample_selection_index = 0;

  // initialize the resonsant filter
  global_filter_index = 12;
  for (uint8_t channel = 0; channel < 2; channel++)
  {
    ResonantFilter_setFilterType(resFilter[channel], 0);
    ResonantFilter_setFc(resFilter[channel], global_filter_index);
  }
  global_filter_index = resonantfilter_fc_max;
  for (uint8_t channel = 0; channel < 2; channel++)
  {
    ResonantFilter_setFilterType(resFilter[channel], 0);
    ResonantFilter_setFc(resFilter[channel], global_filter_index);
  }

#ifdef INCLUDE_SSD1306
  ssd1306_t disp;
  disp.external_vcc = false;
  ssd1306_init(&disp, 128, 64, 0x3C, i2c_default);
  ssd1306_clear(&disp);
#endif

#ifdef DETROITUNDERGROUND
  probability_of_random_jump = 25;
  probability_of_random_retrig = 400;
  probability_of_random_tunnel = 60;
#endif

  regenerate_random_sequence_arr();

  KnobChange *knob_change_arcade[8];
  uint32_t knob_settle_since[8] = {0};
  ADS7830 *arcade_ads7830 = NULL;
  if (is_arcade_box || is_zeptomech)
  {
    arcade_ads7830 = ADS7830_malloc(ADS7830_ADDR);
    // create array of knob changes
    for (uint8_t i = 0; i < 8; i++)
    {
      knob_change_arcade[i] = KnobChange_malloc(2);
    }
  }

  bool sel_sample_knob_ready = false;
  clock_start_stop_sync = true;
  ZD_CALL(zd_control.context[2] = getFreeHeap());
  metadata_optional_reverb();
  while (1)
  {
    static uint32_t pass_tick_last = 0;
    bool pass_tick = zeptomech_tick(&pass_tick_last, ZEPTOMECH_PASS_US);
    audio_media_poll();
    // a bank change loads the bank's metadata one small step per call, and
    // zeptomech's main loop is slower than zeptocore's, so run more steps
    // while one is under way (audio is faded out meanwhile)
    uint32_t bank_poll_start = time_us_32();
    while (bank_transition_busy() && time_us_32() - bank_poll_start < 4000)
    {
      audio_media_poll();
    }
    metadata_deferred_presets();
    ZD_CALL(zd_service(ZD_CONTROL));
#ifdef INCLUDE_MIDI
    tud_task();
    ZV_CALL(zv_realtime_service(tud_mounted(), tud_midi_packet_write));
    // Complete any partially queued telemetry before processing commands that
    // can emit another SysEx message. Clock packets may interleave legally.
    ZV_CALL(if (zv_tx_pending()) {
      zv_service(time_us_32(), tud_mounted(), tud_midi_packet_write);
      continue;
    });
    midi_comm_task(midi_comm_callback_fn, midi_note_on, midi_note_off,
                   midi_start, midi_continue, midi_stop, midi_timing,
                   midi_control_change);
    ZV_CALL(zv_service(time_us_32(), tud_mounted(), tud_midi_packet_write));
    ZV_CALL(if (zv_tx_pending()) continue);
#endif
    // process incoming hardware TRS midi
    process_midi();

    if (!total_number_samples)
    {
      sleep_ms(1);
      continue;
    }
    if (do_switch_between_clock_and_midi)
    {
      // zeptomech: TRS midi has its own UART and the clock input its own
      // jack, so there is nothing to switch between
      do_switch_between_clock_and_midi = false;
    }

    // if in startup deduct
    if (adc_startup > 0 && pass_tick)
    {
      adc_startup--;
      // if (adc_startup == 0) {
      //   for (int i = 1; i < 2; i++) {
      //     PIO p = (i == 0) ? pio0 : pio1;
      //     printf("PIO%d:\n", i);
      //     for (int sm = 2; sm < 4; sm++) {
      //       if (pio_sm_is_claimed(p, sm)) {
      //         printf("  State Machine %d: USED\n", sm);
      //       } else {
      //         printf("  State Machine %d: NOT USED\n", sm);
      //       }
      //     }
      //     sleep_ms(1);
      //   }
      // }
    }

    // check for input
    int char_input = getchar_timeout_us(10);
    if (char_input >= 0)
    {
      if (char_input == 118)
      {
        send_text_as_sysex("version=v8.1.0");
      }
    }

#ifdef BTN_COL_START
    // button handler
    if (zeptomech_button_handler(bm))
    {
      // reset knob debouncers
      adc_threshold = 10000;
      for (uint8_t i = 0; i < 3; i++)
      {
        adc_debounce[i] = 0;
      }
    }
    else
    {
      adc_threshold = adc_threshold_const;
    }
#endif

    clockPulseUpdate(clockinput);

#ifdef INCLUDE_SSD1306
    char buf_ssd1306[16];
    format_int32_decimal(buf_ssd1306, beat_current);
    ssd1306_clear(&disp);
    ssd1306_draw_string(&disp, 8, 24, 1, buf_ssd1306);
    ssd1306_bmp_show_image_with_offset(&disp, output_bmp_data, output_bmp_size,
                                       0, 64 - 17);
    // tile 4x4 on the right 64x64 pixels of a 14x14 image
    for (int i = 0; i < 4; i++)
    {
      for (int j = 0; j < 4; j++)
      {
        ssd1306_bmp_show_image_with_offset(&disp, sun_solid2_bmp_data,
                                           sun_solid2_bmp_size, 64 + i * 16,
                                           64 - 17 - j * 16 + 1);
      }
    }
    ssd1306_show(&disp);
#endif

    // check to see if the probability knobs are activated for the fx
    if (clock_did_activate)
    {
      clock_did_activate = false;
      for (uint8_t i = 0; i < 16; i++)
      {
        if (sf->fx_param[i][2] > 0)
        {
          if (sf->fx_active[i])
          {
            if (random_integer_in_range(0, 96) <
                probability_max_values_off[sf->fx_param[i][2] >> 4])
            {
              toggle_fx(i);
            }
          }
          else
          {
            if (random_integer_in_range(0, 96) <
                probability_max_values[sf->fx_param[i][2] >> 4])
            {
              toggle_fx(i);
              // TODO: also randomize the parameters?
            }
          }
        }
      }
    }

#ifdef PRINT_SDCARD_TIMING
    // random stuff
    if (random_integer_in_range(1, 10000) < 10)
    {
      // printf("random retrig\n");
      key_do_jump(random_integer_in_range(0, 15));
    }
    else if (random_integer_in_range(1, 10000) < 5)
    {
      // printf("random retrigger\n");
      go_retrigger_2key(random_integer_in_range(0, 15),
                        random_integer_in_range(0, 15));
    }
#else
#ifdef PRINT_AUDIOBLOCKDROPS
    // random stuff
    if (random_integer_in_range(1, 10000) < 80)
    {
      // printf("random retrig\n");
      key_do_jump(random_integer_in_range(0, 15));
    }
    else if (random_integer_in_range(1, 10000) < 5)
    {
      // printf("random retrigger\n");
      go_retrigger_2key(random_integer_in_range(0, 15),
                        random_integer_in_range(0, 15));
    }
#endif
#endif

    if (random_integer_in_range(1, 1000000) < probability_of_random_retrig)
    {
      sf->do_retrig_pitch_changes = (random_integer_in_range(1, 10) < 5);
      go_retrigger_2key(random_integer_in_range(0, 15),
                        random_integer_in_range(0, 15));
    }

    adc_select_input(knobX);

    // check if a single button is held
    // for purposes of changing the fx params
    int8_t single_key = -1;
    if (key_on_buttons[0] > 0 && key_on_buttons[1] > 0)
    {
    }
    else
    {
      for (uint8_t i = 4; i < 20; i++)
      {
        if (key_on_buttons[i] > 0)
        {
          if (single_key == -1)
          {
            single_key = i;
          }
          else
          {
            single_key = -1;
            break;
          }
        }
      }
    }
    if (single_key > -1)
    {
      DebounceDigits_clear(debouncer_digits);
    }

    if (debounce_beat_repeat > 0)
    {
      if (pass_tick)
        debounce_beat_repeat--;
      if (debounce_beat_repeat == 10)
      {
        BeatRepeat_repeat(beatrepeat,
                          sf->fx_param[FX_BEATREPEAT][0] * 19000 / 255 + 100);
      }
      else if (debounce_beat_repeat == 100)
      {
        BeatRepeat_repeat(beatrepeat, 20);
      }
    }

    int adc;

#ifdef INCLUDE_KNOBS
    // knob X
    uint16_t adc_raw = adc_read();
    adc = FilterExp_update(adcs[0], adc_raw);
    if (abs(adc_last[0] - adc) > adc_threshold)
    {
      adc_debounce[0] = adc_debounce_max;
    }
    if (adc_debounce[0] && pass_tick)
    {
      adc_last[0] = adc;
      adc_debounce[0]--;
      if (mode_buttons16 == MODE_MASH && single_key > -1)
      {
        sf->fx_param[single_key - 4][0] = adc * 255 / 4096;
        zeptomech_fx_param_touched(single_key - 4);
        clear_debouncers();
        DebounceUint8_set(debouncer_uint8[DEBOUNCE_UINT8_LED_BAR],
                          sf->fx_param[single_key - 4][0], 100);
        if (key_on_buttons[FX_BEATREPEAT + 4] && do_update_beat_repeat == 0)
        {
          debounce_beat_repeat = 30;
        }
        else if (key_on_buttons[FX_DELAY + 4])
        {
          Delay_setFeedbackf(delay, (float)adc / 8192.0f + 0.49f);
        }
        else if (key_on_buttons[FX_TIGHTEN + 4])
        {
          Gate_set_amount(audio_gate, sf->fx_param[FX_TIGHTEN][0]);
          // deactivated
          // } else if (key_on_buttons[FX_TREMELO + 4]) {
          //   lfo_tremelo_step =
          //       Q16_16_2PI / (12 + (255 - sf->fx_param[single_key - 4][0]) *
          //       2);
        }
        else if (key_on_buttons[FX_PAN + 4])
        {
          lfo_pan_step =
              Q16_16_2PI / (12 + (255 - sf->fx_param[single_key - 4][0]) * 2);
        }
        else if (key_on_buttons[FX_SCRATCH + 4])
        {
          scratch_lfo_hz = sf->fx_param[FX_SCRATCH][0] / 255.0 * 4.0 + 0.1;
          scratch_lfo_inc = round(SCRATCH_LFO_1_HZ * scratch_lfo_hz);
        }
        else if (key_on_buttons[FX_EXPAND + 4])
        {
          update_reverb();
        }
      }
      else if (adc_startup == 0)
      {
        if (button_is_pressed(KEY_A))
        {
#ifdef INCLUDE_MIDI
          // send out midi cc
          MidiOut_cc(midiout[0], cc_volume, adc * 127 / 4096);
#endif
          new_vol = adc * VOLUME_STEPS / 4096;
          // new_vol = 100;
          if (new_vol != sf->vol)
          {
            sf->vol = new_vol;
          }
          clear_debouncers();
          DebounceUint8_set(debouncer_uint8[DEBOUNCE_UINT8_LED_WALL],
                            adc * 255 / 4096, 200);
        }
        else if (button_is_pressed(KEY_B))
        {
#ifdef INCLUDE_MIDI
          // send out midi cc
          MidiOut_cc(midiout[0], cc_randsequence, adc * 127 / 4096);
#endif
          make_random_sequence(adc * 255 / 4096);
        }
        else if (button_is_pressed(KEY_C))
        {
          // C + X
#ifdef INCLUDE_MIDI
          // send out midi cc
          MidiOut_cc(midiout[0], cc_sampleselect, adc * 127 / 4096);
#endif
          sample_selection_index = adc_raw * sample_selection_num / 4096;
        }
        else if (button_is_pressed(KEY_D))
        {
#ifdef INCLUDE_MIDI
          // send out midi cc
          MidiOut_cc(midiout[0], cc_randjump, adc * 127 / 4096);
#endif
          probability_of_random_jump = adc * 100 / 4096;
          clear_debouncers();
          DebounceUint8_set(debouncer_uint8[DEBOUNCE_UINT8_LED_RANDOM1],
                            adc * 255 / 4096, 100);
        }
        else
        {
          if (global_knobx_sample_selector)
          {
            sample_selection_index = adc_raw * sample_selection_num / 4096;
          }
#ifdef INCLUDE_MIDI
          // send out midi cc
          MidiOut_cc(midiout[0], cc_knobx, adc * 127 / 4096);
#endif
        }
      }
    }
#endif

    if (sample_selection_index_last != sample_selection_index)
    {
      sample_selection_index_last = sample_selection_index;
      debounce_sample_selection = 40;
    }
    else if (debounce_sample_selection > 0 && pass_tick)
    {
      debounce_sample_selection--;
      if (debounce_sample_selection == 0)
      {
        uint8_t f_sel_bank_next = sample_selection[sample_selection_index].bank;
        uint8_t f_sel_sample_next =
            sample_selection[sample_selection_index].sample;
        if (f_sel_bank_next != sel_bank_cur ||
            f_sel_sample_next != sel_sample_cur)
        {
          sel_bank_next = f_sel_bank_next;
          sel_sample_next = f_sel_sample_next;
          fil_current_change = true;
        }
      }
    }

#ifdef BTN_COL_START
    if (!is_arcade_box)
      zeptomech_button_handler(bm);
#endif

    clockPulseUpdate(clockinput);

    break_fx_update();

#ifdef INCLUDE_KNOBS
    // knob Y
    adc_select_input(knobY);
    adc = FilterExp_update(adcs[1], adc_read());
    if (abs(adc_last[1] - adc) > adc_threshold)
    {
      adc_debounce[1] = adc_debounce_max;
    }
    if (adc_debounce[1] > 0 && pass_tick)
    {
      adc_last[1] = adc;
      adc_debounce[1]--;
      if (mode_buttons16 == MODE_MASH && single_key > -1)
      {
        sf->fx_param[single_key - 4][1] = adc * 255 / 4096;
        zeptomech_fx_param_touched(single_key - 4);
        if (key_on_buttons[FX_EXPAND + 4])
        {
          update_reverb();
        }
        else if (key_on_buttons[FX_DELAY + 4])
        {
          Delay_setDuration(
              delay, powf(2, linlin((float)adc, 0.0f, 4095.0f, 6.64f, 13.28f)));
        }
      }
      else if (adc_startup == 0)
      {
        if (button_is_pressed(KEY_A))
        {
#ifdef INCLUDE_MIDI
          // send out midi cc
          MidiOut_cc(midiout[0], cc_djfilter, adc * 127 / 4096);
#endif
#if DJ_FILTER
          for (uint8_t channel = 0; channel < 2; channel++)
          {
            const uint16_t filter_zero_spacing = 500;
            if (adc < 2048 - filter_zero_spacing)
            {
              global_filter_index =
                  adc * (resonantfilter_fc_max) / (2048 - filter_zero_spacing);
              global_filter_lphp = 0;
              ResonantFilter_setFilterType(resFilter[channel],
                                           global_filter_lphp);
              ResonantFilter_setFc(resFilter[channel], global_filter_index);
            }
            else if (adc > 2048 + filter_zero_spacing)
            {
              global_filter_index = (adc - (2048 + filter_zero_spacing)) *
                                    (resonantfilter_fc_max) /
                                    (2048 - filter_zero_spacing);
              global_filter_lphp = 1;
              ResonantFilter_setFilterType(resFilter[channel],
                                           global_filter_lphp);
              ResonantFilter_setFc(resFilter[channel], global_filter_index);
            }
            else
            {
              global_filter_index = resonantfilter_fc_max;
              global_filter_lphp = 0;
              ResonantFilter_setFilterType(resFilter[channel],
                                           global_filter_lphp);
              ResonantFilter_setFc(resFilter[channel], resonantfilter_fc_max);
            }
          }
#else
          set_global_lpf_cutoff(adc, 4095);
#endif
          clear_debouncers();
          DebounceUint8_set(debouncer_uint8[DEBOUNCE_UINT8_LED_SPIRAL1],
                            adc * 255 / 4096, 200);
        }
        else if (button_is_pressed(KEY_B))
        {
#ifdef INCLUDE_MIDI
          // send out midi cc
          MidiOut_cc(midiout[0], cc_pitch, adc * 127 / 4096);
#endif

          int16_t adc_original = adc;
          if (adc < 2048 - 200)
          {
            sf->pitch_val_index = adc * PITCH_VAL_MID / (2048 - 200);
          }
          else if (adc > 2048 + 200)
          {
            adc -= 2048 + 200;
            sf->pitch_val_index =
                adc * (PITCH_VAL_MAX - PITCH_VAL_MID) / (2048 - 200) +
                PITCH_VAL_MID;
          }
          else
          {
            sf->pitch_val_index = PITCH_VAL_MID;
          }
          clear_debouncers();
          DebounceUint8_set(debouncer_uint8[DEBOUNCE_UINT8_LED_TRIANGLE],
                            adc_original * 255 / 4096, 250);
        }
        else if (button_is_pressed(KEY_C))
        {
          // C + Y
#ifdef INCLUDE_MIDI
          // send out midi cc
          MidiOut_cc(midiout[0], cc_randtunnel, adc * 127 / 4096);
#endif
          probability_of_random_tunnel = adc * 1000 / 4096;
          // same cutoff as the midi cc (see midi_control_change)
          if ((adc * 127 / 4096) < 20)
          {
            probability_of_random_tunnel = 0;
          }
          clear_debouncers();
          DebounceUint8_set(debouncer_uint8[DEBOUNCE_UINT8_LED_RANDOM2],
                            adc * 255 / 4096, 100);
        }
        else if (button_is_pressed(KEY_D))
        {
          // D + Y
#ifdef INCLUDE_MIDI
          // send out midi cc
          MidiOut_cc(midiout[0], cc_randfx, adc * 127 / 4096);
#endif
          break_knob_set_point = adc * 1024 / 4096;
          clear_debouncers();
          DebounceUint8_set(debouncer_uint8[DEBOUNCE_UINT8_LED_RANDOM2],
                            adc * 255 / 4096, 100);
        }
        else
        {
#ifdef INCLUDE_MIDI
          // send out midi cc
          MidiOut_cc(midiout[0], cc_knoby, adc * 127 / 4096);
#endif
        }
      }
    }
#endif

    clockPulseUpdate(clockinput);

#ifdef BTN_COL_START
    if (!is_arcade_box)
      zeptomech_button_handler(bm);
#endif

    clockPulseUpdate(clockinput);

#ifdef INCLUDE_KNOBS
    // knob Z
    adc_select_input(knobZ);
    adc = FilterExp_update(adcs[2], adc_read());
    if (abs(adc_last[2] - adc) > adc_threshold)
    {
      adc_debounce[2] = adc_debounce_max;
    }
    if (adc_debounce[2] > 0 && pass_tick)
    {
      adc_last[2] = adc;
      adc_debounce[2]--;
      if (mode_buttons16 == MODE_MASH && single_key > -1)
      {
        sf->fx_param[single_key - 4][2] = adc * 255 / 4096;
      }
      else if (adc_startup == 0)
      {
        if (button_is_pressed(KEY_A))
        {
#ifdef INCLUDE_MIDI
          // send out midi cc
          MidiOut_cc(midiout[0], cc_realtime_stretch, adc * 127 / 4096);
#endif
          set_realtime_stretch_knob(adc);
          DebounceUint8_set(debouncer_uint8[DEBOUNCE_UINT8_LED_BAR],
                            adc * 255 / 4096, 200);
        }
        else if (button_is_pressed(KEY_B))
        {
#ifdef INCLUDE_MIDI
          // send out midi cc
          MidiOut_cc(midiout[0], cc_tempo, adc * 127 / 4096);
#endif
          uint16_t bpm_new_tempo =
              banks[sel_bank_cur]->sample[sel_sample_cur].snd[FILEZERO]->bpm;
          bpm_new_tempo = round(
              linlin(adc, 0, 4095, bpm_new_tempo / 2, bpm_new_tempo * 3 / 2));
          if (bpm_new_tempo % 10 == 1 || bpm_new_tempo % 10 == 9)
          {
            // round to nearest 5
            bpm_new_tempo = (bpm_new_tempo / 5) * 5;
          }
          else if (bpm_new_tempo % 10 == 3 || bpm_new_tempo % 10 == 7)
          {
            // round to nearest 2
            bpm_new_tempo = (bpm_new_tempo / 2) * 2;
          }
          sf->bpm_tempo = util_clamp(bpm_new_tempo, 30, 300);
          clear_debouncers();
          DebounceUint8_set(debouncer_uint8[DEBOUNCE_UINT8_LED_DIAGONAL],
                            adc * 255 / 4096, 100);
          DebounceDigits_set(debouncer_digits, sf->bpm_tempo, 300);
        }
        else if (button_is_pressed(KEY_C))
        {
#ifdef INCLUDE_MIDI
          // send out midi cc
          MidiOut_cc(midiout[0], cc_quantize, adc * 127 / 4096);
#endif

          const uint8_t quantizations[10] = {1, 6, 12, 24, 48,
                                             64, 96, 144, 192, 192};
          Sequencer_quantize(
              sf->sequencers[mode_buttons16][sf->sequence_sel[mode_buttons16]],
              quantizations[adc * 9 / 4096]);
          clear_debouncers();
          DebounceUint8_set(debouncer_uint8[DEBOUNCE_UINT8_LED_WALL],
                            adc * 255 / 4096, 200);
        }
        else if (button_is_pressed(KEY_D))
        {
          // D + Z
#ifdef INCLUDE_MIDI
          // send out midi cc
          MidiOut_cc(midiout[0], cc_randfxbank, adc * 127 / 4096);
#endif
          // change the grimoire rune
          grimoire_rune = adc * 7 / 4096;
          clear_debouncers();
          DebounceUint8_set(debouncer_uint8[DEBOUNCE_UINT8_LED_GRIMOIRE],
                            adc * 255 / 4096, 100);
        }
        else
        {
#ifdef INCLUDE_MIDI
          // send out midi cc
          MidiOut_cc(midiout[0], cc_knobz, adc * 127 / 4096);
#endif
        }
      }
    }
#endif

    // arcade knobs (zeptomech reads its knobs the same way)
    if (is_arcade_box || is_zeptomech)
    {
      // volume                   tempo
      // sample selection         dj filter
      // grimoire selection       grimoire probability
      // random sequencer         random jump
      // read the arcade box knobs
      int arcadeknobmap[8] = {cc_volume, cc_tempo, cc_sampleselect,
                              cc_djfilter, cc_randfxbank, cc_randfx,
                              cc_randsequence, cc_randjump};
      for (uint8_t i = 0; i < 8; i++)
      {
        int16_t adcValue = KnobChange_update(
            knob_change_arcade[i], (int16_t)ADS7830_read(arcade_ads7830, i));
        if (adcValue < 0)
        {
          adcValue = zeptomech_knob_settle(knob_change_arcade[i],
                                           &knob_settle_since[i]);
        }
        else
        {
          knob_settle_since[i] = 0;
        }
        if (adcValue > -1)
        {
// printf("knob %d: %d\n", i, adcValue);
#ifdef INCLUDE_MIDI
          MidiOut_cc(midiout[0], arcadeknobmap[i], adcValue * 127 / 255);
#endif
          if (i == 0)
          {
            // change volume (the arcade box knob is reversed)
            if (is_arcade_box)
            {
              new_vol = (255 - adcValue) * VOLUME_STEPS * 6 / 7 / 255;
            }
            else
            {
              new_vol = adcValue * VOLUME_STEPS * 6 / 7 / 255;
            }
            if (new_vol != sf->vol)
            {
              sf->vol = new_vol;
            }
            clear_debouncers();
            if (is_arcade_box)
            {
              DebounceUint8_set(debouncer_uint8[DEBOUNCE_UINT8_LED_BAR],
                                255 - adcValue, 200);
            }
            else
            {
              // same LED feedback as A + Z
              DebounceUint8_set(debouncer_uint8[DEBOUNCE_UINT8_LED_WALL],
                                adcValue, 200);
            }
          }
          else if (i == 1)
          {
            // change bpm/tempo
            if (adcValue < 16)
            {
              sf->bpm_tempo = banks[sel_bank_cur]
                                  ->sample[sel_sample_cur]
                                  .snd[FILEZERO]
                                  ->bpm;
            }
            else
            {
              sf->bpm_tempo = util_clamp(
                  (((adcValue - 16) * (240 - 60) / (255 - 16)) / 2) * 2 + 60,
                  60, 240);
              clear_debouncers();
              DebounceUint8_set(debouncer_uint8[DEBOUNCE_UINT8_LED_DIAGONAL],
                                adcValue, 100);
            }
          }
          else if (i == 2)
          {
            // <change_sample>
            sample_selection_index =
                adcValue * (sample_selection_num - 1) / 255;
            uint8_t f_sel_bank_next =
                sample_selection[sample_selection_index].bank;
            uint8_t f_sel_sample_next =
                sample_selection[sample_selection_index].sample;
            if (f_sel_bank_next != sel_bank_cur ||
                f_sel_sample_next != sel_sample_cur)
            {
              sel_bank_next = f_sel_bank_next;
              sel_sample_next = f_sel_sample_next;
              fil_current_change = true;
            }
            clear_debouncers();
            DebounceUint8_set(debouncer_uint8[DEBOUNCE_UINT8_LED_BAR], adcValue,
                              200);
            // </change_sample>
          }
          else if (i == 3)
          {
#if INCLUDE_ZEPTOMECH
            // <zeptomech_filter>
            // A + knob: low pass, B + knob: high pass, otherwise dj style
            for (uint8_t channel = 0; channel < 2; channel++)
            {
              if (button_is_pressed(KEY_A) || button_is_pressed(KEY_B))
              {
                global_filter_index = adcValue * (resonantfilter_fc_max) / 256;
                global_filter_lphp = button_is_pressed(KEY_A) ? 0 : 1;
                ResonantFilter_setFilterType(resFilter[channel],
                                             global_filter_lphp);
                ResonantFilter_setFc(resFilter[channel], global_filter_index);
              }
              else
              {
                const int filter_spacing = 8;
                if (adcValue < 128 - filter_spacing)
                {
                  global_filter_index =
                      adcValue * (resonantfilter_fc_max) / (128 - filter_spacing);
                  global_filter_lphp = 0;
                  ResonantFilter_setFilterType(resFilter[channel],
                                               global_filter_lphp);
                  ResonantFilter_setFc(resFilter[channel], global_filter_index);
                }
                else if (adcValue >= 128 + filter_spacing)
                {
                  global_filter_index = (adcValue - (128 + filter_spacing)) *
                                        (resonantfilter_fc_max) /
                                        (128 - filter_spacing);
                  global_filter_lphp = 1;
                  ResonantFilter_setFilterType(resFilter[channel],
                                               global_filter_lphp);
                  ResonantFilter_setFc(resFilter[channel], global_filter_index);
                }
                else
                {
                  global_filter_index = resonantfilter_fc_max;
                  global_filter_lphp = 0;
                  ResonantFilter_setFilterType(resFilter[channel],
                                               global_filter_lphp);
                  ResonantFilter_setFc(resFilter[channel],
                                       resonantfilter_fc_max);
                }
              }
            }
            // </zeptomech_filter>
#elif DJ_FILTER
            // <dj_style_filter>
            for (uint8_t channel = 0; channel < 2; channel++)
            {
              if (adcValue < 128)
              {
                if (adcValue < 5)
                {
                  global_filter_index = resonantfilter_fc_max;
                }
                else
                {
                  global_filter_index =
                      (128 - adcValue) * (resonantfilter_fc_max) / 128;
                }
                global_filter_lphp = 0;
                ResonantFilter_setFilterType(resFilter[channel],
                                             global_filter_lphp);
                ResonantFilter_setFc(resFilter[channel], global_filter_index);
              }
              else if (adcValue >= 128)
              {
                global_filter_index =
                    (128 - (adcValue - 128)) * resonantfilter_fc_max / 128;
                global_filter_lphp = 1;
                ResonantFilter_setFilterType(resFilter[channel],
                                             global_filter_lphp);
                ResonantFilter_setFc(resFilter[channel], global_filter_index);
              }
              else
              {
                global_filter_index = resonantfilter_fc_max;
                global_filter_lphp = 0;
                ResonantFilter_setFilterType(resFilter[channel],
                                             global_filter_lphp);
                ResonantFilter_setFc(resFilter[channel], resonantfilter_fc_max);
              }
            }
            // </dj_style_filter>
#else
            // <lpf_filter>
            set_global_lpf_cutoff(adcValue, 255);
            // </lpf_filter>
#endif
            clear_debouncers();
            DebounceUint8_set(debouncer_uint8[DEBOUNCE_UINT8_LED_SPIRAL1],
                              adcValue, 200);
          }
          else if (i == 4)
          {
            // <grimoire_selection>
            // change the grimoire rune
            grimoire_rune = adcValue * 7 / 255;
            clear_debouncers();
            DebounceUint8_set(debouncer_uint8[DEBOUNCE_UINT8_LED_GRIMOIRE],
                              adcValue, 100);
            // </grimoire_selection>
          }
          else if (i == 5)
          {
            // <grimoire_probability>
            break_knob_set_point = adcValue * 1024 / 255;
            clear_debouncers();
            DebounceUint8_set(debouncer_uint8[DEBOUNCE_UINT8_LED_RANDOM2],
                              adcValue, 100);
            // </grimoire_probability>
          }
          else if (i == 6)
          {
            // <random_sequencer>
            make_random_sequence(adcValue);
            // </random_sequencer>
          }
          else if (i == 7)
          {
            // <random_jump>
            if (adcValue < 128)
            {
              sf->stay_in_sync = true;
              probability_of_random_jump = adcValue * 100 / 128;
            }
            else if (adcValue >= 128)
            {
              sf->stay_in_sync = false;
              probability_of_random_jump = (255 - adcValue) * 100 / 128;
            }
            clear_debouncers();
            DebounceUint8_set(debouncer_uint8[DEBOUNCE_UINT8_LED_RANDOM1],
                              adcValue, 100);
            // </random_jump>
          }
        }
      }
    }

    // button_handler already renders the leds; on zeptomech a second render
    // here would push the ws2812 strip twice per loop
    // LEDText_update(ledtext, leds);
    // LEDS_render(leds);

#ifdef BTN_COL_START
    if (!is_arcade_box)
      zeptomech_button_handler(bm);
#endif

    clockPulseUpdate(clockinput);

    clockPulseUpdate(clockinput);

#ifdef INCLUDE_KEYBOARD
    // check keyboard
    run_keyboard();
#endif
  }
}
