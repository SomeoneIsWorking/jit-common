/*
 * test_code_memory -- does the memory a JIT emits into actually execute?
 *
 * There is exactly one way to answer that, and it is to write real host
 * machine code into a region, call it, and check what comes back. Everything
 * short of that -- the allocation succeeded, mprotect returned 0, the pointer
 * is non-null -- is satisfied by memory that faults the moment you jump to it.
 *
 * The second program matters as much as the first. Writing a function, calling
 * it, then PATCHING it and calling again is what exercises the instruction-cache
 * flush: on x86-64 the flush is a no-op and the patch is visible regardless, but
 * on ARM64 a missing flush leaves the old instructions in the fetcher and the
 * second call returns the FIRST answer. That is a test which passes on the
 * developer's machine and fails on the user's phone, so it is written here
 * deliberately rather than discovered there.
 */
#include "code_memory.h"

#include <stdio.h>
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
 * A function returning a constant, in host machine code.
 *
 * Written per host rather than compiled, because the point is to have bytes
 * whose meaning we state exactly -- a compiler could return the constant in
 * any number of ways, and then a wrong answer would not tell us whether the
 * memory or the compiler was at fault.
 */
#if defined(__x86_64__) || defined(_M_X64)
#define HAVE_HOST_CODE 1
#define HOST_NAME "x86-64"
static size_t emit_return_constant(unsigned char *p, unsigned value) {
  p[0] = 0xB8; /* MOV EAX, imm32 */
  p[1] = (unsigned char)(value & 0xFFu);
  p[2] = (unsigned char)((value >> 8) & 0xFFu);
  p[3] = (unsigned char)((value >> 16) & 0xFFu);
  p[4] = (unsigned char)((value >> 24) & 0xFFu);
  p[5] = 0xC3; /* RET */
  return 6;
}
#elif defined(__aarch64__) || defined(_M_ARM64)
#define HAVE_HOST_CODE 1
#define HOST_NAME "ARM64"
static size_t emit_return_constant(unsigned char *p, unsigned value) {
  /* MOVZ W0, #imm16 -- the constants used here all fit in 16 bits, which the
     caller must respect; a wider one would need a second MOVK and would make
     this helper lie about what it emitted. */
  unsigned movz = 0x52800000u | ((value & 0xFFFFu) << 5);
  unsigned ret = 0xD65F03C0u;
  memcpy(p, &movz, 4);
  memcpy(p + 4, &ret, 4);
  return 8;
}
#else
#define HAVE_HOST_CODE 0
#define HOST_NAME "unknown"
#endif

typedef unsigned (*ConstFn)(void);

/* The mechanism is reported, not assumed. "The JIT worked here" and "the JIT
   worked using the mechanism the user's machine will pick" are different
   claims, and a run that does not say which one it made is not evidence. */
static void test_mechanism_is_named(void) {
  const char *m = jc_code_mechanism();
  CHECK(m != NULL);
  printf("    mechanism: %s   host: %s\n", m, HOST_NAME);
  /* "unresolved" would mean the probe never ran, which makes every other
     result in this file meaningless. */
  CHECK(strcmp(m, "unresolved") != 0);
}

#if HAVE_HOST_CODE
/* Write a function, call it, get the answer back. The whole subsystem in one
   assertion. */
static void test_emitted_code_actually_runs(void) {
  JcCodeRegion r;
  char reason[256] = {0};
  size_t n;
  ConstFn fn;

  CHECK_EQ_U(jc_code_region_create(4096, &r, reason, sizeof reason), kJcCodeOk);
  if (!r.write) {
    printf("    refused: %s\n", reason);
    return;
  }
  CHECK(r.size >= 4096);
  CHECK_EQ_U(r.writable, 1);

  n = emit_return_constant(r.write, 42);
  CHECK_EQ_U(jc_code_publish(&r, n), kJcCodeOk);
  CHECK_EQ_U(r.writable, 0);

  /* The cast is through a union rather than a direct function-pointer cast,
     which is what C actually permits between object and function pointers. */
  {
    union {
      unsigned char *p;
      ConstFn f;
    } u;
    u.p = r.exec;
    fn = u.f;
  }
  CHECK_EQ_U(fn(), 42u);

  jc_code_region_destroy(&r);
  CHECK(r.write == NULL); /* destroy zeroes, so a double free is a no-op */
}

