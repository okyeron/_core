// MIDI code adapted from Jacob Vosmaer
// MIT License
//
// Copyright (c) 2020 andrewikenberry
// Copyright (c) 2023 Jacob Vosmaer
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
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <stdint.h>

#include "uart_rx.pio.h"

enum {
  MIDI_NOTE_OFF = 0x80,
  MIDI_NOTE_ON = 0x90,
  MIDI_AFTERTOUCH = 0xa0,
  MIDI_CONTROL_CHANGE = 0xb0,
  MIDI_PROGRAM_CHANGE = 0xc0,
  MIDI_CHANNEL_PRESSURE = 0xd0,
  MIDI_PITCH_BEND = 0xe0,
  MIDI_SYSEX = 0xf0,
  MIDI_SYSEX_END = 0xf7,
  MIDI_TIMING_CLOCK = 0xf8,
  MIDI_ACTIVE_SENSE = 0xfe,
  MIDI_START = 0xfa,
  MIDI_CONTINUE = 0xfb,
  MIDI_STOP = 0xfc,
};

typedef struct Onewiremidi {
  PIO pio;
  unsigned char sm;
  uint8_t status;
  uint8_t previous;
  uint32_t last_time;
  callback_int_int midi_note_on;
  callback_int midi_note_off;
  callback_void midi_start;
  callback_void midi_continue;
  callback_void midi_stop;
  callback_void midi_timing;
  callback_uint8_uint8_uint8 midi_control_change;
} Onewiremidi;

/*
// TESTING PIO INTERRUPTS
static void async_worker_func(async_context_t *async_context, async_when_pending_worker_t *worker);
// An async context is notified by the irq to "do some work"
static async_context_threadsafe_background_t async_context;
static async_when_pending_worker_t worker = { .do_work = async_worker_func };
*/

Onewiremidi *Onewiremidi_new(PIO pio, unsigned char sm, const uint pin,
                             callback_int_int midi_note_on,
                             callback_int midi_note_off,
                             callback_void midi_start,
                             callback_void midi_continue,
                             callback_void midi_stop,
                             callback_void midi_timing,
                             callback_uint8_uint8_uint8 midi_control_change) {
  Onewiremidi *self = (Onewiremidi *)malloc(sizeof(Onewiremidi));
  self->pio = pio;
  self->sm = sm;
  self->status = 0;
  self->previous = 0;
  self->midi_note_on = midi_note_on;
  self->midi_note_off = midi_note_off;
  self->midi_start = midi_start;
  self->midi_continue = midi_continue;
  self->midi_stop = midi_stop;
  self->midi_timing = midi_timing;
  self->midi_control_change = midi_control_change;

  //  PIO INTRERUPT TESTING
/*
  uint rx_offset = pio_add_program(pio, &uart_rx_program);
  uart_rx_program_init(pio, sm, rx_offset, pin, UART_BAUD);
     // Find a free irq
    pio_irq = pio_get_irq_num(pio, 0);
    if (irq_get_exclusive_handler(pio_irq)) {
        pio_irq++;
        if (irq_get_exclusive_handler(pio_irq)) {
            panic("All IRQs are in use");
        }
    }
    // Enable interrupt
    irq_add_shared_handler(pio_irq, midi_read_poll, PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY); // Add a shared IRQ handler
    irq_set_enabled(pio_irq, true); // Enable the IRQ
    const uint irq_index = pio_irq - pio_get_irq_num(pio, 0); // Get index of the IRQ
    pio_set_irqn_source_enabled(pio, irq_index, pio_get_rx_fifo_not_empty_interrupt_source(sm), true); // Set pio to tell us when the FIFO is NOT empty
*/

  return self;
}



void Onewiremidi_destroy(Onewiremidi *self) { free(self); }

uint8_t Onewiremidi_reverse_uint8_t(uint8_t b) {
  b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
  b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
  b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
  return b;
}

typedef struct midi_message {
  uint8_t status;
  uint8_t data[2];
} midi_message;


// uint8_t midi_msg[32]; // typically 3 bytes, but more for sysex messages. cou
uint8_t midi_rx_state = 0;  
unsigned mPendingMessageExpectedLength;
unsigned mPendingMessageIndex;
uint8_t mPendingMessage[3];

