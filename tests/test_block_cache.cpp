/*
 * test_block_cache -- can it still find everything it kept?
 *
 * The interesting failure in an open-addressed table is not "lookup returns the
 * wrong block". It is a DELETION that breaks the probe chain, after which some
 * unrelated block becomes unreachable -- still present, still counted, simply
 * never found again. The program keeps working; it just retranslates that block
 * forever. Nothing crashes, no assertion fires, and the only visible symptom is
 * that the emulator is slower than it should be.
 *
 * So the central test here is not any single operation. It is a randomised
 * sequence of inserts and invalidations, after every step of which EVERY block
 * that should still be present is looked up and must be found. That is the only
 * shape of test that catches a broken backward shift.
 */
#include "block_cache.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_checks;
static int g_failed;
static int g_test_failed;

#define CHECK(cond)                                                                                                    \
  do {                                                                                                                 \
    g_checks++;                                                                                                        \
    if (!(cond)) {                                                                                                     \
      g_failed++;                                                                                                      \
      printf("    FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                                                       \
    }                                                                                                                  \
  } while (0)

#define CHECK_EQ_U(got, want)                                                                                          \
  do {                                                                                                                 \
    unsigned long long g_ = (unsigned long long)(got);                                                                 \
    unsigned long long w_ = (unsigned long long)(want);                                                                \
    g_checks++;                                                                                                        \
    if (g_ != w_) {                                                                                                    \
      g_failed++;                                                                                                      \
      printf("    FAIL %s:%d: %s: got %llu want %llu\n", __FILE__, __LINE__, #got, g_, w_);                            \
    }                                                                                                                  \
  } while (0)

#define RUN(fn)                                                                                                        \
  do {                                                                                                                 \
    int before = g_failed;                                                                                             \
    printf("test %s\n", #fn);                                                                                          \
    fn();                                                                                                              \
    if (g_failed != before) {                                                                                          \
      g_test_failed++;                                                                                                 \
      printf("  FAIL\n");                                                                                              \
    } else {                                                                                                           \
      printf("  PASS\n");                                                                                              \
    }                                                                                                                  \
  } while (0)

/*
 * A distinct, never-dereferenced host pointer per index.
 *
 * These are real addresses into a real array rather than integers cast to
 * pointers. Casting was simpler and clang-tidy was right to reject it
 * (performance-no-int-to-ptr): a synthesized address is not a pointer the
 * compiler can reason about, and more to the point it is not what the cache
 * stores in production, where every host value is a genuine address inside a
 * JcCodeRegion. Taking real addresses keeps the test's values the same KIND of
 * value the shipping path holds, and costs nothing.
 *
 * Distinctness is the property every assertion here rests on -- two indices
 * sharing a pointer would make a stranded or mis-shifted entry compare equal to
 * the right answer, and the suite would pass while the cache was broken. So an
 * index past the array ABORTS rather than wrapping into an alias.
 */
static unsigned char g_host_space[8192];

static void *fake_host(uint64_t guest) {
  if (guest >= sizeof g_host_space) {
    printf("    FATAL: fake_host(%llu) is past the %zu-byte pool; two indices "
           "would alias and every identity assertion below would go blind\n",
           (unsigned long long)guest,
           sizeof g_host_space);
    abort();
  }
  return &g_host_space[guest];
}

static void test_insert_and_find(void) {
  JcBlockCache *c = jc_block_cache_create(64);
  JcBlockStats s;
  int i;
  CHECK(c != NULL);
  if (!c) {
    return;
  }
  for (i = 0; i < 50; i++) {
    CHECK(jc_block_insert(c, 0x1000u + (JcGuestAddr)i * 16u, fake_host((uint64_t)i), 16));
  }
  CHECK_EQ_U(jc_block_count(c), 50u);
  for (i = 0; i < 50; i++) {
    CHECK(jc_block_lookup(c, 0x1000u + (JcGuestAddr)i * 16u) == fake_host((uint64_t)i));
  }
  /* A miss is a normal outcome, and it is COUNTED -- a cache that never hits
     and one that works are otherwise indistinguishable from the outside. */
  CHECK(jc_block_lookup(c, 0x9999u) == NULL);
  jc_block_stats(c, &s);
  CHECK_EQ_U(s.hits, 50u);
  CHECK_EQ_U(s.misses, 1u);
  CHECK_EQ_U(s.lookups, 51u);
  jc_block_cache_destroy(c);
}

/*
 * THE ONE THAT MATTERS. Insert, invalidate, insert, invalidate -- and after
 * every operation, verify that everything still supposed to be present can be
 * found. A backward shift that moves the wrong entry, or fails to move one,
 * leaves a live block stranded behind a hole in its probe chain.
 *
 * Deterministic: a fixed seed, so a failure is reproducible rather than a
 * story about a run nobody can repeat.
 */
static void test_nothing_is_stranded_by_deletion(void) {
  enum { kSlots = 512 };
  JcBlockCache *c = jc_block_cache_create(kSlots);
  /* present[i] mirrors what the cache should hold, as the thing to compare
     against -- a second, dead-simple model, which is the point. */
  static unsigned char present[kSlots];
  uint64_t rng = 0xDEADBEEFCAFEull;
  int round;
  int i;
  unsigned long verified = 0;

  CHECK(c != NULL);
  if (!c) {
    return;
  }
  memset(present, 0, sizeof present);

  for (round = 0; round < 300; round++) {
    rng = rng * 6364136223846793005ull + 1442695040888963407ull;
    if ((rng >> 60) < 10) {
      /* Invalidate a random range, in guest-address terms. */
      uint64_t lo_i = (rng >> 16) % kSlots;
      uint64_t span = 1u + ((rng >> 32) % 24u);
      uint64_t hi_i = lo_i + span;
      JcGuestAddr lo = 0x2000u + lo_i * 16u;
      JcGuestAddr hi = 0x2000u + hi_i * 16u;
      size_t expect = 0;
      size_t dropped;
      for (i = 0; i < kSlots; i++) {
        /* Block i covers [0x2000+i*16, +16). Overlap with [lo,hi). */
        JcGuestAddr g = 0x2000u + (JcGuestAddr)i * 16u;
        if (present[i] && g < hi && (g + 16u) > lo) {
          expect++;
        }
      }
      dropped = jc_block_invalidate_range(c, lo, hi);
      CHECK_EQ_U(dropped, expect);
      for (i = 0; i < kSlots; i++) {
        JcGuestAddr g = 0x2000u + (JcGuestAddr)i * 16u;
        if (present[i] && g < hi && (g + 16u) > lo) {
          present[i] = 0;
        }
      }
    } else {
      int idx = (int)((rng >> 20) % kSlots);
      if (!present[idx]) {
        if (jc_block_insert(c, 0x2000u + (JcGuestAddr)idx * 16u, fake_host((uint64_t)idx), 16)) {
          present[idx] = 1;
        }
      }
    }

    /* After EVERY operation: everything that should be there, is. */
    for (i = 0; i < kSlots; i++) {
      JcGuestAddr g = 0x2000u + (JcGuestAddr)i * 16u;
      void *got = jc_block_lookup(c, g);
      verified++;
      if (present[i]) {
        if (got != fake_host((uint64_t)i)) {
          g_checks++;
          g_failed++;
          printf("    FAIL round %d: block %d STRANDED (expected %p, got %p)\n", round, i, fake_host((uint64_t)i), got);
          jc_block_cache_destroy(c);
          return;
        }
      } else if (got != NULL) {
        g_checks++;
        g_failed++;
        printf("    FAIL round %d: block %d should be gone but was found\n", round, i);
        jc_block_cache_destroy(c);
        return;
      }
    }
  }
  printf("    %lu lookup(s) verified against the model across 300 rounds\n", verified);
  CHECK(verified > 100000u);
  jc_block_cache_destroy(c);
}

/*
 * COLLIDING addresses, which is the only way the backward shift is reached.
 *
 * Fibonacci hashing sends sequential guest addresses to distinct slots -- which
 * is exactly why it was chosen, and exactly why every other test here leaves
 * the displacement logic untouched. A mutation making the shift relocate
 * entries it must leave alone survived the entire suite, including the 153,600
 * randomised lookups, because no probe chain ever formed.
 *
 * So this builds the chains deliberately, using the SAME published hash the
 * emitter will use, and then deletes out of the middle of them.
 */
static size_t home_of(JcGuestAddr g, uint32_t shift) {
  return (size_t)(jc_block_hash(g) >> shift);
}

static void test_probe_chains_survive_deletion(void) {
  JcBlockCache *c = jc_block_cache_create(64);
  JcBlockTableLayout L;
  enum { kGroup = 6 };
  JcGuestAddr colliding[kGroup];
  int found = 0;
  JcGuestAddr probe;
  size_t target;
  unsigned char alive[kGroup];
  int i;
  int j;

  CHECK(c != NULL);
  if (!c) {
    return;
  }
  jc_block_table_layout(c, &L);

  /* Find kGroup addresses that share a home slot. */
  target = home_of(0x40000u, L.hash_shift);
  colliding[found++] = 0x40000u;
  for (probe = 0x40001u; probe < 0x40000u + 4000000u && found < kGroup; probe++) {
    if (home_of(probe, L.hash_shift) == target) {
      colliding[found++] = probe;
    }
  }
  CHECK_EQ_U(found, kGroup); /* if this fails the rest proves nothing */
  if (found != kGroup) {
    jc_block_cache_destroy(c);
    return;
  }
  printf("    %d addresses sharing home slot %zu -- a real probe chain\n", found, target);

  /* Delete each member in turn, from a freshly built chain, and require every
     survivor to still be reachable. Deleting the FIRST is the interesting one:
     everything behind it must shift back. */
  for (j = 0; j < kGroup; j++) {
    jc_block_flush(c);
    for (i = 0; i < kGroup; i++) {
      CHECK(jc_block_insert(c, colliding[i], fake_host((uint64_t)i), 4));
      alive[i] = 1;
    }
    CHECK_EQ_U(jc_block_invalidate_range(c, colliding[j], colliding[j] + 4u), 1u);
    alive[j] = 0;
    for (i = 0; i < kGroup; i++) {
      void *got = jc_block_lookup(c, colliding[i]);
      if (alive[i]) {
        if (got != fake_host((uint64_t)i)) {
          g_checks++;
          g_failed++;
          printf("    FAIL deleting chain member %d STRANDED member %d\n", j, i);
        } else {
          g_checks++;
        }
      } else {
        CHECK(got == NULL);
      }
    }
  }

  /* And the whole chain removed one at a time, in order, never losing one. */
  jc_block_flush(c);
  for (i = 0; i < kGroup; i++) {
    CHECK(jc_block_insert(c, colliding[i], fake_host((uint64_t)i), 4));
  }
  for (j = 0; j < kGroup; j++) {
    CHECK_EQ_U(jc_block_invalidate_range(c, colliding[j], colliding[j] + 4u), 1u);
    for (i = j + 1; i < kGroup; i++) {
      CHECK(jc_block_lookup(c, colliding[i]) == fake_host((uint64_t)i));
    }
  }
  CHECK_EQ_U(jc_block_count(c), 0u);
  jc_block_cache_destroy(c);
}

/*
 * The case a chain of same-home addresses CANNOT reach.
 *
 * When every entry in a run shares one home slot, "shift it back" is always the
 * right answer, so a rule that always shifts and the correct rule behave
 * identically -- two mutations of the displacement condition survived the
 * chain test above for exactly that reason.
 *
 * The discriminating arrangement is an entry sitting AT its own home, directly
 * behind a deleted slot. It must NOT be moved back: doing so puts it in front
 * of its home, where a probe starting at that home finds an empty slot and
 * declares a miss. The block is still in the table, still counted, and
 * permanently unreachable.
 *
 * The second arrangement is the same thing across the table's wrap, which is a
 * separate branch of the condition and is otherwise never executed at all.
 */
static JcGuestAddr addr_with_home(size_t want, uint32_t shift, JcGuestAddr from) {
  JcGuestAddr g;
  for (g = from; g < from + 8000000u; g++) {
    if (home_of(g, shift) == want) {
      return g;
    }
  }
  return 0; /* caller checks; a zero here would silently weaken the test */
}

static void test_entry_at_its_own_home_is_not_shifted_back(void) {
  JcBlockCache *c = jc_block_cache_create(64);
  JcBlockTableLayout L;
  size_t table;
  int pass;

  CHECK(c != NULL);
  if (!c) {
    return;
  }
  jc_block_table_layout(c, &L);
  table = (size_t)L.mask + 1u;

  /* pass 0: an ordinary adjacent pair. pass 1: the same across the wrap, which
     is the other half of the condition and needs the chain to start in the
     table's last slot. */
  for (pass = 0; pass < 2; pass++) {
    size_t h = (pass == 0) ? 10u : table - 1u;
    size_t next = (h + 1u) & L.mask;
    JcGuestAddr a = addr_with_home(h, L.hash_shift, 0x80000u);
    JcGuestAddr d = addr_with_home(next, L.hash_shift, 0x80000u);

    CHECK(a != 0);
    CHECK(d != 0);
    if (a == 0 || d == 0) {
      continue;
    }
    jc_block_flush(c);
    CHECK(jc_block_insert(c, a, fake_host(1), 4)); /* lands at h */
    CHECK(jc_block_insert(c, d, fake_host(2), 4)); /* lands at its own home */
    CHECK(jc_block_lookup(c, a) == fake_host(1));
    CHECK(jc_block_lookup(c, d) == fake_host(2));

    /* Delete the one in front. `d` sits at its own home and must stay there. */
    CHECK_EQ_U(jc_block_invalidate_range(c, a, a + 4u), 1u);
    CHECK(jc_block_lookup(c, a) == NULL);
    if (jc_block_lookup(c, d) != fake_host(2)) {
      g_checks++;
      g_failed++;
      printf("    FAIL pass %d (slots %zu,%zu): the entry at its own home was STRANDED\n", pass, h, next);
    } else {
      g_checks++;
      printf("    pass %d: slots %zu,%zu -- survivor still reachable\n", pass, h, next);
    }
  }
  jc_block_cache_destroy(c);
}

/*
 * Invalidation is by OVERLAP. A block that starts before the range and runs
 * into it is stale, and the containment test that most implementations write
 * gets this exactly wrong -- correct for every case written by hand, wrong for
 * the one-byte patch in the middle of a block, which is what real
 * self-modifying code does.
 */
static void test_invalidation_is_by_overlap(void) {
  JcBlockCache *c = jc_block_cache_create(16);
  CHECK(c != NULL);
  if (!c) {
    return;
  }
  /* One block covering [0x1000, 0x1040). */
  CHECK(jc_block_insert(c, 0x1000u, fake_host(1), 0x40));

  /* A one-byte write in the MIDDLE. Containment would keep the block. */
  CHECK_EQ_U(jc_block_invalidate_range(c, 0x1020u, 0x1021u), 1u);
  CHECK(jc_block_lookup(c, 0x1000u) == NULL);

  /* A write ending exactly at the block's first byte touches nothing: ranges
     are half-open, and off-by-one here throws away live code every time an
     adjacent page is written. */
  CHECK(jc_block_insert(c, 0x1000u, fake_host(1), 0x40));
  CHECK_EQ_U(jc_block_invalidate_range(c, 0x0F00u, 0x1000u), 0u);
  CHECK(jc_block_lookup(c, 0x1000u) == fake_host(1));

  /* And a write starting at the block's last byte does hit it. */
  CHECK_EQ_U(jc_block_invalidate_range(c, 0x103Fu, 0x1040u), 1u);
  CHECK(jc_block_lookup(c, 0x1000u) == NULL);

  /* Zero dropped is a real answer, distinct from "never called". */
  CHECK_EQ_U(jc_block_invalidate_range(c, 0x5000u, 0x6000u), 0u);
  {
    JcBlockStats s;
    jc_block_stats(c, &s);
    CHECK_EQ_U(s.invalidations, 4u);
    CHECK_EQ_U(s.blocks_invalidated, 2u);
  }
  jc_block_cache_destroy(c);
}

/* A full table REFUSES rather than evicting something a chained branch may
   still point at. */
static void test_full_cache_refuses(void) {
  JcBlockCache *c = jc_block_cache_create(8);
  JcBlockStats s;
  int i;
  int accepted = 0;
  CHECK(c != NULL);
  if (!c) {
    return;
  }
  for (i = 0; i < 40; i++) {
    if (jc_block_insert(c, 0x100u + (JcGuestAddr)i * 4u, fake_host((uint64_t)i), 4)) {
      accepted++;
    }
  }
  CHECK_EQ_U(accepted, 8);
  jc_block_stats(c, &s);
  CHECK_EQ_U(s.insert_refusals, 32u); /* refusals are COUNTED, not silent */

  /* Everything accepted is still findable -- a refusal must not have disturbed
     what was already held. */
  for (i = 0; i < 8; i++) {
    CHECK(jc_block_lookup(c, 0x100u + (JcGuestAddr)i * 4u) == fake_host((uint64_t)i));
  }

  /* Flush is the answer to a full cache, and afterwards it accepts again. */
  jc_block_flush(c);
  CHECK_EQ_U(jc_block_count(c), 0u);
  CHECK(jc_block_insert(c, 0x999u, fake_host(99), 4));
  jc_block_cache_destroy(c);
}

/* Re-inserting the same guest address replaces it: that is retranslation after
   an invalidation, not a duplicate. */
static void test_reinsert_replaces(void) {
  JcBlockCache *c = jc_block_cache_create(16);
  CHECK(c != NULL);
  if (!c) {
    return;
  }
  CHECK(jc_block_insert(c, 0x1000u, fake_host(1), 4));
  CHECK(jc_block_insert(c, 0x1000u, fake_host(2), 8));
  CHECK_EQ_U(jc_block_count(c), 1u);
  CHECK(jc_block_lookup(c, 0x1000u) == fake_host(2));
  jc_block_cache_destroy(c);
}

/*
 * The layout an emitter will use must find the same slot the C code does.
 *
 * This is the drift that has no symptom: emitted code that computes the wrong
 * slot misses every time, falls into the slow path, and the program runs
 * CORRECTLY -- just never taking the fast path anyone thought they built. So
 * the emitted address computation is reproduced here from the published layout
 * and required to agree.
 */
static void test_published_layout_finds_the_same_slot(void) {
  JcBlockCache *c = jc_block_cache_create(256);
  JcBlockTableLayout L;
  int i;
  int agreed = 0;
  int checked = 0;
  CHECK(c != NULL);
  if (!c) {
    return;
  }
  for (i = 0; i < 100; i++) {
    CHECK(jc_block_insert(c, 0x4000u + (JcGuestAddr)i * 32u, fake_host((uint64_t)i), 32));
  }
  jc_block_table_layout(c, &L);
  CHECK(L.entries != NULL);
  CHECK_EQ_U(L.entry_size, sizeof(JcBlockEntry));
  CHECK_EQ_U(L.guest_offset, offsetof(JcBlockEntry, guest));
  CHECK_EQ_U(L.host_offset, offsetof(JcBlockEntry, host));

  for (i = 0; i < 100; i++) {
    JcGuestAddr g = 0x4000u + (JcGuestAddr)i * 32u;
    /* Exactly what emitted code would do: multiply, shift, scale, load. */
    uint64_t slot = (g * L.hash_mult) >> L.hash_shift;
    const unsigned char *base = (const unsigned char *)L.entries;
    const unsigned char *e = base + (size_t)slot * L.entry_size;
    JcGuestAddr key;
    void *host;
    memcpy(&key, e + L.guest_offset, sizeof key);
    memcpy(&host, e + L.host_offset, sizeof host);
    checked++;
    if (key == g) {
      /* A first-slot hit: the emitted fast path would take it, and it must be
         the same answer the C lookup gives. */
      agreed++;
      CHECK(host == jc_block_lookup(c, g));
    }
  }
  /*
   * The DENOMINATOR matters. Some addresses collide and live past their home
   * slot, where emitted code would correctly fall through to the slow path --
   * so not every entry is a first-slot hit. But if almost none were, the
   * inline lookup would be worthless and this test would still "pass" while
   * proving nothing.
   */
  printf("    %d of %d found in their home slot (the emitted fast path's reach)\n", agreed, checked);
  CHECK(agreed > checked / 2);
  jc_block_cache_destroy(c);
}

/* A cache nobody asked anything must not report a perfect hit rate. */
static void test_empty_report_names_its_denominator(void) {
  JcBlockCache *c = jc_block_cache_create(16);
  char buf[256];
  CHECK(c != NULL);
  if (!c) {
    return;
  }
  jc_block_stats_report(c, buf, sizeof buf);
  printf("    %s\n", buf);
  CHECK(strstr(buf, "NO LOOKUPS") != NULL);
  CHECK(strstr(buf, "100") == NULL); /* the misleading number is not printed */

  jc_block_insert(c, 0x10u, fake_host(1), 4);
  jc_block_lookup(c, 0x10u);
  jc_block_lookup(c, 0x20u);
  jc_block_stats_report(c, buf, sizeof buf);
  printf("    %s\n", buf);
  CHECK(strstr(buf, "/2 lookups") != NULL); /* the rate carries its denominator */

  jc_block_stats_report(NULL, buf, sizeof buf);
  CHECK(strstr(buf, "none") != NULL);
  jc_block_cache_destroy(c);
}

static void test_bad_arguments(void) {
  JcBlockStats s;
  CHECK(jc_block_cache_create(0) == NULL);
  CHECK(jc_block_lookup(NULL, 0x10u) == NULL);
  CHECK_EQ_U(jc_block_insert(NULL, 0x10u, fake_host(1), 4), 0u);
  CHECK_EQ_U(jc_block_invalidate_range(NULL, 0, 1), 0u);
  CHECK_EQ_U(jc_block_count(NULL), 0u);
  jc_block_flush(NULL);
  jc_block_cache_destroy(NULL);
  jc_block_stats(NULL, &s);
  CHECK_EQ_U(s.lookups, 0u);

  {
    /* An inverted or empty range drops nothing rather than everything, which
       is the sign error that would silently flush the cache on every call. */
    JcBlockCache *c = jc_block_cache_create(8);
    CHECK(jc_block_insert(c, 0x1000u, fake_host(1), 4));
    CHECK_EQ_U(jc_block_invalidate_range(c, 0x2000u, 0x1000u), 0u);
    CHECK_EQ_U(jc_block_invalidate_range(c, 0x1000u, 0x1000u), 0u);
    CHECK(jc_block_lookup(c, 0x1000u) == fake_host(1));
    /* A null host is refused: a block whose code is nowhere is not a block. */
    CHECK_EQ_U(jc_block_insert(c, 0x2000u, NULL, 4), 0u);
    jc_block_cache_destroy(c);
  }
}

int main(void) {
  RUN(test_insert_and_find);
  RUN(test_reinsert_replaces);
  RUN(test_invalidation_is_by_overlap);
  RUN(test_probe_chains_survive_deletion);
  RUN(test_entry_at_its_own_home_is_not_shifted_back);
  RUN(test_nothing_is_stranded_by_deletion);
  RUN(test_full_cache_refuses);
  RUN(test_published_layout_finds_the_same_slot);
  RUN(test_empty_report_names_its_denominator);
  RUN(test_bad_arguments);
  printf("%d check(s), %d failed, %d failing test(s)\n", g_checks, g_failed, g_test_failed);
  return g_test_failed ? 1 : 0;
}
