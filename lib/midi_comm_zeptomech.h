// Zeptomech version of midi_comm.h (included instead of it): adds hardware
// TRS midi out and lets printf go to usb serial instead of sysex.
// Kept in step with upstream by scripts/sync_zeptomech_copies.sh.
#ifndef LIB_MIDI_COMM_H
#define LIB_MIDI_COMM_H 1
#include <stdarg.h> // Include this header for va_start, va_end, etc.
#include <stdio.h>  // Include for vsnprintf
#include "visualizer_telemetry.h"
#include "midi_receive.h"

#ifdef INCLUDE_ZEPTOMECH
// hardware TRS midi out (pio uart_tx on UART1_TX)
PIO pio_tx = pio2;
uint sm_tx = 0;

void send_midi_trs(const uint8_t *midi_message, uint len)
{
  uart_tx_program_puts_len(pio_tx, sm_tx, (const char *)midi_message, len);
}

// The rest of a usb midi message that did not fit in TinyUSB's fifo.
// TinyUSB keeps a part-written message open (a SysEx without its F7) and
// packs everything written after it into that message, which then reaches
// the host as invalid SysEx. So the rest goes out before anything else.
#define MIDI_USB_PENDING 128
static uint8_t midi_usb_pending[MIDI_USB_PENDING];
static uint16_t midi_usb_pending_len = 0;
static uint16_t midi_usb_pending_pos = 0;

// true when no message is left part-written
static bool midi_usb_flush()
{
  if (midi_usb_pending_pos < midi_usb_pending_len)
  {
    midi_usb_pending_pos += tud_midi_n_stream_write(
        0, 0, midi_usb_pending + midi_usb_pending_pos,
        midi_usb_pending_len - midi_usb_pending_pos);
  }
  if (midi_usb_pending_pos < midi_usb_pending_len)
  {
    return false;
  }
  midi_usb_pending_pos = midi_usb_pending_len = 0;
  return true;
}

// writes a whole message to usb midi; returns 0 (nothing sent) when usb is
// not ready or an earlier message is still going out
uint32_t midi_usb_write(const uint8_t *data, uint32_t len)
{
  if (!tud_ready() || !midi_usb_flush())
  {
    return 0;
  }
  uint32_t n = tud_midi_n_stream_write(0, 0, data, len);
  if (n < len)
  {
    uint32_t rest = len - n;
    if (rest > MIDI_USB_PENDING)
    {
      rest = MIDI_USB_PENDING;
    }
    memcpy(midi_usb_pending, data + n, rest);
    if (data[0] == 0xF0)
    {
      // a cut SysEx still has to end
      midi_usb_pending[rest - 1] = 0xF7;
    }
    midi_usb_pending_len = rest;
    midi_usb_pending_pos = 0;
  }
  return len;
}

// MIDI written from the timer interrupt: clock, start and stop, and notes
// from sequencer playback. TinyUSB builds each usb midi packet from the
// stream a byte at a time, so writing there can land inside a message the
// main loop is writing (a knob cc) and break both into invalid packets; a
// note sent to the TRS jack there would split the cc the same way. These
// messages wait here for midi_irq_service(), which the main loop calls
// between messages.
#define MIDI_IRQ_QUEUE 32
typedef struct
{
  uint8_t len;
  bool trs;
  uint8_t data[3];
} midi_irq_message;
static midi_irq_message midi_irq_queue[MIDI_IRQ_QUEUE];
static volatile uint8_t midi_irq_head = 0;
static volatile uint8_t midi_irq_tail = 0;

static inline bool midi_in_irq()
{
  return __get_current_exception() != 0;
}

static void midi_irq_enqueue(const uint8_t *data, uint32_t len, bool trs)
{
  uint8_t next = (midi_irq_head + 1) % MIDI_IRQ_QUEUE;
  if (next == midi_irq_tail || len > 3)
  {
    return; // full
  }
  midi_irq_message *m = &midi_irq_queue[midi_irq_head];
  m->len = len;
  m->trs = trs;
  memcpy(m->data, data, len);
  __atomic_signal_fence(__ATOMIC_SEQ_CST);
  midi_irq_head = next;
}

void midi_irq_service()
{
  midi_usb_flush();
  while (midi_irq_tail != midi_irq_head)
  {
    __atomic_signal_fence(__ATOMIC_SEQ_CST);
    midi_irq_message *m = &midi_irq_queue[midi_irq_tail];
    if (tud_ready() && midi_usb_write(m->data, m->len) == 0)
    {
      return; // usb fifo full, try again next pass
    }
    if (m->trs)
    {
      send_midi_trs(m->data, m->len);
    }
    midi_irq_tail = (midi_irq_tail + 1) % MIDI_IRQ_QUEUE;
  }
}
#endif