typedef struct MidiMessage{
  uint8_t type;
  uint8_t channel;
  uint8_t data1;
  uint8_t data2;
  bool valid;
  uint8_t mlength;
  uint8_t sysexArray[32];
} MidiMessage;

MidiMessage mMessage;


static inline uint8_t midi_length(uint8_t status) {
  if (status < 0xc0)
    return 3;
  if (status < 0xe0)
    return 2; // 0xcx and 0xdx are 1 byte messages
  if (status < 0xf0)
    return 3; // 0xex are 2 byte messages
  const uint8_t syssize[16] = {
      0, // 0xf0 sysex start
      1, // 0xf1 time code
      2, // 0xf2 song position
      1, // 0xf3 song select
      1, // 0xf4 tune request
  };
  return 1 + syssize[status & 0xf];
}

uint8_t getTypeFromStatusByte(uint8_t inStatus) {
    if ((inStatus < 0x80) || (inStatus == 0xF4) ||
        (inStatus == 0xF5) || (inStatus == 0xFD)) {
        return 0; // Data bytes and undefined.
    }
    if (inStatus < 0xf0){
        // Channel message, remove channel nibble.
        return (inStatus & 0xf0);
    }
    return (inStatus);
}
uint8_t getChannelFromStatusByte(uint8_t inStatus){
    return (inStatus & 0x0f) + 1;
}
bool isChannelMessage(uint8_t inType){
    return (inType == 0x80 ||
            inType == 0x90 ||
            inType == 0xb0 ||
            inType == 0xA0 ||
            inType == 0xD0 ||
            inType == 0xE0 ||
            inType == 0xC0);
}

uint8_t midi_msg[16]; 		// typically 3 bytes, but more for sysex messages. 
uint8_t usb_midi_msg[4];    // first byte is CIN/cable identifier
uint8_t midi_rx_pos = 0;  // the current position within midi_msg

// READ INCOMING HARDWARE MIDI
uint32_t midi_read_poll(void) {
	uart_hw_t *uart = uart_get_hw(UART_ID);
	while (uart_is_readable(UART_ID)) {
	    uint8_t c = (uint8_t)uart->dr;

		switch (c) {
		case 0xF7: { // 0xF7
			// end of a sysex message
			// caller can read the sysex out of midi_msg buffer directly
			// keep assembling unless buffer is full
			if (midi_rx_pos < sizeof(midi_msg)) {
				midi_msg[midi_rx_pos++] = c;
			}
			return 0xF7; 
		} break;
		// Real time messages
		// These can be anywhere in the stream except in sysex messgages
		// so this c needs to be processed immediately 
		// and removed from the logic of assembling a note
		case 0xF8: // 0xF8
		case 0xFA: // 0xFA
		case 0xFB: // 0xFB
		case 0xFC: // 0xFC
		case 0xFE : // 0xFE
		case 0xFF: // 0xFF
			return c; // cable 0 in top byte
		}

		// if the first bit is set,
		// c is a status byte
		if (c & 0x80) {
			midi_rx_pos = 0;
			*(uint32_t *)midi_msg = 0;
		}
		// keep assembling unless buffer is full
		if (midi_rx_pos < sizeof(midi_msg)) {
			midi_msg[midi_rx_pos++] = c;
		}
		// use status byte to determine 
		// whether we collected enough bytes
		// to complete a message 
		if (midi_rx_pos == midi_length(midi_msg[0])) {
			midi_rx_pos = 1;            	// support for running status
			midi_msg[3] = 0;           		// 0 for TRS Midi
			return *(uint32_t *)midi_msg; 	// got a complete message!
		}
  	} // while uart_is_readable

	return 0;
}