/*
 * PATCH AND RE-CALL. This is the instruction-cache test, and on x86-64 it
 * passes whether or not the flush exists -- which is exactly why it is here,
 * because it is the same code path that will run on ARM64 where it does not.
 */
static void test_patched_code_is_visible(void) {
  JcCodeRegion r;
  char reason[256] = {0};
  size_t n;
  union {
    unsigned char *p;
    ConstFn f;
  } u;

  CHECK_EQ_U(jc_code_region_create(4096, &r, reason, sizeof reason), kJcCodeOk);
  if (!r.write) {
    printf("    refused: %s\n", reason);
    return;
  }
  n = emit_return_constant(r.write, 42);
  CHECK_EQ_U(jc_code_publish(&r, n), kJcCodeOk);
  u.p = r.exec;
  CHECK_EQ_U(u.f(), 42u);

  /* Now overwrite it with a different constant, exactly as a chained branch or
     an inline-cache patch would. */
  CHECK_EQ_U(jc_code_begin_write(&r), kJcCodeOk);
  CHECK_EQ_U(r.writable, 1);
  n = emit_return_constant(r.write, 4095);
  CHECK_EQ_U(jc_code_publish(&r, n), kJcCodeOk);
  u.p = r.exec;
  CHECK_EQ_U(u.f(), 4095u); /* 42 here means the flush did not happen */

  jc_code_region_destroy(&r);
}

/* Appending and patching away from offset zero must both publish the actual
 * instructions, without destroying code already published in the region. */
static void test_range_publication(void) {
  JcCodeRegion r{};
  char reason[256] = {};
  CHECK_EQ_U(jc_code_region_create(4096, &r, reason, sizeof reason), kJcCodeOk);
  if (!r.write) {
    return;
  }
  union {
    unsigned char *p;
    ConstFn f;
  } first{}, second{};
  first.p = r.exec;
  second.p = r.exec + 256;
  size_t n = emit_return_constant(r.write, 42);
  CHECK_EQ_U(jc_code_publish_range(&r, 0, n), kJcCodeOk);
  CHECK_EQ_U(first.f(), 42u);
  for (unsigned value = 1; value <= 64; value++) {
    CHECK_EQ_U(jc_code_begin_write(&r), kJcCodeOk);
    n = emit_return_constant(r.write + 256, value);
    CHECK_EQ_U(jc_code_publish_range(&r, 256, n), kJcCodeOk);
    CHECK_EQ_U(first.f(), 42u);
    CHECK_EQ_U(second.f(), value);
  }
  CHECK_EQ_U(jc_code_publish_range(&r, r.size + 1, 0), kJcCodeBadArgument);
  CHECK_EQ_U(jc_code_publish_range(&r, r.size, 1), kJcCodeBadArgument);
  CHECK_EQ_U(jc_code_publish_range(&r, 1, (size_t)-1), kJcCodeBadArgument);
  CHECK_EQ_U(jc_code_begin_write(&r), kJcCodeOk);
  CHECK_EQ_U(jc_code_publish_range(&r, r.size, 1), kJcCodeBadArgument);
  CHECK_EQ_U(r.writable, 1);
  CHECK_EQ_U(jc_code_publish_range(&r, r.size, 0), kJcCodeOk);
  CHECK_EQ_U(r.writable, 0);
  CHECK_EQ_U(first.f(), 42u);
  jc_code_region_destroy(&r);
}

/*
 * Two regions, both live at once, each holding a different function. A cache
 * holds thousands of blocks simultaneously, and an allocator that hands back
 * the same pages twice -- or that unmaps one region when another is destroyed --
 * passes every single-region test above.
 */
