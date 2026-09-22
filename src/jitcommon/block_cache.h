/*
 * block_cache.h -- guest address to translated code.
 *
 * This is what replaces static recompilation's dispatch table, and it is worth
 * being explicit about why it exists at all.
 *
 * A static recompiler must decide, before running anything, which bytes are
 * code. That is undecidable in general, so it guesses and is then seeded by
 * hand wherever the guess fails: computed calls, jump tables, virtual dispatch,
 * overlays, anything reached only through a pointer. `pc/xmen2` measured the
 * cost -- 8,234 instructions it could not translate, a tail its own translator
 * annotates as "embedded data decoded as code", and ~460,000 distinct entry
 * points its level build dispatches through. Every seed is a hand-maintained
 * claim that can go stale, and a missing one is not a build error but a wrong
 * branch at run time.
 *
 * Translating at run time deletes that problem by construction: code is
 * whatever the program branches to, discovered exactly, at the moment it is
 * reached. What it does NOT delete is the lookup -- it moves it onto the hot
 * path, because every indirect branch now asks "do I have a block for this
 * guest address?". In xmen2 that same question, answered by linear search, was
 * the worst measured hotspot in the port (4,592 ms of level load, 500 ms once
 * it became a binary search). Here it will be asked far more often.
 *
 * SO THE LAYOUT IS CHOSEN TO BE EMITTABLE, NOT MERELY FAST IN C. A JIT must be
 * able to inline this lookup into generated code -- a call into a C function
 * per indirect branch is its own tax. Open addressing over a power-of-two table
 * means the emitted sequence is: mask the address, scale by entry size, load,
 * compare, and fall out to a slow path. Roughly ten instructions and no branch
 * to a helper. That is why this is a flat table with a mask rather than a
 * balanced tree or a bucket-chained map, and jc_block_table_layout() publishes
 * the offsets an emitter needs so the two can never disagree.
 *
 * DESIGN THE NEGATIVE FIRST. A cache that never hits and a cache that works are
 * indistinguishable from the outside: both run the program correctly, one just
 * retranslates constantly. So every question this structure is asked is
 * counted, and the counts carry their denominators -- a report saying "1,000
 * hits" is meaningless without the lookups it came from.
 */
#ifndef JITCOMMON_BLOCK_CACHE_H
#define JITCOMMON_BLOCK_CACHE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * A guest address, wide enough for every framework here: MIPS R3000, x86-32 and
 * PPC Gekko are 32-bit, Xenon is 64-bit. One width shared rather than a
 * template, because the cache does not care what the number means -- this repo
 * holds no guest CPU knowledge, and an address is just a key.
 */
typedef uint64_t JcGuestAddr;

/* Nothing is stored at this address by any framework here, so it can mean
   "empty" in the table itself and save a parallel occupancy array -- which
   matters because the emitted lookup has to check it in one compare. */
#define JC_BLOCK_EMPTY ((JcGuestAddr)UINT64_MAX)

typedef struct JcBlockEntry {
  JcGuestAddr guest;  /* JC_BLOCK_EMPTY when this slot is free */
  void *host;         /* the EXEC address of the translated code */
  uint32_t guest_len; /* guest bytes covered, for range invalidation */
  uint32_t flags;     /* JC_BLOCK_GUARDED, or 0 */
  uint64_t _reserved; /* pad to 32 bytes so entries never span cache lines and scale is a power-of-two shift */
} JcBlockEntry;

/* Counters. Every one has a denominator recorded beside it, because a rate is
   the only form in which these numbers say anything. */
typedef struct JcBlockStats {
  uint64_t lookups;
  uint64_t hits;
  uint64_t misses;
  uint64_t inserts;
  uint64_t insert_refusals; /* table full: a REPORTED condition, never silent */
  uint64_t invalidations;   /* calls to invalidate_range */
  uint64_t blocks_invalidated;
  uint64_t flushes;
  uint64_t probe_length_total; /* divided by table lookups: the health of the hashing */
  uint64_t probe_length_max;
  uint64_t front_hits; /* hits answered by the front cache, never probing the table */
  uint64_t guarded;    /* found, but refused to a lookup that refuses guarded blocks */
} JcBlockStats;

/*
 * A block the framework must not enter without asking its consumer first --
 * one at an address the consumer may take over (a host thunk, a native
 * override). The framework's dispatcher looks up with
 * jc_block_lookup_refusing(..., JC_BLOCK_GUARDED) so that only these blocks,
 * and misses, reach the question; every other entry skips it. The front cache
 * never holds a guarded block, so it can answer any lookup.
 */
