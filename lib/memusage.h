#ifdef SIMULATOR
// The simulator counts firmware allocations against the RP2350 heap size
// (see ../zeptomech-sim/hal/sim_prelude.h).
uint32_t getTotalHeap() { return sim_heap_total(); }
uint32_t getFreeHeap() { return sim_heap_total() - sim_heap_used(); }
#else
#include <malloc.h>
uint32_t getTotalHeap() {
  extern char __StackLimit, __bss_end__;

  return &__StackLimit - &__bss_end__;
}

uint32_t getFreeHeap() {
  struct mallinfo m = mallinfo();

  return getTotalHeap() - m.uordblks;
}
#endif