// sysex data bytes must be below 0x80, anything else would end the message
static inline uint8_t sysex_byte(char c)
{
  return (uint8_t)c < 0x80 ? (uint8_t)c : '?';
}

uint32_t send_buffer_as_sysex(char *buffer, uint32_t bufsize)
{
  uint8_t sysex_data[bufsize + 2]; // +2 for SysEx start and end bytes

  sysex_data[0] = 0xF0; // Start of SysEx
  for (uint32_t i = 0; i < bufsize; i++)
  {
    sysex_data[i + 1] = sysex_byte(buffer[i]); // Copy buffer into SysEx data
  }
  sysex_data[bufsize + 1] = 0xF7; // End of SysEx

  return midi_usb_write(sysex_data, sizeof(sysex_data));
}

uint32_t send_text_as_sysex(const char *text)
{
  uint32_t text_length = strlen(text); // Get the length of the text
  uint8_t sysex_data[text_length + 2]; // +2 for SysEx start and end bytes

  sysex_data[0] = 0xF0; // Start of SysEx
  for (uint32_t i = 0; i < text_length; i++)
  {
    sysex_data[i + 1] = sysex_byte(text[i]); // Copy text into SysEx data
  }
  sysex_data[text_length + 1] = 0xF7; // End of SysEx

  // Call the stream write function with the SysEx message
  return midi_usb_write(sysex_data, sizeof(sysex_data));
}

void send_midi_clock()
{
#if ZV_ENABLED
  zv_realtime_enqueue(0xf8);
#elif defined(INCLUDE_ZEPTOMECH)
  // usb only: the trs copy below goes out now
  midi_irq_enqueue((uint8_t[1]){0xf8}, 1, false);
#else
  // Ensure TinyUSB stack is initialized and ready
  if (tud_ready())
  {
    // Construct the MIDI message
    // MIDI Timing Clock message format: 0xF8
    uint8_t midi_message[1];
    midi_message[0] = 0xF8; // Timing Clock command

    // Send the MIDI message
    midi_usb_write(midi_message, sizeof(midi_message));
  }
#endif
#ifdef INCLUDE_ZEPTOMECH
  send_midi_trs((uint8_t[1]){0xF8}, 1);
#endif
}

void send_midi_start()
{
#if ZV_ENABLED
  zv_realtime_enqueue(0xfa);
#elif defined(INCLUDE_ZEPTOMECH)
  // usb only: the trs copy below goes out now
  midi_irq_enqueue((uint8_t[1]){0xfa}, 1, false);
#else
  // Ensure TinyUSB stack is initialized and ready
  if (tud_ready())
  {
    // Construct the MIDI message
    // MIDI Start message format: 0xFA
    uint8_t midi_message[1];
    midi_message[0] = 0xFA; // Start command

    // Send the MIDI message
    midi_usb_write(midi_message, sizeof(midi_message));
  }
#endif
#ifdef INCLUDE_ZEPTOMECH
  send_midi_trs((uint8_t[1]){0xFA}, 1);
#endif
}

void send_midi_stop()
{
#if ZV_ENABLED
  zv_realtime_enqueue(0xfc);
#elif defined(INCLUDE_ZEPTOMECH)
  // usb only: the trs copy below goes out now
  midi_irq_enqueue((uint8_t[1]){0xfc}, 1, false);
#else
  // Ensure TinyUSB stack is initialized and ready
  if (tud_ready())
  {
    // Construct the MIDI message
    // MIDI Stop message format: 0xFC
    uint8_t midi_message[1];
    midi_message[0] = 0xFC; // Stop command

    // Send the MIDI message
    midi_usb_write(midi_message, sizeof(midi_message));
  }
#endif
#ifdef INCLUDE_ZEPTOMECH
  send_midi_trs((uint8_t[1]){0xFC}, 1);
#endif
}