#define JC_BLOCK_GUARDED 1u

#define JC_HASH_MULT 0x9E3779B97F4A7C15ull

/*
 * THE FRONT CACHE: a small direct-mapped copy of recent (guest, host) pairs,
 * asked before the table.
 *
 * The table's hash scatters on purpose (see block_cache.cpp), so every block a
 * run keeps entering owns its own cache line of a table sized for the whole
 * translation budget -- 4 MB at 65,536 blocks. Measured on an x86 title's
 * gameplay, 52% of the dispatcher's samples sat on the load of the probed
 * slot: one last-level-cache round trip per block entered. The front cache is
 * indexed by address bits instead, so neighbouring blocks share lines and the
 * hot set stays in a 128 KiB array; a conflict is only a fall-through to the
 * table.
 *
 * It holds COPIES, so the invariant is that no pair outlives its table entry:
 * every path that removes or replaces a mapping clears or updates the front
 * slot of that guest address (invalidation, flush, retranslation).
 */
#define JC_BLOCK_FRONT_SLOTS 8192u
/* Low address bits dropped from the index: 2 keeps neighbouring blocks in
   neighbouring slots on every guest ISA (fixed-width ISAs never use them). */
#define JC_BLOCK_FRONT_SHIFT 2u

typedef struct JcBlockFront {
  JcGuestAddr guest; /* JC_BLOCK_EMPTY when this slot is free */
  void *host;
} JcBlockFront;

static inline size_t jc_block_front_slot(JcGuestAddr guest) {
  return (size_t)(guest >> JC_BLOCK_FRONT_SHIFT) & (size_t)(JC_BLOCK_FRONT_SLOTS - 1u);
}

typedef struct JcBlockCache {
  JcBlockEntry *entries;
  JcBlockFront *front; /* JC_BLOCK_FRONT_SLOTS entries; see above */
  size_t capacity;     /* power of two */
  size_t mask;
  unsigned shift; /* 64 - log2(capacity) */
  size_t count;
  size_t limit; /* refuse inserts past this, to bound probe length */
  JcBlockStats stats;
} JcBlockCache;

/*
 * Create a cache holding up to `capacity` blocks. The table is rounded up to a
 * power of two and kept below a load factor, because open addressing degrades
 * sharply when full and the emitted lookup has no way to bail out early.
 */
JcBlockCache *jc_block_cache_create(size_t capacity);
void jc_block_cache_destroy(JcBlockCache *c);

/* Keep a table slot distinct from a guest address at the C ABI boundary. */
typedef struct JcBlockSlot {
  size_t index;
} JcBlockSlot;

/* Slow path for collision probes. */
void *jc_block_lookup_slow(JcBlockCache *c, JcGuestAddr guest, JcBlockSlot initial_slot, uint32_t refuse);

/* A table hit: refused if it carries a flag in `refuse`, else remembered in
   the front cache -- which holds only unflagged blocks -- and returned. */
static inline void *jc_block_take_hit(JcBlockCache *c, JcBlockFront *front, const JcBlockEntry *hit, uint32_t refuse) {
  if (hit->flags & refuse) {
    c->stats.guarded++;
    return NULL;
  }
  c->stats.hits++;
  if (hit->flags == 0u) {
    front->guest = hit->guest;
    front->host = hit->host;
  }
  return hit->host;
}

/*
 * The hot path. Returns the host code address, or NULL if this guest address
 * has not been translated -- which is a NORMAL, COUNTED outcome and the signal
 * to translate, not an error -- or if the block carries a flag in `refuse`.
 */
static inline void *jc_block_lookup_refusing(JcBlockCache *c, JcGuestAddr guest, uint32_t refuse) {
  if (!c || guest == JC_BLOCK_EMPTY) {
    return NULL;
  }
  c->stats.lookups++;
  JcBlockFront *front = &c->front[jc_block_front_slot(guest)];
  if (front->guest == guest) {
    c->stats.hits++;
    c->stats.front_hits++;
    return front->host;
  }
  size_t i = (size_t)(((uint64_t)guest * JC_HASH_MULT) >> c->shift);
  if (c->entries[i].guest == guest) {
    c->stats.probe_length_total++;
    return jc_block_take_hit(c, front, &c->entries[i], refuse);
  }
  if (c->entries[i].guest == JC_BLOCK_EMPTY) {
    c->stats.misses++;
    c->stats.probe_length_total++;
    return NULL;
  }
  JcBlockSlot initial_slot = {i};
  return jc_block_lookup_slow(c, guest, initial_slot, refuse);
}

