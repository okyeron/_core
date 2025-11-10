#ifndef LIB_MIDI_COMM_H
#define LIB_MIDI_COMM_H 1
#include <stdarg.h>  // Include this header for va_start, va_end, etc.
#include <stdio.h>   // Include for vsnprintf
#include <hardware/pio.h>

#ifdef INCLUDE_ZEPTOMECH
#define UART_ID uart1
#define UART_BAUD 31250
PIO pio_tx = pio2;
uint sm_tx = 0;
#endif

#ifdef INCLUDE_MIDI
uint32_t send_buffer_as_sysex(char* buffer, uint32_t bufsize) {
  uint8_t sysex_data[bufsize + 2];  // +2 for SysEx start, +3 for manufacturer?, +1 for end byte

  sysex_data[0] = 0xF0;  // Start of SysEx
  // sysex_data[1] = 0x7d; // manufacturer ID 3 bytes long,
	// sysex_data[2] = 0x00;
	// sysex_data[3] = 0x00;

  for (uint32_t i = 0; i < bufsize; i++) {
    sysex_data[i + 1] = buffer[i];  // Copy buffer into SysEx data
  }
  sysex_data[bufsize + 1] = 0xF7;  // End of SysEx

  return tud_midi_n_stream_write(0, 0, sysex_data, sizeof(sysex_data));
}

uint32_t send_text_as_sysex(const char* text) {
  uint32_t text_length = strlen(text);  // Get the length of the text
  uint8_t sysex_data[text_length + 2];  // +2 for SysEx start and end bytes

  sysex_data[0] = 0xF0;  // Start of SysEx
  // sysex_data[1] = 0x7d; // manufacturer ID 3 bytes long,
	// sysex_data[2] = 0x00;
	// sysex_data[3] = 0x00;

  for (uint32_t i = 0; i < text_length; i++) {
    sysex_data[i + 1] = text[i];  // Copy text into SysEx data
  }
  sysex_data[text_length + 1] = 0xF7;  // End of SysEx

// #ifndef DONT_SEND_SYSEX
  // Call the stream write function with the SysEx message
  return tud_midi_n_stream_write(0, 0, sysex_data, sizeof(sysex_data));
// #endif
}

void send_midi_clock() {
  // Construct the MIDI message
  // MIDI Timing Clock message format: 0xF8
  uint8_t midi_message[1];
  midi_message[0] = 0xF8;  // Timing Clock command

  // Ensure TinyUSB stack is initialized and ready
  if (tud_ready()) {
    // Send the MIDI message
    tud_midi_n_stream_write(0, 0, midi_message, sizeof(midi_message));
  }
#ifdef INCLUDE_ZEPTOMECH
  // uart_write_blocking(UART_ID, midi_message, sizeof(midi_message));
  uart_tx_program_puts_len(pio_tx, sm_tx, midi_message, sizeof(midi_message)); // HWMIDI
#endif
}

void send_midi_start() {
  // Construct the MIDI message
  // MIDI Start message format: 0xFA
  uint8_t midi_message[1];
  midi_message[0] = 0xFA;  // Start command

  // Ensure TinyUSB stack is initialized and ready
  if (tud_ready()) {
    // Send the MIDI message
    tud_midi_n_stream_write(0, 0, midi_message, sizeof(midi_message)); // USBMIDI
  }
#ifdef INCLUDE_ZEPTOMECH
  // uart_write_blocking(UART_ID, midi_message, sizeof(midi_message));
  uart_tx_program_puts_len(pio_tx, sm_tx, midi_message, sizeof(midi_message)); // HWMIDI
#endif
}

void send_midi_stop() {
  // Construct the MIDI message
  // MIDI Stop message format: 0xFC
  uint8_t midi_message[1];
  midi_message[0] = 0xFC;  // Stop command

  // Ensure TinyUSB stack is initialized and ready
  if (tud_ready()) {
    // Send the MIDI message
    tud_midi_n_stream_write(0, 0, midi_message, sizeof(midi_message)); // USBMIDI
  }
#ifdef INCLUDE_ZEPTOMECH
  // uart_write_blocking(UART_ID, midi_message, sizeof(midi_message));
  uart_tx_program_puts_len(pio_tx, sm_tx, midi_message, sizeof(midi_message)); // HWMIDI
#endif
}

void send_midi_note_on(uint8_t note, uint8_t velocity) {
  // MIDI cable number 0, Note On event, channel 1
  uint8_t channel = 0;  // MIDI channels are 0-15

  // Construct the MIDI message
  // MIDI Note On message format: 0x9n, where n is the channel number
  uint8_t midi_message[3];
  midi_message[0] = 0x90 | channel;  // Note On command with channel
  midi_message[1] = note;            // MIDI note number
  midi_message[2] = velocity;        // Note velocity

  // Ensure TinyUSB stack is initialized and ready
  if (tud_ready()) {
    // Send the MIDI message
    tud_midi_n_stream_write(0, 0, midi_message, sizeof(midi_message));
  }
#ifdef INCLUDE_ZEPTOMECH
  // uart_write_blocking(UART_ID, midi_message, sizeof(midi_message));
  uart_tx_program_puts_len(pio_tx, sm_tx, midi_message, sizeof(midi_message)); // HWMIDI
#endif
}