void send_midi_note_on(uint8_t note, uint8_t velocity)
{
  // Ensure TinyUSB stack is initialized and ready
  if (tud_ready())
  {
    // MIDI cable number 0, Note On event, channel 1
    uint8_t channel = 0; // MIDI channels are 0-15

    // Construct the MIDI message
    // MIDI Note On message format: 0x9n, where n is the channel number
    uint8_t midi_message[3];
    midi_message[0] = 0x90 | channel; // Note On command with channel
    midi_message[1] = note;           // MIDI note number
    midi_message[2] = velocity;       // Note velocity

    // Send the MIDI message
    midi_usb_write(midi_message, sizeof(midi_message));
  }
#ifdef INCLUDE_ZEPTOMECH
  send_midi_trs((uint8_t[3]){0x90, note, velocity}, 3);
#endif
}

int printf_sysex(const char *format, ...)
{
  if (!tud_ready())
  {
    return 0;
  }
  va_list args;
  va_start(args, format);
  int text_length = vsnprintf(NULL, 0, format, args);
  va_end(args);

  char text[text_length + 1]; // +1 for null terminator
  va_start(args, format);
  vsnprintf(text, text_length + 1, format, args);
  va_end(args);

  return send_text_as_sysex(text);
}

typedef void (*midi_comm_callback)(uint8_t, uint8_t, uint8_t, uint8_t);

#ifdef INCLUDE_ZEPTOCORE
static char midi_command_buffer[64];
static uint8_t midi_command_length;
static bool midi_command_active;

static void midi_command_reset(void)
{
  midi_command_active = false;
  midi_command_length = 0;
}

static void midi_command_execute(midi_comm_callback callback)
{
  midi_command_buffer[midi_command_length] = 0;
  const char *prefix = "core_cmd=1,";
  if (strncmp(midi_command_buffer, prefix, strlen(prefix)) != 0)
    return;
  const char *operation = midi_command_buffer + strlen(prefix);
  if (strcmp(operation, "hello") == 0)
  {
    send_text_as_sysex("core_caps=1");
  }
  else if (strcmp(operation, "version") == 0)
  {
    send_text_as_sysex("version=v8.1.0");
  }
  else if (strcmp(operation, "bootloader") == 0)
  {
    send_text_as_sysex("command=reset");
    sleep_ms(10);
    reset_usb_boot(0, 0);
  }
  else if (callback != NULL)
  {
    // Only validated SysEx can enter the existing management handlers.
    if (strcmp(operation, "info") == 0)
      callback(0x80, 9, 4, 0);
    else if (strcmp(operation, "slices") == 0)
      callback(0x80, 9, 3, 0);
    else if (strcmp(operation, "view") == 0)
      callback(0x80, 9, 5, 0);
    else if (strncmp(operation, "key,", 4) == 0)
    {
      const char *digits = operation + 4;
      unsigned value = 0, length = 0;
      for (; digits[length]; ++length)
      {
        if (length >= 3 || digits[length] < '0' || digits[length] > '9')
          return;
        value = value * 10 + (unsigned)(digits[length] - '0');
      }
      if (length && value <= 127)
        callback(0x80, 9, (uint8_t)value, 1);
    }
  }
}

static void midi_command_packet(uint8_t cin, const uint8_t *bytes,
                                uint8_t length, midi_comm_callback callback)
{
  // Validate the entire USB event before consuming any of it. CIN 4 continues;
  // CIN 5/6/7 must end in F7. Realtime packets never enter this function.
  bool ending = cin != 4;
  for (uint8_t i = 0; i < length; ++i)
  {
    uint8_t b = bytes[i];
    if ((ending && i == length - 1) ? b != 0xf7
                                    : (b == 0xf0 ? i != 0 : b < 0x20 || b > 0x7e))
    {
      midi_command_reset();
      return;
    }
  }
  for (uint8_t i = 0; i < length; ++i)
  {
    uint8_t b = bytes[i];
    if (b == 0xf0)
    {
      midi_command_reset();
      midi_command_active = true;
    }
    else if (b == 0xf7)
    {
      if (midi_command_active)
        midi_command_execute(callback);
      midi_command_reset();
    }
    else if (midi_command_active)
    {
      if (midi_command_length == sizeof midi_command_buffer - 1)
      {
        midi_command_reset();
        return;
      }
      midi_command_buffer[midi_command_length++] = (char)b;
    }
  }
}
#endif