// PROCESS INCOMING HARDWARE MIDI
void process_midi(){
  	uint32_t midimsg;

	// read it and weep
	while ((midimsg = midi_read_poll())) {

    uint8_t type = midimsg;         // first byte = message type & channel
    uint8_t d0 = midimsg>>8;        // second byte = data byte 0
    uint8_t d1 = midimsg>>16;       // third byte = data byte 1
    uint8_t cable = midimsg>>24;    // the cable number for midi clock (0xff is TRS, 0 is usb)

    int MidiRunningStatus = type & 0xf0;
    int channel = type & 0xf;

  // uint8_t d0 = mMessage.data1;
  // uint8_t d1 = mMessage.data2;
  // uint8_t channel = mMessage.channel;
  // printf("[midi] data:  %02x,%02x,%02x\n", type, d0, d1);
  switch (type) {
    case 0xf2: // set song position
      printf("midi set song position %d\n", d0 * 128 + d1);
      break;
    case 0xfa: // start
      // printf("midi start\n");
      midi_start();
      break;
    case 0xfb: // continue
      // printf("midi continue\n");
      midi_continue();
      break;
    case 0xfc: // stop
      // printf("midi stop\n");
      midi_stop();
      break;
    case 0xf8: // clock
      // printf("  midi clock\n");
      // send_midi_clock();
      // midi_timing ignores midi clock while the clock jack is active
      midi_timing();
      break;
    case 0xfe: // active sensing
      break;
    case 0xff: // reset
      printf("midi reset\n");
      break;

    case 0xc0: // Program change
      //  printf("program change\n");
      // midi_onProgramChange(d0, channel);
      break;

  	case 0x80: // note off
      // printf("midi note off\n");
      midi_note_off(d0);
      break;
	  case 0x90: // note on
      // printf("midi note on\n");
      midi_note_on(d0, d1);
      break;
	
    case 0xb0: // control change
      // this is a CC message
      midi_control_change (channel, d0, d1);
      break;
    }
  }
}

// void midi_read_poll() { 
//     // if (pio_sm_is_rx_fifo_empty(pio_rx, sm_rx)) {
//     //   return;
//     // }
//     // uint8_t extracted = uart_rx_program_getc(pio_rx, sm_rx);
// /*
// // TESTING PIO INTERRUPTS
//       while(!pio_sm_is_rx_fifo_empty(pio_rx, sm_rx)) {
//         uint8_t extracted = uart_rx_program_getc(pio_rx, sm_rx);
//         if (!queue_try_add(&fifo_rx, &extracted)) {
//             panic("fifo full");
//         }
//     }
//     // Tell the async worker that there are some characters waiting for us
//     async_context_set_work_pending(&async_context.core, &worker);
// */

//     // UART RX
//     volatile uint8_t extracted;
//     if (uart_is_readable (UART_ID)){
//       extracted = uart_getc(UART_ID);
//     } else {
//       return;
//     }
//     printf("[midi] data: %02x\n", extracted);
    
//     // PARSE INCOMING MIDI BYTES
//     if (mPendingMessageIndex == 0){
//         mPendingMessage[0] = extracted;
//         const uint8_t pendingType = getTypeFromStatusByte(mPendingMessage[0]);

//         switch(pendingType){
//             // 1 byte messages
//             case 0xf8: // Clock
//             case 0xfa: // Start
//             case 0xfb: // Continue
//             case 0xfc: // Stop
//             case 0xf6: // Tune Request
//             case 0xF9: // Tick / undefined
//             case 0xfe: // Active Sensing
//             case 0xff: // SystemReset
//                 mMessage.type    = pendingType;
//                 mMessage.channel = 0;
//                 mMessage.data1   = 0;
//                 mMessage.data2   = 0;
//                 mMessage.valid   = true;
//                 mPendingMessageIndex = 0;
//                 mPendingMessageExpectedLength = 0;
//                 // return;
//                 break;
//             // 2 byte messages
//             case 0xC0: // program change
//             case 0xD0: // AfterTouchChannel
//             case 0xF1: // TimeCodeQuarterFrame
//             case 0xF3: // SongSelect
//                 mPendingMessageExpectedLength = 2;
//                 break;
//             // 3 bytes messages
//             case 0x80:  // Note on
//             case 0x90:  // Note off
//             case 0xB0:  // CC
//             case 0xE0:  // Pitchbend
//             case 0xA0:  // AfterTouchPoly:
//             case 0xF2:  // SongPosition:
//                 mPendingMessageExpectedLength = 3;
//                 break;
//             default:
//                 return;
//                 break;
//         }
//         if (mPendingMessageIndex >= (mPendingMessageExpectedLength - 1)) {
//             // Reception complete
//             mMessage.type    = pendingType;
//             mMessage.channel = getChannelFromStatusByte(mPendingMessage[0]);
//             mMessage.data1   = mPendingMessage[1];
//             mMessage.data2   = 0; // Completed new message has 1 data byte
//             mMessage.mlength  = 1;