int printf_sysex(const char* format, ...) {
  if (!tud_ready()) {
    return 0;
  }
  va_list args;
  va_start(args, format);
  int text_length = vsnprintf(NULL, 0, format, args);
  va_end(args);

  char text[text_length + 1];  // +1 for null terminator
  va_start(args, format);
  vsnprintf(text, text_length + 1, format, args);
  va_end(args);

  return send_text_as_sysex(text);
}

typedef void (*midi_comm_callback)(uint8_t, uint8_t, uint8_t, uint8_t);

void midi_comm_task(midi_comm_callback callback, callback_int_int midi_note_on,
                    callback_int midi_note_off, callback_void midi_start,
                    callback_void midi_continue, callback_void midi_stop,
                    callback_void midi_timing, callback_uint8_uint8_uint8 midi_control_change) {
  
    uint8_t buff[3]; // buff[4] for tud_midi_packet_read
    uint32_t bytes_read = 0;
    // while (tud_midi_available())
    //     tud_midi_packet_read(buff);
    if (tud_midi_available()){
      bytes_read = tud_midi_stream_read(buff, 3);
    } else {
      return;
    }
    if (bytes_read == 0) {
      return;
    }
    
    uint8_t status = buff[0] & 0xF0;
    uint8_t channel = buff[0] & 0xF;
    uint8_t data1 = buff[1];
    uint8_t data2 = buff[2];
    // uint8_t cable = buff[3];

    switch (status)
    {
    case 0x90: // NOTE_ON
        midi_note_on(data1, data2);
        break;
    case 0x80: // NOTE_OFF
        midi_note_off(data1);
        break;
    case 0xB0: // CONTROL CHANGE
        midi_control_change (channel, data1, data2);
        break;
    case 0xA0: // POLYPHONIC AFTERTOUCH
        break;
    case 0xD0: // CHANNEL AFTERTOUCH/PRESSURE
        break;
    case 0xC0: // PROGRAM_CHANGE
        break;
    case 0xE0: // PITCH_BEND
        break;
    case 0xF0: // SYSEX
        break;
    case 0xF8: // CLOCK
        midi_timing();
        break;
    case 0xFA: // START
        midi_start();
        break;
    case 0xFB: // CONTINUE
        midi_continue();
        break;
    case 0xFC: // STOP
        midi_stop();
        break;
    default:
        break;
    }
    return;
  /*
  uint8_t midi_buffer[3];
  midi_buffer[0] = 0;
  midi_buffer[1] = 0;
  midi_buffer[2] = 0;
  uint32_t bytes_read = 0;
  if (tud_midi_n_available(0, 0)) {
    bytes_read = tud_midi_n_stream_read(0, 0, midi_buffer, 3);
  } else {
    return;
  }
  if (bytes_read == 0) {
    return;
  }
  if (midi_buffer[0] == 0xf8) { // CLOCK
    // timing received
    usb_midi_present = true;
    if (midi_timing != NULL) {
      midi_timing();
    }
    return;
  } else if (midi_buffer[0] == 0xfa) { // START
    // start received
    usb_midi_present = true;
    if (midi_start != NULL) {
      midi_start();
    }
    return;
  } else if (midi_buffer[0] == 0xfb) { // CONTINUE
    // continue received
    usb_midi_present = true;
    if (midi_continue != NULL) {
      midi_continue();
    }
    return;
  } else if (midi_buffer[0] == 0xfc) { // STOP
    // stop received
    usb_midi_present = true;
    if (midi_stop != NULL) {
      midi_stop();
    }
    return;
  } else if (midi_buffer[0] == 0x80 && bytes_read > 1) {
    // note off received
    usb_midi_present = true;
    if (midi_note_off != NULL) {
      midi_note_off(midi_buffer[1]);
    }
    return;
  } else if (midi_buffer[0] == 0x90 && bytes_read > 2) {
    // note on received
    usb_midi_present = true;
    if (midi_note_on != NULL) {
      if (bytes_read == 3) {
        // TODO: for some reason this is not working
        midi_note_on(midi_buffer[1], midi_buffer[2]);
      } else {
        midi_note_on(midi_buffer[1], 0);
      }
    }
    return;
  }
  // for (int i = 0; i < bytes_read; i++) {
  //   printf_sysex("[midi_comm_task] midi_buffer[%d]: %x\n", i,
  //   midi_buffer[i]);
  // }
  if (bytes_read == 3) {
    usb_midi_present = true;
    // Extract the status byte and MIDI channel
    uint8_t status = midi_buffer[0] & 0xF0;
    uint8_t channel = midi_buffer[0] & 0x0F;

    // Extract the note number and velocity
    uint8_t byte2 = midi_buffer[1];
    uint8_t byte3 = midi_buffer[2];

    if (status == 0xb0 ){
      // this is a CC message
      midi_control_change (channel, byte2, byte3);
    }
    //   elseif data[1] & 0xf0 == 0xb0 then
    // msg = {
    //   type = "cc",
    //   cc = data[2],
    //   val = data[3],
    //   ch = data[1] - 0xb0 + 1
    // }

    // if (status == 176 && channel == 0 && byte2 == 0) {
    //   send_text_as_sysex("command=reset");
    //   sleep_ms(10);
    //   reset_usb_boot(0, 0);
    // } else if (status == 176 && channel == 0 && byte2 == 1) {
    //   send_text_as_sysex("version=v6.3.9");
    // }
    if (callback != NULL) {
      callback(status, channel, byte2, byte3);
    }
  }
    */
}
#define printf printf_sysex

#endif
#endif