static void test_regions_are_independent(void) {
  JcCodeRegion a;
  JcCodeRegion b;
  union {
    unsigned char *p;
    ConstFn f;
  } ua;
  union {
    unsigned char *p;
    ConstFn f;
  } ub;

  CHECK_EQ_U(jc_code_region_create(4096, &a, NULL, 0), kJcCodeOk);
  CHECK_EQ_U(jc_code_region_create(4096, &b, NULL, 0), kJcCodeOk);
  if (!a.write || !b.write) {
    return;
  }
  CHECK(a.exec != b.exec);

  CHECK_EQ_U(jc_code_publish(&a, emit_return_constant(a.write, 111)), kJcCodeOk);
  /* MAP_JIT protection belongs to the thread: publishing A closes the write
     window even though B's region-local flag still says writable. */
  CHECK_EQ_U(b.writable, 1);
  CHECK_EQ_U(jc_code_begin_write(&b), kJcCodeOk);
  CHECK_EQ_U(jc_code_publish(&b, emit_return_constant(b.write, 222)), kJcCodeOk);
  ua.p = a.exec;
  ub.p = b.exec;
  CHECK_EQ_U(ua.f(), 111u);
  CHECK_EQ_U(ub.f(), 222u);

  /* Destroying one must not disturb the other. */
  jc_code_region_destroy(&a);
  ub.p = b.exec;
  CHECK_EQ_U(ub.f(), 222u);
  jc_code_region_destroy(&b);
}

/*
 * Under dual mapping the write and exec addresses DIFFER, and this states what
 * is true either way: bytes written through `write` are what `exec` runs. On a
 * host that flips in place the two pointers are equal and this is trivially
 * true; on Android it is the whole mechanism, and a test that only ever used
 * one pointer would not notice them being swapped.
 */
static void test_write_and_exec_views_agree(void) {
  JcCodeRegion r;
  size_t n;
  CHECK_EQ_U(jc_code_region_create(4096, &r, NULL, 0), kJcCodeOk);
  if (!r.write) {
    return;
  }
  n = emit_return_constant(r.write, 1234);
  CHECK_EQ_U(jc_code_publish(&r, n), kJcCodeOk);
  /* Read the bytes back through the EXEC mapping and compare with what was
     written through the WRITE mapping. */
  CHECK(memcmp(r.write, r.exec, n) == 0);
  jc_code_region_destroy(&r);
}
#endif /* HAVE_HOST_CODE */

/* The refusals. A subsystem whose failures are silent hands back memory that
   faults later, somewhere else. */
static void test_bad_requests_are_refused(void) {
  JcCodeRegion r;
  char reason[128] = {0};

  CHECK_EQ_U(jc_code_region_create(0, &r, reason, sizeof reason), kJcCodeBadArgument);
  CHECK(reason[0] != '\0'); /* and it SAYS why */
  CHECK_EQ_U(jc_code_region_create(4096, NULL, NULL, 0), kJcCodeBadArgument);

  /* Publishing more than was reserved is refused rather than clamped: a caller
     that believes it wrote past the end has already corrupted something, and
     clamping would hide it. */
  CHECK_EQ_U(jc_code_region_create(4096, &r, NULL, 0), kJcCodeOk);
  if (r.write) {
    CHECK_EQ_U(jc_code_publish(&r, r.size + 1u), kJcCodeBadArgument);
    jc_code_region_destroy(&r);
  }

  /* Destroy is safe on a zeroed region, so teardown paths need no null checks
     and a double destroy is not a crash. */
  memset(&r, 0, sizeof r);
  CHECK_EQ_U(jc_code_publish_range(NULL, 0, 0), kJcCodeBadArgument);
  CHECK_EQ_U(jc_code_publish_range(&r, 0, 0), kJcCodeBadArgument);
  jc_code_region_destroy(&r);
  jc_code_region_destroy(&r);

  /* Every status has a name; an unnamed refusal cannot be reported. */
  {
    int i;
    for (i = 0; i < (int)kJcCodeStatusCount; i++) {
      CHECK(strcmp(jc_code_status_name((JcCodeStatus)i), "unknown") != 0);
    }
    CHECK(strcmp(jc_code_status_name((JcCodeStatus)kJcCodeStatusCount), "unknown") == 0);
  }
}

