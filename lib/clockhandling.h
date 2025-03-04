bool clock_in_did = true;
uint32_t negative_latency_us =
    (uint32_t)roundf(((float)SAMPLES_PER_BUFFER * 1000000.0f) / 44100.0f);
uint32_t negative_latency_us_last_ct = 0;
uint32_t negative_latency_us_next = 0;

void clock_in_do_update() {
#ifdef INCLUDE_ECTOCORE
  if (clock_in_activator < 0) {
#else
  if (clock_in_activator < 1) {
#endif
    clock_in_activator++;
  } else {
    if (clock_in_did) {
      clock_in_did = false;
    } else {
      clock_in_do = true;
      clock_in_last_last_time = clock_in_last_time;
      clock_in_last_time = time_us_32();
      clock_in_beat_total++;
      clock_in_ready = true;
      printf("[clockhandling] %d %d %d", negative_latency_us_last_ct,
             clock_in_last_time, negative_latency_us_last_ct);
    }
  }
  if (playback_stopped) {
    playback_was_stopped_clock = true;
    clock_in_beat_total = 0;
  } else if (playback_was_stopped_clock) {
    playback_was_stopped_clock = false;
    clock_in_beat_total = 0;
  }
}

void clock_handling_up(int time_diff) {
  // printf("[clockhandling] clock_handling_up: %d %d\n", time_diff, bpm_new);
  clock_in_diff_2x = time_diff * 2;
  uint16_t bpm_new = round(30000000.0 / (float)(time_diff));
  if (bpm_new > 30 && bpm_new < 300) {
    sf->bpm_tempo = bpm_new;
  }
  clock_in_do_update();
}

void clock_handling_down(int time_diff) {
  // printf("[zeptocore] clock_handling_down: %d\n", time_diff);
}

void clock_handling_start() {
  // printf("[clockhandling] clock_handling_start\n");
#ifdef INCLUDE_ECTOCORE
  if (clock_in_activator < 0) {
#else
  if (clock_in_activator < 1) {
#endif
    clock_in_activator++;
  } else {
    clock_in_do = true;
    clock_in_last_last_time = clock_in_last_time;
    clock_in_last_time = time_us_32();
    clock_in_beat_total = 0;
    clock_in_ready = true;
    cancel_repeating_timer(&timer);
    do_restart_playback = true;
    timer_step();
    update_repeating_timer_to_bpm(sf->bpm_tempo);
  }
}

void clock_handling_every() {
  if (!clock_in_do || clock_in_did || clock_in_ready ||
      clock_in_last_time == 0 || clock_in_diff_2x == 0) {
    return;
  }
  if (negative_latency_us_last_ct == 0) {
    negative_latency_us_last_ct = time_us_32();
    return;
  }
  if (negative_latency_us_next == 0) {
    negative_latency_us_next =
        clock_in_last_time + (clock_in_diff_2x / 2) - negative_latency_us;
  }
  uint32_t now_time = time_us_32();
  if (now_time >= negative_latency_us_next &&
      negative_latency_us_last_ct < negative_latency_us_next) {
    printf("[clockhandling] negative latency: %d\n",
           negative_latency_us_next - now_time);
    negative_latency_us = negative_latency_us_next - now_time;
    negative_latency_us_next = 0;
    negative_latency_us_last_ct = 0;
    clock_in_do = true;
    clock_in_last_last_time = clock_in_last_time;
    clock_in_last_time = time_us_32();
    clock_in_beat_total++;
    clock_in_ready = true;
    clock_in_did = true;
  } else {
    negative_latency_us_last_ct = now_time;
  }
}