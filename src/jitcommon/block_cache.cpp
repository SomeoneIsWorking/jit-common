/* block_cache.cpp -- see block_cache.h for why the layout is emittable. */
#include "block_cache.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Fibonacci hashing: multiply by 2^64/phi and take the HIGH bits.
 *
 * The high bits are used because guest addresses are dense and aligned -- every
 * x86 basic block starts at an arbitrary byte, but MIPS and PPC ones are
 * 4-aligned, so the low two bits are always zero and a low-bit mask would send
 * every block on those platforms into a quarter of the table. The multiply
 * spreads that structure upward. It is also two instructions to emit.
 */
uint64_t jc_block_hash(JcGuestAddr guest) {
  return (uint64_t)guest * JC_HASH_MULT;
}

static size_t round_up_pow2(size_t n) {
  size_t p = 1;
  while (p < n) {
    if (p > (SIZE_MAX >> 1)) {
      return 0;
    }
    p <<= 1;
  }
  return p;
}

static unsigned log2_size(size_t n) {
  unsigned s = 0;
  while ((((size_t)1) << s) < n) {
    s++;
  }
  return s;
}

static size_t home_slot(const JcBlockCache *c, JcGuestAddr guest) {
  return (size_t)(jc_block_hash(guest) >> c->shift);
}

JcBlockCache *jc_block_cache_create(size_t capacity) {
  JcBlockCache *c;
  size_t i;
  size_t table;
  if (capacity == 0) {
    return NULL;
  }
  /*
   * Sized well above the requested capacity, and inserts refuse before it
   * fills. Open addressing degrades sharply near full -- and the emitted fast
   * path checks ONE slot, so a long probe chain does not merely slow the C
   * lookup, it turns emitted hits into slow-path calls. Headroom is not
   * generosity here, it is what keeps the inline lookup worth emitting.
   */
  table = round_up_pow2(capacity * 2u);
  if (table == 0 || table < 8u) {
    table = (table == 0) ? 0 : 8u;
  }
  if (table == 0) {
    return NULL;
  }
  c = (JcBlockCache *)calloc(1, sizeof *c);
  if (!c) {
    return NULL;
  }
  c->entries = (JcBlockEntry *)malloc(table * sizeof *c->entries);
  if (!c->entries) {
    free(c);
    return NULL;
  }
  c->capacity = table;
  c->mask = table - 1u;
  c->shift = (unsigned)(64u - log2_size(table));
  c->limit = capacity;
  for (i = 0; i < table; i++) {
    c->entries[i].guest = JC_BLOCK_EMPTY;
    c->entries[i].host = NULL;
    c->entries[i].guest_len = 0;
    c->entries[i].flags = 0;
  }
  return c;
}

void jc_block_cache_destroy(JcBlockCache *c) {
  if (!c) {
    return;
  }
  free(c->entries);
  free(c);
}

void *jc_block_lookup_slow(JcBlockCache *c, JcGuestAddr guest, size_t initial_slot) {
  size_t i = initial_slot;
  uint64_t probes = 1;
  for (;;) {
    probes++;
    i = (i + 1u) & c->mask;
    if (c->entries[i].guest == guest) {
      c->stats.hits++;
      c->stats.probe_length_total += probes;
      if (probes > c->stats.probe_length_max) {
        c->stats.probe_length_max = probes;
      }
      return c->entries[i].host;
    }
    if (c->entries[i].guest == JC_BLOCK_EMPTY || probes >= (uint64_t)c->capacity) {
      c->stats.misses++;
      c->stats.probe_length_total += probes;
      if (probes > c->stats.probe_length_max) {
        c->stats.probe_length_max = probes;
      }
      return NULL;
    }
  }
}

int jc_block_insert(JcBlockCache *c, JcGuestAddr guest, void *host, uint32_t guest_len) {
  size_t i;
  size_t probes;
  if (!c || !host || guest == JC_BLOCK_EMPTY) {
    return 0;
  }
  if (c->count >= c->limit) {
    /* REFUSED, not evicted. Something a chained branch already points at could
       be the victim, and quietly dropping it is corruption rather than
       pressure. The caller's answer to a full cache is to flush. */
    c->stats.insert_refusals++;
    return 0;
  }
  i = home_slot(c, guest);
  for (probes = 0; probes < c->capacity; probes++) {
    if (c->entries[i].guest == JC_BLOCK_EMPTY) {
      c->entries[i].guest = guest;
      c->entries[i].host = host;
      c->entries[i].guest_len = guest_len;
      c->entries[i].flags = 0;
      c->count++;
      c->stats.inserts++;
      return 1;
    }
    if (c->entries[i].guest == guest) {
      /* Retranslation of the same address -- after an invalidation, or two
         threads racing to translate. The newer code wins; the old block's
         memory belongs to the code arena, not to this table. */
      c->entries[i].host = host;
      c->entries[i].guest_len = guest_len;
      c->stats.inserts++;
      return 1;
    }
    i = (i + 1u) & c->mask;
  }
  /* No empty slot anywhere. Refused rather than looping: see the note in
     lookup. */
  c->stats.insert_refusals++;
  return 0;
}

/*
 * Remove the entry at `hole`, then shift back any entry that probed past it.
 *
 * Tombstones would be simpler, and are what most implementations reach for --
 * but they lengthen probe chains permanently, and here that is not just slower:
 * the emitted fast path examines ONE slot, so every extra step of displacement
 * converts an emitted hit into a call into the slow path. A game that patches
 * code steadily would degrade the JIT's hot path over a session with no
 * counter, short of the probe-length ones below, ever showing why.
 */