/* Any block at `guest`, guarded or not. */
static inline void *jc_block_lookup(JcBlockCache *c, JcGuestAddr guest) {
  return jc_block_lookup_refusing(c, guest, 0u);
}

/*
 * Record a translation. `guest_len` is how many guest bytes the block covers,
 * and it is what makes range invalidation possible -- a block is invalidated
 * when a write lands anywhere in [guest, guest+guest_len), not merely on its
 * first byte, which is the case a naive cache gets wrong on self-modifying
 * code.
 *
 * Returns 0 when the table is full. A full cache is REFUSED rather than
 * silently overwriting a live block: the caller's job is then to flush, and a
 * cache that quietly evicted something a chained branch still points at would
 * be corruption, not pressure.
 */
int jc_block_insert(JcBlockCache *c, JcGuestAddr guest, void *host, uint32_t guest_len);

/* Mark the block at `guest` JC_BLOCK_GUARDED. Returns 0 when no block is held
   there. An insert -- including a retranslation -- clears the mark, so a
   caller guards after every insert that needs it. */
int jc_block_guard(JcBlockCache *c, JcGuestAddr guest);

/*
 * Forget every block overlapping [lo, hi). This is what a framework calls on
 * self-modifying code, an overlay load, a bank switch, or DMA into code memory.
 *
 * OVERLAP, NOT CONTAINMENT. A block starting before `lo` and running into the
 * range is invalidated too. Writing the containment test instead is correct for
 * every test anyone writes by hand and wrong for the one-byte patch in the
 * middle of a basic block, which is what a game's own patching actually does.
 *
 * Returns how many blocks were dropped -- zero is a legitimate answer and is
 * reported as such, so "the invalidation did nothing" and "the invalidation was
 * never called" stay distinguishable.
 */
size_t jc_block_invalidate_range(JcBlockCache *c, JcGuestAddr lo, JcGuestAddr hi);

/* Forget everything. The response to a full table, and to any event whose reach
   the framework cannot bound. */
void jc_block_flush(JcBlockCache *c);

/* How many blocks are currently held. */
size_t jc_block_count(const JcBlockCache *c);

/* Read the counters. Never fails; a zeroed struct on a null cache. */
void jc_block_stats(const JcBlockCache *c, JcBlockStats *out);

/*
 * Print a one-line health report to `buf`.
 *
 * It always names the DENOMINATOR. "94% hit rate" and "94% hit rate over 17
 * lookups" are different claims, and a cache reporting the first from the
 * second is how a run that never warmed up gets read as a run that worked.
 */
void jc_block_stats_report(const JcBlockCache *c, char *buf, size_t len);

/*
 * The table's memory layout, so a JIT can emit the lookup inline instead of
 * calling jc_block_lookup.
 *
 * Published from the real structure rather than duplicated in the emitter: an
 * emitter carrying its own idea of the entry size or the guest-address offset
 * is a silent corruption the first time this struct changes, and nothing would
 * catch it. The test suite asserts the emitted-shape invariants hold.
 */
typedef struct JcBlockTableLayout {
  const void *entries;   /* base of the entry array */
  uint64_t mask;         /* index = (guest * MULT >> SHIFT) & mask */
  uint32_t entry_size;   /* stride between entries, in bytes */
  uint32_t guest_offset; /* byte offset of the guest address within an entry */
  uint32_t host_offset;  /* byte offset of the host pointer within an entry */
  uint64_t hash_mult;    /* the multiply the emitted code must reproduce */
  uint32_t hash_shift;
} JcBlockTableLayout;

void jc_block_table_layout(const JcBlockCache *c, JcBlockTableLayout *out);

/*
 * The hash, exposed so an emitter and the C implementation cannot drift.
 *
 * There is one definition and both callers use it. A JIT that emits a hash
 * "equivalent to" this one is a second source of truth, and the failure mode is
 * that emitted code looks in the wrong slot, misses every time, and the program
 * still runs -- correctly, and slowly, with no error anywhere.
 */
uint64_t jc_block_hash(JcGuestAddr guest);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* JITCOMMON_BLOCK_CACHE_H */