//             mPendingMessageIndex = 0;
//             mPendingMessageExpectedLength = 0;
//             mMessage.valid = true;
//             return;
//         } else {
//             // Waiting for more data
//             mPendingMessageIndex++;
//         }
//         if(mMessage.valid){
//             printf("[midi] data:  %02x,%02x,%02x,%02x\n", mMessage.type, mMessage.channel, mMessage.data1, mMessage.data2);
//             process_midi( mMessage);
//         }

//         return;  
//     } else {
//         // First, test if this is a status byte
//         if (extracted >= 0x80){
//         switch(extracted){
//             // 1 byte messages
//             case 0xf8: // Clock
//             case 0xfa: // Start
//             case 0xfb: // Continue
//             case 0xfc: // Stop
//             case 0xF9: // Tick / undefined
//             case 0xfe: // Active Sensing
//             case 0xff: // SystemReset
//                     mMessage.type    = extracted;
//                     mMessage.data1   = 0;
//                     mMessage.data2   = 0;
//                     mMessage.channel = 0;
//                     mMessage.mlength  = 1;
//                     mMessage.valid   = true;
//                     if(mMessage.valid){
//                         process_midi( mMessage);
//                     }
//                     return;
//             default:
//                 break;
//             }
//           }
//           if ((mPendingMessage[0] == 0xF0) ||  (mPendingMessage[0] == 0xF7)){
//               mMessage.sysexArray[mPendingMessageIndex] = extracted;
//           }else{
//               mPendingMessage[mPendingMessageIndex] = extracted;
//           }
//           // Now we are going to check if we have reached the end of the message
//           if (mPendingMessageIndex >= (mPendingMessageExpectedLength - 1)){

//               mMessage.type = getTypeFromStatusByte(mPendingMessage[0]);

//               if (isChannelMessage(mMessage.type))
//                   mMessage.channel = getChannelFromStatusByte(mPendingMessage[0]);
//               else
//                   mMessage.channel = 0;

//               mMessage.data1 = mPendingMessage[1];
//               // Save data2 only if applicable
//               mMessage.data2 = mPendingMessageExpectedLength == 3 ? mPendingMessage[2] : 0;
//               mMessage.mlength = mPendingMessageExpectedLength;

//               // Reset local variables
//               mPendingMessageIndex = 0;
//               mPendingMessageExpectedLength = 0;
//               mMessage.valid = true;
//           } else {
//               // Then update the index of the pending message.
//               mPendingMessageIndex++;
//               return;
//           }
//       if(mMessage.valid){
//           process_midi( mMessage);
//       }
//     }       

// }

// PIO ASYNC - Process characters
// static void async_worker_func(__unused async_context_t *async_context, __unused async_when_pending_worker_t *worker) {
//   work_done = true;
//   while(!queue_is_empty(&fifo_rx)) {
//       uint8_t extracted;
//       if (!queue_try_remove(&fifo_rx, &extracted)) {
//           panic("fifo empty");
//       }
//       // Do stuff
//     // printf("[midi] data: %02x\n", extracted);
 
//     if (mPendingMessageIndex == 0){
//         mPendingMessage[0] = extracted;
//         const uint8_t pendingType = getTypeFromStatusByte(mPendingMessage[0]);

