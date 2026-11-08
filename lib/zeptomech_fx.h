// Zeptomech effect settings (included from includes.h, before
// sdcard_startup.h).
//
// Effects keep the X / Y values set in mash mode:
// - update_fx() turning on the delay sets it from its saved values (upstream
//   picks a tempo-based duration and random feedback), and circulate gets
//   its rate back, which upstream only sets while the knob turns;
// - random effects (break_fx_toggle) write random values over the saved
//   ones; the saved values are kept here and put back when the random
//   effect ends, unless a knob changed them meanwhile.
#ifndef ZEPTOMECH_FX_H
#define ZEPTOMECH_FX_H 1

// true while break_fx_toggle() runs, so update_fx() keeps its random values
static bool zeptomech_break_fx_busy = false;

static bool zeptomech_fx_saved[16];
static uint8_t zeptomech_fx_saved_param[16][2];
static int32_t zeptomech_pan_step_saved;

// the effect whose settings a random effect overwrites, or -1
static int8_t zeptomech_break_fx_target(uint8_t effect) {
  switch (effect) {
    case 1:
      return FX_SHAPER;
    case 2:
      return FX_BITCRUSH;
    case 3:
      return FX_FILTER;
    case 6:
      return FX_COMB;
    case 9:
      return FX_PAN;  // its rate, lfo_pan_step
    case 10:
    case 11:
      return FX_REPITCH;
    case 15:
      return FX_TAPE_STOP;
    default:
      return -1;
  }
}

static void zeptomech_fx_restore(int8_t fx) {
  if (fx < 0 || !zeptomech_fx_saved[fx]) return;
  sf->fx_param[fx][0] = zeptomech_fx_saved_param[fx][0];
  sf->fx_param[fx][1] = zeptomech_fx_saved_param[fx][1];
  if (fx == FX_PAN) lfo_pan_step = zeptomech_pan_step_saved;
}

// called at the start of break_fx_toggle()
void zeptomech_break_fx_begin(uint8_t effect, bool on) {
  zeptomech_break_fx_busy = true;
  int8_t fx = zeptomech_break_fx_target(effect);
  if (fx < 0) return;
  if (on) {
    if (!zeptomech_fx_saved[fx]) {
      zeptomech_fx_saved_param[fx][0] = sf->fx_param[fx][0];
      zeptomech_fx_saved_param[fx][1] = sf->fx_param[fx][1];
      if (fx == FX_PAN) zeptomech_pan_step_saved = lfo_pan_step;
      zeptomech_fx_saved[fx] = true;
    }
  } else {
    // turning off uses the saved values too
    zeptomech_fx_restore(fx);
  }
}

// called at the end of break_fx_toggle()
void zeptomech_break_fx_end(uint8_t effect, bool on) {
  zeptomech_break_fx_busy = false;
  int8_t fx = zeptomech_break_fx_target(effect);
  if (fx < 0 || on) return;
  // tapestop and circulate pick random values when turning off as well
  zeptomech_fx_restore(fx);
  zeptomech_fx_saved[fx] = false;
}

// a knob set this effect's value in mash mode: it replaces the saved one
void zeptomech_fx_param_touched(uint8_t fx) {
  if (fx < 16) zeptomech_fx_saved[fx] = false;
}

// called at the end of update_fx()
void zeptomech_update_fx(uint8_t fx_num) {
  if (zeptomech_break_fx_busy || !sf->fx_active[fx_num]) return;
  if (fx_num == FX_DELAY) {
    // the knob handlers' mappings (key 6 + X / Y)
    float adc_x = sf->fx_param[FX_DELAY][0] * 4096.0f / 255.0f;
    float adc_y = sf->fx_param[FX_DELAY][1] * 4096.0f / 255.0f;
    Delay_setFeedbackf(delay, adc_x / 8192.0f + 0.49f);
    Delay_setDuration(delay,
                      powf(2, linlin(adc_y, 0.0f, 4095.0f, 6.64f, 13.28f)));
  } else if (fx_num == FX_PAN) {
    lfo_pan_step = Q16_16_2PI / (12 + (255 - sf->fx_param[FX_PAN][0]) * 2);
  }
}

#endif