/*
 * Create and destroy many regions, and check the process's mapping count comes
 * back. A cache churns thousands of blocks, so a destroy that releases only one
 * of a dual-mapped pair exhausts the mapping limit in a long session -- and
 * nothing else here would notice, because every functional test destroys at
 * most two regions and never looks afterwards. This one exists because a
 * mutation deleting the exec-view munmap survived everything above.
 */
#if defined(__linux__)
static long mapping_count(void) {
  FILE *f = fopen("/proc/self/maps", "r");
  char line[512];
  long n = 0;
  if (!f) {
    return -1;
  }
  while (fgets(line, sizeof line, f)) {
    n++;
  }
  fclose(f);
  return n;
}

static void test_destroy_releases_every_mapping(void) {
  long before;
  long after;
  int i;
  const int rounds = 64;

  (void)jc_code_region_create(4096, NULL, NULL, 0); /* warm any lazy state */
  before = mapping_count();
  if (before < 0) {
    printf("    SKIP -- /proc/self/maps unreadable, so no leak check ran\n");
    return;
  }
  for (i = 0; i < rounds; i++) {
    JcCodeRegion r;
    if (jc_code_region_create(4096, &r, NULL, 0) != kJcCodeOk) {
      CHECK(0);
      return;
    }
    jc_code_region_destroy(&r);
  }
  after = mapping_count();
  printf("    mappings before %ld, after %d create/destroy rounds %ld\n", before, rounds, after);
  /* Exact equality is too strict -- the allocator may legitimately split or
     merge a vma -- but leaking one mapping per round would show as ~64. */
  CHECK(after - before < rounds / 4);
}
#endif

/*
 * Run the whole battery again through EVERY mechanism this host can provide,
 * not merely the one it would choose.
 *
 * On a Linux desktop the probe picks mprotect, so without this the dual-mapping
 * path -- the one Android actually uses, and the only one where `write` and
 * `exec` are different addresses -- would never execute until it reached a
 * phone. memfd is available here, so the identical code can be exercised now.
 * A mechanism this host genuinely cannot provide is reported as not covered
 * rather than skipped silently.
 */
static void run_battery(void) {
#if HAVE_HOST_CODE
  RUN(test_emitted_code_actually_runs);
  RUN(test_patched_code_is_visible);
  RUN(test_range_publication);
  RUN(test_regions_are_independent);
  RUN(test_write_and_exec_views_agree);
#endif
#if defined(__linux__)
  /* Per-mechanism, because the leak it looks for only EXISTS under dual
     mapping: that is the only mechanism with two mappings to release. Run once
     under the host's default it would always pass. */
  RUN(test_destroy_releases_every_mapping);
#endif
}

static void test_every_available_mechanism(void) {
  static const char *const mechanisms[] = {"mprotect", "dual-mapped memfd"};
  const int n = (int)(sizeof mechanisms / sizeof mechanisms[0]);
  int covered = 0;
  int i;
  jc_code_select_mechanism(NULL);
  printf("  -- through default %s:\n", jc_code_mechanism());
  run_battery();
  for (i = 0; i < n; i++) {
    if (!jc_code_select_mechanism(mechanisms[i])) {
      printf("  -- %s: NOT AVAILABLE on this host, so it is UNTESTED here\n", mechanisms[i]);
      continue;
    }
    covered++;
    printf("  -- through %s:\n", mechanisms[i]);
    CHECK(strcmp(jc_code_mechanism(), mechanisms[i]) == 0);
    run_battery();
  }
  jc_code_select_mechanism(NULL);
  /* The denominator. A run that covered one mechanism and a run that covered
     both must not read the same. */
  printf("  -- default mechanism and %d of %d forced mechanism(s) exercised on this host\n", covered, n);
}

int main(void) {
  RUN(test_mechanism_is_named);
  RUN(test_bad_requests_are_refused);
#if HAVE_HOST_CODE
  test_every_available_mechanism();
#else
  printf("test host_code\n  SKIP -- no machine code written for this host, so NOTHING here\n"
         "  proved that emitted code executes. The allocation paths above are all\n"
         "  that ran.\n");
#endif
  printf("%d check(s), %d failed, %d failing test(s)\n", g_checks, g_failed, g_test_failed);
  return g_test_failed ? 1 : 0;
}
