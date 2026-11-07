// Sector cache for sdcard_startup() (SD_BOOT_CACHE, set for Zeptomech).
//
// FatFs keeps a single sector buffer, so the boot scans of the bank folders
// (metadata catalogue, seek maps) read the same directory sectors again for
// every file they look up: about 8400 single-sector reads, 2.4 s, on a card
// with 112 samples. A scan only hits the cache if the whole folder fits, so
// the cache holds SD_BOOT_CACHE_SECTORS (default 16, 8 KB).
//
// main() turns it on only around sdcard_startup(), so playback and the audio
// core read the card exactly as without it. Writes drop the sectors they
// cover. Included by lib/sdio/src/glue.c, which calls these from disk_read,
// disk_write and disk_initialize.
#ifndef SD_BOOT_CACHE_H
#define SD_BOOT_CACHE_H 1

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef SD_BOOT_CACHE_SECTORS
#define SD_BOOT_CACHE_SECTORS 16
#endif
#define SD_BOOT_CACHE_SECTOR_SIZE 512

typedef struct {
  LBA_t sector;
  uint32_t used;
  BYTE pdrv;
  bool valid;
  BYTE data[SD_BOOT_CACHE_SECTOR_SIZE];
} sd_boot_cache_entry;

static sd_boot_cache_entry *sd_boot_cache = NULL;
static uint32_t sd_boot_cache_clock = 0;

// called from main() around sdcard_startup(); the cache is freed when off
void sd_boot_cache_enable(bool on) {
  free(sd_boot_cache);
  sd_boot_cache =
      on ? calloc(SD_BOOT_CACHE_SECTORS, sizeof(sd_boot_cache_entry)) : NULL;
}

static sd_boot_cache_entry *sd_boot_cache_find(BYTE pdrv, LBA_t sector) {
  for (uint16_t i = 0; i < SD_BOOT_CACHE_SECTORS; i++) {
    sd_boot_cache_entry *e = &sd_boot_cache[i];
    if (e->valid && e->pdrv == pdrv && e->sector == sector) {
      return e;
    }
  }
  return NULL;
}

// true when the sector was copied from the cache
static bool sd_boot_cache_read(BYTE pdrv, BYTE *buff, LBA_t sector) {
  if (sd_boot_cache == NULL) return false;
  sd_boot_cache_entry *e = sd_boot_cache_find(pdrv, sector);
  if (e == NULL) return false;
  memcpy(buff, e->data, SD_BOOT_CACHE_SECTOR_SIZE);
  e->used = ++sd_boot_cache_clock;
  return true;
}

// keeps a sector just read from the card, replacing the least recently used
static void sd_boot_cache_store(BYTE pdrv, const BYTE *buff, LBA_t sector) {
  if (sd_boot_cache == NULL) return;
  sd_boot_cache_entry *victim = &sd_boot_cache[0];
  for (uint16_t i = 0; i < SD_BOOT_CACHE_SECTORS; i++) {
    sd_boot_cache_entry *e = &sd_boot_cache[i];
    if (!e->valid) {
      victim = e;
      break;
    }
    if (e->used < victim->used) victim = e;
  }
  victim->sector = sector;
  victim->pdrv = pdrv;
  victim->valid = true;
  victim->used = ++sd_boot_cache_clock;
  memcpy(victim->data, buff, SD_BOOT_CACHE_SECTOR_SIZE);
}

// drops cached copies of sectors that are written
static void sd_boot_cache_invalidate(BYTE pdrv, LBA_t sector, UINT count) {
  if (sd_boot_cache == NULL) return;
  for (uint16_t i = 0; i < SD_BOOT_CACHE_SECTORS; i++) {
    sd_boot_cache_entry *e = &sd_boot_cache[i];
    if (e->valid && e->pdrv == pdrv && e->sector >= sector &&
        e->sector - sector < count) {
      e->valid = false;
    }
  }
}

// drops everything, e.g. when the card is initialized again
static void sd_boot_cache_clear(void) {
  if (sd_boot_cache == NULL) return;
  for (uint16_t i = 0; i < SD_BOOT_CACHE_SECTORS; i++) {
    sd_boot_cache[i].valid = false;
  }
}

#endif
