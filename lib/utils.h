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

#ifndef LIB_UTILS
#define LIB_UTILS 1

// callback definitions
typedef void (*callback_int_int)(int, int);
typedef void (*callback_int)(int);
typedef void (*callback_int32)(int32_t);
typedef void (*callback_uint8_uint8)(uint8_t, uint8_t);
typedef void (*callback_uint8)(uint8_t);
typedef void (*callback_uint16)(uint16_t);
typedef void (*callback_void)();

// multiplies and clips the output
void MultipyAndClip_process(int32_t mul, int16_t max_val, int16_t *values,
                            uint16_t num_values) {
  for (uint16_t i = 0; i < num_values; i++) {
    int32_t v = ((int32_t)values[i] * mul);
    if (v > max_val) {
      v = max_val;
    } else if (v < -max_val) {
      v = -max_val;
    }
    values[i] = v;
  }
}
#endif