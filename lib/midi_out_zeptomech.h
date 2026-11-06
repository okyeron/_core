// Zeptomech: MidiOut's notes and ccs also go to the TRS jack. midi_out.h
// routes its usb midi writes through midi_out_write_zeptomech.
#ifndef MIDIOUT_ZEPTOMECH_LIB
#define MIDIOUT_ZEPTOMECH_LIB 1

// TRS output of notes and ccs is toggled with a button combo
volatile bool send_trs_cc_note = false;

static inline uint32_t midi_out_write_zeptomech(uint8_t itf, uint8_t cable_num,
                                               uint8_t const *buffer,
                                               uint32_t bufsize) {
  uint32_t written = tud_midi_n_stream_write(itf, cable_num, buffer, bufsize);
  if (send_trs_cc_note) {
    send_midi_trs(buffer, bufsize);
  }
  return written;
}

#endif
