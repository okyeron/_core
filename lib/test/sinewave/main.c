// Copyright 2023 Zack Scholl.
//
// Author: Zack Scholl (zack.scholl@gmail.com)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// See http://creativecommons.org/licenses/MIT/ for more information.

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../fixedpoint.h"
#include "../../sinewave.h"

int main() {
  SinOsc *osc1 = SinOsc_malloc();
  // SinOsc *osc2 = SinOsc_malloc();
  SinOsc_quiet(osc1, 0);
  SinOsc_wave(osc1, 12);
  for (int i = 0; i < 4000; i++) {
    printf("%d\n", SinOsc_next(osc1));
  }
  SinOsc_wave(osc1, 24);
  for (int i = 0; i < 4000; i++) {
    printf("%d\n", SinOsc_next(osc1));
  }
  SinOsc_wave(osc1, 0);
  for (int i = 0; i < 8000; i++) {
    printf("%d\n", SinOsc_next(osc1));
  }
  SinOsc_wave(osc1, 23);
  for (int i = 0; i < 8000; i++) {
    printf("%d\n", SinOsc_next(osc1));
  }
  // SinOsc_wave(osc1, 0);
  // for (int i = 0; i < 441; i++) {
  //   printf("%d\n", SinOsc_next(osc1));
  // }
  // SinOsc_free(osc1);
  // SinOsc_free(osc2);

  return 0;
}