static void remove_at(JcBlockCache *c, size_t hole) {
  size_t j = hole;
  for (;;) {
    size_t k;
    c->entries[hole].guest = JC_BLOCK_EMPTY;
    c->entries[hole].host = NULL;
    c->entries[hole].guest_len = 0;
    for (;;) {
      j = (j + 1u) & c->mask;
      if (c->entries[j].guest == JC_BLOCK_EMPTY) {
        return;
      }
      k = home_slot(c, c->entries[j].guest);
      /* Can entry j legally live at `hole`? Only if its home does not lie
         strictly between hole and j, cyclically. */
      if (hole <= j ? (hole < k && k <= j) : (hole < k || k <= j)) {
        continue; /* it must stay where it is */
      }
      break;
    }
    c->entries[hole] = c->entries[j];
    hole = j;
  }
}

size_t jc_block_invalidate_range(JcBlockCache *c, JcGuestAddr lo, JcGuestAddr hi) {
  size_t dropped = 0;
  size_t i;
  if (!c || hi <= lo) {
    return 0;
  }
  c->stats.invalidations++;
  /*
   * A full scan. Invalidation is driven by overlay loads, bank switches, DMA
   * into code, and self-modifying code -- all rare next to lookups, which is
   * why this is not indexed by page. That trade is measurable rather than
   * assumed: `invalidations` beside `lookups` in the report is exactly the
   * ratio that would say a title needs a page index, and until one does, a page
   * map maintained across backward-shift moves is a bug farm bought with
   * nothing.
   */
  for (i = 0; i < c->capacity;) {
    JcGuestAddr g = c->entries[i].guest;
    if (g != JC_BLOCK_EMPTY) {
      JcGuestAddr end = g + c->entries[i].guest_len;
      /*
       * OVERLAP, not containment. A block beginning before `lo` and running
       * into the range is stale too. Testing `g >= lo && g < hi` is correct for
       * every hand-written case and wrong for the one-byte patch in the middle
       * of a block, which is what real self-modifying code does.
       */
      if (g < hi && end > lo) {
        remove_at(c, i);
        c->count--;
        dropped++;
        /* remove_at may have shifted an entry INTO slot i, so this slot must be
           examined again rather than stepped over. */
        continue;
      }
    }
    i++;
  }
  c->stats.blocks_invalidated += dropped;
  return dropped;
}

void jc_block_flush(JcBlockCache *c) {
  size_t i;
  if (!c) {
    return;
  }
  for (i = 0; i < c->capacity; i++) {
    c->entries[i].guest = JC_BLOCK_EMPTY;
    c->entries[i].host = NULL;
    c->entries[i].guest_len = 0;
    c->entries[i].flags = 0;
  }
  c->count = 0;
  c->stats.flushes++;
}

size_t jc_block_count(const JcBlockCache *c) {
  return c ? c->count : 0u;
}

void jc_block_stats(const JcBlockCache *c, JcBlockStats *out) {
  if (!out) {
    return;
  }
  if (!c) {
    memset(out, 0, sizeof *out);
    return;
  }
  *out = c->stats;
}

void jc_block_stats_report(const JcBlockCache *c, char *buf, size_t len) {
  JcBlockStats s;
  double hit_rate;
  double mean_probe;
  if (!buf || len == 0) {
    return;
  }
  if (!c) {
    snprintf(buf, len, "block cache: none");
    return;
  }
  s = c->stats;
  if (s.lookups == 0) {
    /*
     * NO LOOKUPS IS ITS OWN REPORT. A cache that was never asked anything and a
     * cache with a perfect hit rate must not print the same line -- "100%" over
     * zero lookups is the single most misleading number this file could
     * produce, and it is what a naive percentage prints.
     */
    snprintf(buf, len, "block cache: %zu block(s) held, NO LOOKUPS -- nothing was asked of it", c->count);
    return;
  }
  hit_rate = 100.0 * (double)s.hits / (double)s.lookups;
  mean_probe = (double)s.probe_length_total / (double)s.lookups;
  snprintf(buf,
           len,
           "block cache: %.1f%% hit (%llu/%llu lookups), %zu held, %llu insert(s), %llu refused, "
           "%llu invalidation(s) dropping %llu block(s), %llu flush(es), probe mean %.2f max %llu",
           hit_rate,
           (unsigned long long)s.hits,
           (unsigned long long)s.lookups,
           c->count,
           (unsigned long long)s.inserts,
           (unsigned long long)s.insert_refusals,
           (unsigned long long)s.invalidations,
           (unsigned long long)s.blocks_invalidated,
           (unsigned long long)s.flushes,
           mean_probe,
           (unsigned long long)s.probe_length_max);
}

void jc_block_table_layout(const JcBlockCache *c, JcBlockTableLayout *out) {
  if (!out) {
    return;
  }
  if (!c) {
    memset(out, 0, sizeof *out);
    return;
  }
  /* Derived from the real structure, never restated. An emitter that hardcodes
     an offset keeps working until this struct changes and then looks in the
     wrong place -- missing every time, silently, while the program still runs
     correctly through the slow path. */
  out->entries = c->entries;
  out->mask = (uint64_t)c->mask;
  out->entry_size = (uint32_t)sizeof(JcBlockEntry);
  out->guest_offset = (uint32_t)offsetof(JcBlockEntry, guest);
  out->host_offset = (uint32_t)offsetof(JcBlockEntry, host);
  out->hash_mult = JC_HASH_MULT;
  out->hash_shift = c->shift;
}
