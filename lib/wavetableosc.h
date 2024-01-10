#ifndef LIB_WAVETABLEOSC_H
#define LIB_WAVETABLEOSC_H 1
#include "fixedpoint.h"
//
#include "crossfade3.h"
#include "stdbool.h"
#include "wavetable_data.h"

typedef struct WaveOsc {
  uint8_t wave[2];
  uint16_t phase[2];
  uint16_t limit[2];
  uint8_t quiet[2];
  uint16_t crossfade;
  uint8_t crossfade_factor;
} WaveOsc;

void WaveOsc_wave(WaveOsc *self, uint8_t wave) {
  if (self->wave[0] == wave) {
    return;
  }
  if (wave >= 0 && wave < WAVETABLEOSC_MAX) {
    self->crossfade = 0;
    self->quiet[1] = self->quiet[0];
    self->wave[1] = self->wave[0];
    self->phase[1] = self->phase[0];
    self->limit[1] = self->limit[0];
    self->wave[0] = wave;
    self->phase[0] = 0;
    self->limit[0] = sinewave_len(wave);
  }
}

void WaveOsc_quiet(WaveOsc *self, uint8_t quiet) {
  if (quiet >= 0 && quiet <= 16) {
    self->crossfade = 0;
    self->quiet[1] = self->quiet[0];
    self->wave[1] = self->wave[0];
    self->phase[1] = self->phase[0];
    self->limit[1] = self->limit[0];
    self->quiet[0] = quiet;
  }
}

WaveOsc *WaveOsc_malloc() {
  WaveOsc *self = malloc(sizeof(WaveOsc));
  self->quiet[0] = 0;
  self->quiet[1] = 0;
  self->wave[0] = 0;
  self->wave[1] = 0;
  self->phase[0] = 0;
  self->phase[1] = 0;
  self->limit[0] = sinewave_len(0);
  self->limit[1] = sinewave_len(0);
  self->crossfade = CROSSFADE3_LIMIT;
  return self;
}

void WaveOsc_free(WaveOsc *self) { free(self); }

int32_t WaveOsc_next(WaveOsc *self) {
  int32_t val = 0;
  if (self->crossfade < CROSSFADE3_LIMIT) {
    if (self->phase[0] >= self->limit[0]) {
      self->phase[0] = 0;
    }
    if (self->phase[1] >= self->limit[1]) {
      self->phase[1] = 0;
    }
    if (self->wave[0] == 0) {
      val = q16_16_multiply(crossfade3_cos_out[self->crossfade],
                            sinewave_sample(self->wave[1], self->phase[1])) >>
            self->quiet[1];
      if (self->crossfade_factor == 0) {
        self->crossfade++;
        self->crossfade_factor = 2;
      } else {
        self->crossfade_factor--;
      }
    } else if (self->wave[1] == 0) {
      val = q16_16_multiply(Q16_16_1 - crossfade3_line[self->crossfade],
                            sinewave_sample(self->wave[0], self->phase[0])) >>
            self->quiet[0];

      self->crossfade++;
    } else {
      val = (q16_16_multiply(Q16_16_1 - crossfade3_line[self->crossfade],
                             sinewave_sample(self->wave[0], self->phase[0])) >>
             self->quiet[0]) +
            (q16_16_multiply(crossfade3_line[self->crossfade],
                             sinewave_sample(self->wave[1], self->phase[1])) >>
             self->quiet[1]);
      self->crossfade++;
    }
    self->phase[0]++;
    self->phase[1]++;
  } else if (self->wave[0] > 0) {
    if (self->phase[0] >= self->limit[0]) {
      self->phase[0] = 0;
    }
    val = sinewave_sample(self->wave[0], self->phase[0]) >> self->quiet[0];
    self->phase[0]++;
  }
  return val;
}

#endif