void midi_comm_task(midi_comm_callback callback, callback_int_int midi_note_on,
                    callback_int midi_note_off, callback_void midi_start,
                    callback_void midi_continue, callback_void midi_stop,
                    callback_void midi_timing,
                    callback_uint8_uint8_uint8 midi_control_change)
{
#ifdef INCLUDE_ZEPTOCORE
  if (!tud_mounted())
  {
    midi_command_reset();
    return;
  }
#endif
  // A stream read can merge a clock byte with part of the following note.
  // Consume one complete USB MIDI event per foreground iteration instead.
  uint8_t packet[4];
  if (!tud_midi_n_packet_read(0, packet) || (packet[0] >> 4) != 0)
  {
    return;
  }
  static const uint8_t packet_lengths[16] = {
      0, 0, 2, 3, 3, 1, 2, 3, 3, 3, 3, 3, 2, 2, 3, 1};
  uint8_t cin = packet[0] & 0x0f;
  uint8_t bytes_read = packet_lengths[cin];
  uint8_t *midi_buffer = packet + 1;
  if (cin >= 4 && cin <= 7)
  {
#ifdef INCLUDE_ZEPTOCORE
    midi_command_packet(cin, midi_buffer, bytes_read, callback);
#endif
    return;
  }
#ifdef INCLUDE_ZEPTOCORE
  // Channel/system-common traffic cancels a partial frame; realtime does not.
  if (cin != 15)
    midi_command_reset();
#endif
  if (!bytes_read)
  {
    return;
  }
  // Channel-voice CINs must agree with the status and contain MIDI data bytes.
  if (cin >= 8 && cin <= 14 &&
      ((midi_buffer[0] >> 4) != cin || midi_buffer[1] >= 0x80 ||
       (bytes_read == 3 && midi_buffer[2] >= 0x80)))
  {
    return;
  }
  if (cin == 15 && midi_buffer[0] == 0xf8)
  {
    // timing received
    usb_midi_present = true;
    if (midi_timing != NULL)
    {
      midi_timing();
    }
    return;
  }
  else if (cin == 15 && midi_buffer[0] == 0xfa)
  {
    // start received
    usb_midi_present = true;
    if (midi_start != NULL)
    {
      midi_start();
    }
    return;
  }
  else if (cin == 15 && midi_buffer[0] == 0xfb)
  {
    // continue received
    usb_midi_present = true;
    if (midi_continue != NULL)
    {
      midi_continue();
    }
    return;
  }
  else if (cin == 15 && midi_buffer[0] == 0xfc)
  {
    // stop received
    usb_midi_present = true;
    if (midi_stop != NULL)
    {
      midi_stop();
    }
    return;
  }
#ifdef INCLUDE_ZEPTOCORE
  if (cin < 8 || cin > 14 || !midi_receive_matches(midi_buffer[0]))
    return;
#endif
  if (cin == 11 && midi_receive_matches(midi_buffer[0]))
  {
    uint8_t channel = midi_buffer[0] & 0xf;
#ifndef INCLUDE_ZEPTOCORE
    if (channel == 0 && midi_buffer[1] == 0)
    {
      send_text_as_sysex("command=reset");
      sleep_ms(10);
      reset_usb_boot(0, 0);
      return;
    }
    else if (channel == 0 && midi_buffer[1] == 1)
    {
      send_text_as_sysex("version=v8.1.0");
      return;
    }
#endif
    // CONTROL CHANGE
    if (midi_control_change != NULL)
    {
      midi_control_change(channel, midi_buffer[1], midi_buffer[2]);
    }
    return;
  }
  else if (cin == 8 && midi_receive_matches(midi_buffer[0]))
  {
    // note off received
    usb_midi_present = true;
    if (midi_note_off != NULL)
    {
      midi_note_off(midi_buffer[1]);
    }
    return;
  }
  else if (cin == 9 && midi_receive_matches(midi_buffer[0]))
  {
    // note on received
    usb_midi_present = true;
    if (midi_buffer[2] == 0)
    {
      if (midi_note_off != NULL)
        midi_note_off(midi_buffer[1]);
    }
    else if (midi_note_on != NULL)
    {
      midi_note_on(midi_buffer[1], midi_buffer[2]);
    }
    return;
  }
#ifndef INCLUDE_ZEPTOCORE
  if (bytes_read == 3)
  {
    usb_midi_present = true;
    // Extract the status byte and MIDI channel
    uint8_t status = midi_buffer[0] & 0xF0;
    uint8_t channel = midi_buffer[0] & 0x0F;

    // Extract the note number and velocity
    uint8_t note = midi_buffer[1];
    uint8_t velocity = midi_buffer[2];
    if (callback != NULL)
    {
      callback(status, channel, note, velocity);
    }
  }
#endif
}

#ifndef DONT_SEND_SYSEX
#define printf printf_sysex
#else
// printf goes to usb serial (stdio_usb shares the port with midi)
#endif
#endif