//         switch(pendingType){
//             // 1 byte messages
//             case 0xf8: // Clock
//             case 0xfa: // Start
//             case 0xfb: // Continue
//             case 0xfc: // Stop
//             case 0xf6: // Tune Request
//             case 0xF9: // Tick / undefined
//             case 0xfe: // Active Sensing
//             case 0xff: // SystemReset
//                 mMessage.type    = pendingType;
//                 mMessage.channel = 0;
//                 mMessage.data1   = 0;
//                 mMessage.data2   = 0;
//                 mMessage.valid   = true;
//                 mPendingMessageIndex = 0;
//                 mPendingMessageExpectedLength = 0;
//                 // return;
//                 break;
//             // 2 byte messages
//             case 0xC0: // program change
//             case 0xD0: // AfterTouchChannel
//             case 0xF1: // TimeCodeQuarterFrame
//             case 0xF3: // SongSelect
//                 mPendingMessageExpectedLength = 2;
//                 break;
//             // 3 bytes messages
//             case 0x80:  // Note on
//             case 0x90:  // Note off
//             case 0xB0:  // CC
//             case 0xE0:  // Pitchbend
//             case 0xA0:  // AfterTouchPoly:
//             case 0xF2:  // SongPosition:
//                 mPendingMessageExpectedLength = 3;
//                 break;
//             default:
//                 return;
//                 break;
//         }
//         if (mPendingMessageIndex >= (mPendingMessageExpectedLength - 1)) {
//             // Reception complete
//             mMessage.type    = pendingType;
//             mMessage.channel = getChannelFromStatusByte(mPendingMessage[0]);
//             mMessage.data1   = mPendingMessage[1];
//             mMessage.data2   = 0; // Completed new message has 1 data byte
//             mMessage.mlength  = 1;

//             mPendingMessageIndex = 0;
//             mPendingMessageExpectedLength = 0;
//             mMessage.valid = true;
//             return;
//         } else {
//             // Waiting for more data
//             mPendingMessageIndex++;
//         }
//         if(mMessage.valid){
//             printf("[midi] data:  %02x,%02x,%02x,%02x\n", mMessage.type, mMessage.channel, mMessage.data1, mMessage.data2);
//             process_midi( mMessage);
//         }

//         return;  
//     } else {
//         // First, test if this is a status byte
//         if (extracted >= 0x80){
//         switch(extracted){
//             // 1 byte messages
//             case 0xf8: // Clock
//             case 0xfa: // Start
//             case 0xfb: // Continue
//             case 0xfc: // Stop
//             case 0xF9: // Tick / undefined
//             case 0xfe: // Active Sensing
//             case 0xff: // SystemReset
//                     mMessage.type    = extracted;
//                     mMessage.data1   = 0;
//                     mMessage.data2   = 0;
//                     mMessage.channel = 0;
//                     mMessage.mlength  = 1;
//                     mMessage.valid   = true;
//                     if(mMessage.valid){
//                         process_midi( mMessage);
//                     }
//                     return;
//             default:
//                 break;
//             }
//           }
//           if ((mPendingMessage[0] == 0xF0) ||  (mPendingMessage[0] == 0xF7)){
//               mMessage.sysexArray[mPendingMessageIndex] = extracted;
//           }else{
//               mPendingMessage[mPendingMessageIndex] = extracted;
//           }
//           // Now we are going to check if we have reached the end of the message
//           if (mPendingMessageIndex >= (mPendingMessageExpectedLength - 1)){

//               mMessage.type = getTypeFromStatusByte(mPendingMessage[0]);

//               if (isChannelMessage(mMessage.type))
//                   mMessage.channel = getChannelFromStatusByte(mPendingMessage[0]);
//               else
//                   mMessage.channel = 0;

//               mMessage.data1 = mPendingMessage[1];
//               // Save data2 only if applicable
//               mMessage.data2 = mPendingMessageExpectedLength == 3 ? mPendingMessage[2] : 0;
//               mMessage.mlength = mPendingMessageExpectedLength;

//               // Reset local variables
//               mPendingMessageIndex = 0;
//               mPendingMessageExpectedLength = 0;
//               mMessage.valid = true;
//           } else {
//               // Then update the index of the pending message.
//               mPendingMessageIndex++;
//               return;
//           }
//       if(mMessage.valid){
//           process_midi( mMessage);
//       }
//     }
//   }
// }

void Onewiremidi_receive(void *self) {
  // midi_read_poll();
}

// void Onewiremidi_receive(void *self) {
//   // for (uint8_t i = 0; i < 4; i++) {
//     Onewiremidi_receive_(self);
//   // }
// }
