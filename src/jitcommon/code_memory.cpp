/* code_memory.c -- see code_memory.h for why write and exec are two pointers. */
#include "code_memory.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#if defined(__linux__)
#include <sys/syscall.h>
#endif
#if defined(__APPLE__)
#include <libkern/OSCacheControl.h>
#include <pthread.h>
#endif
#endif

static const char *kStatusNames[] = {"ok", "bad-argument", "no-memory", "no-execute-permission", "not-writable"};
static_assert((int)(sizeof kStatusNames / sizeof kStatusNames[0]) == (int)kJcCodeStatusCount,
              "every JcCodeStatus needs a name");

const char *jc_code_status_name(JcCodeStatus s) {
  if ((unsigned)s >= (unsigned)kJcCodeStatusCount) {
    return "unknown";
  }
  return kStatusNames[(int)s];
}

/*
 * Which mechanism this host permits.
 *
 * RESOLVED BY TRYING, ONCE, NOT BY ASKING WHAT PLATFORM WE COMPILED FOR.
 * Whether an anonymous mapping may become executable is a property of the
 * running kernel's policy -- SELinux on Android, and a hardened Linux
 * configuration on a desktop -- not of the target triple. A build that decides
 * this at compile time is correct on the machine it was built on and wrong on
 * the one it ships to, which is the whole problem.
 */
typedef enum Mechanism {
  kMechUnresolved = 0,
  kMechMprotect, /* anonymous mapping, flipped RW <-> RX in place */
  kMechDualMap,  /* one memfd mapped twice: RW here, RX there */
  kMechMapJit,   /* Apple: one address, per-thread write protection */
  kMechVirtualProtect,
  kMechNone /* nothing worked; every allocation refuses */
} Mechanism;

static Mechanism g_mechanism = kMechUnresolved;

static const char *mechanism_name(Mechanism m) {
  switch (m) {
  case kMechMprotect:
    return "mprotect";
  case kMechDualMap:
    return "dual-mapped memfd";
  case kMechMapJit:
    return "MAP_JIT";
  case kMechVirtualProtect:
    return "VirtualProtect";
  case kMechNone:
    return "none (no executable memory on this host)";
  default:
    return "unresolved";
  }
}

static void say(char *reason, unsigned len, const char *fmt, ...) {
  va_list ap;
  if (!reason || len == 0) {
    return;
  }
  va_start(ap, fmt);
  vsnprintf(reason, len, fmt, ap);
  va_end(ap);
}

static size_t page_size(void) {
#if defined(_WIN32)
  SYSTEM_INFO si;
  GetSystemInfo(&si);
  return (size_t)si.dwPageSize;
#else
  long n = sysconf(_SC_PAGESIZE);
  return n > 0 ? (size_t)n : 4096u;
#endif
}

static size_t round_up_pages(size_t n) {
  size_t p = page_size();
  size_t r = n + p - 1u;
  if (r < n) {
    return 0; /* overflow: refused by the caller, never wrapped */
  }
  return r - (r % p);
}

/* ---- POSIX ---------------------------------------------------------------- */

#if !defined(_WIN32)

#if defined(__linux__)
static int open_memfd(size_t size) {
  int fd;
#if defined(SYS_memfd_create)
  fd = (int)syscall(SYS_memfd_create, "jitcommon-code", 0u);
#else
  fd = -1;
#endif
  if (fd < 0) {
    return -1;
  }
  if (ftruncate(fd, (off_t)size) != 0) {
    close(fd);
    return -1;
  }
  return fd;
}
#endif

/*
 * Probe the host ONCE by actually attempting what a JIT does: map a page, ask
 * for execute permission, and see whether the kernel agrees.
 *
 * The probe deliberately does not execute anything. Proving the page is
 * *usable* is the job of the test suite, which writes a real function into a
 * real region and calls it -- here we only need to know which mechanism to use,
 * and running code from a probe would make a failed probe a crash rather than a
 * report.
 */
static Mechanism resolve_mechanism(void) {
#if defined(__APPLE__) && defined(__aarch64__)
  return kMechMapJit;
#else
#if defined(__linux__)
  {
    /* Prefer dual-mapped memfd on Linux: avoiding mprotect flips on every
       block translation eliminates kernel page-table rewrites and TLB
       shootdowns across the code region during JIT warmup, and matches the
       dual-mapping path used on Android. */
    size_t p = page_size();
    int fd = open_memfd(p);
    if (fd >= 0) {
      void *rx = mmap(NULL, p, PROT_READ | PROT_EXEC, MAP_SHARED, fd, 0);
      if (rx != MAP_FAILED) {
        munmap(rx, p);
        close(fd);
        return kMechDualMap;
      }
      close(fd);
    }
  }
#endif
  size_t p = page_size();
  void *m = mmap(NULL, p, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (m == MAP_FAILED) {
    return kMechNone;
  }
  if (mprotect(m, p, PROT_READ | PROT_EXEC) == 0) {
    munmap(m, p);
    return kMechMprotect;
  }
  munmap(m, p);
  return kMechNone;
#endif
}
#endif /* !_WIN32 */

static Mechanism mechanism(void) {
  if (g_mechanism == kMechUnresolved) {
#if defined(_WIN32)
    g_mechanism = kMechVirtualProtect;
#else
    g_mechanism = resolve_mechanism();
#endif
  }
  return g_mechanism;
}

const char *jc_code_mechanism(void) {
  return mechanism_name(mechanism());
}

int jc_code_select_mechanism(const char *name) {
  if (!name) {
    g_mechanism = kMechUnresolved; /* back to probing */
    return 1;
  }
#if !defined(_WIN32)
  /* AVAILABILITY IS TESTED, NOT ASSUMED. Setting the field for a mechanism this
     host cannot provide would let a test report that it exercised a path it
     never reached, which is worse than not testing it. */
  if (strcmp(name, "mprotect") == 0) {
    size_t p = page_size();
    void *m = mmap(NULL, p, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (m == MAP_FAILED) {
      return 0;
    }
    if (mprotect(m, p, PROT_READ | PROT_EXEC) != 0) {
      munmap(m, p);
      return 0;
    }
    munmap(m, p);
    g_mechanism = kMechMprotect;
    return 1;
  }
#if defined(__linux__)
  if (strcmp(name, "dual-mapped memfd") == 0) {
    size_t p = page_size();
    int fd = open_memfd(p);
    void *rx;
    if (fd < 0) {
      return 0;
    }
    rx = mmap(NULL, p, PROT_READ | PROT_EXEC, MAP_SHARED, fd, 0);
    if (rx == MAP_FAILED) {
      close(fd);
      return 0;
    }
    munmap(rx, p);
    close(fd);
    g_mechanism = kMechDualMap;
    return 1;
  }
#endif
#endif
  (void)name;
  return 0;
}

/* ---- creation ------------------------------------------------------------- */

JcCodeStatus jc_code_region_create(size_t size, JcCodeRegion *out, char *reason, unsigned reason_len) {
  size_t bytes;
  Mechanism m;
  if (!out || size == 0) {
    say(reason, reason_len, "a code region needs a destination and a non-zero size");
    return kJcCodeBadArgument;
  }
  memset(out, 0, sizeof *out);
  bytes = round_up_pages(size);
  if (bytes == 0) {
    say(reason, reason_len, "size %zu overflows when rounded to pages", size);
    return kJcCodeBadArgument;
  }
  m = mechanism();

#if defined(_WIN32)
  {
    void *p = VirtualAlloc(NULL, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!p) {
      say(reason, reason_len, "VirtualAlloc of %zu bytes failed (error %lu)", bytes, (unsigned long)GetLastError());
      return kJcCodeNoMemory;
    }
    out->write = (unsigned char *)p;
    out->exec = (unsigned char *)p;
    out->size = bytes;
    out->writable = 1;
    out->mechanism = (int)kMechVirtualProtect;
    return kJcCodeOk;
  }
#else
  if (m == kMechNone) {
    /* REFUSED BY NAME. Returning writable memory here would turn a policy
       decision into a jump into a non-executable page much later. */
    say(reason,
        reason_len,
        "this host permits no executable memory: anonymous mprotect(PROT_EXEC) was refused (%s)"
#if defined(__linux__)
        " and a dual-mapped memfd could not be made executable either"
#endif
        ,
        strerror(errno));
    return kJcCodeNoExecutePermission;
  }

#if defined(__APPLE__) && defined(__aarch64__)
  if (m == kMechMapJit) {
    void *p = mmap(NULL, bytes, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS | MAP_JIT, -1, 0);
    if (p == MAP_FAILED) {
      say(reason,
          reason_len,
          "MAP_JIT mapping of %zu bytes failed (%s); the process needs the "
          "com.apple.security.cs.allow-jit entitlement",
          bytes,
          strerror(errno));
      return kJcCodeNoExecutePermission;
    }
    out->write = (unsigned char *)p;
    out->exec = (unsigned char *)p;
    out->size = bytes;
    out->writable = 1;
    out->mechanism = (int)kMechMapJit;
    pthread_jit_write_protect_np(0);
    return kJcCodeOk;
  }
#endif

  if (m == kMechMprotect) {
    void *p = mmap(NULL, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED) {
      say(reason, reason_len, "mmap of %zu bytes failed (%s)", bytes, strerror(errno));
      return kJcCodeNoMemory;
    }
    out->write = (unsigned char *)p;
    out->exec = (unsigned char *)p;
    out->size = bytes;
    out->writable = 1;
    out->mechanism = (int)kMechMprotect;
    return kJcCodeOk;
  }

#if defined(__linux__)
  if (m == kMechDualMap) {
    int fd = open_memfd(bytes);
    void *rw;
    void *rx;
    if (fd < 0) {
      say(reason, reason_len, "memfd_create/ftruncate for %zu bytes failed (%s)", bytes, strerror(errno));
      return kJcCodeNoMemory;
    }
    rw = mmap(NULL, bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (rw == MAP_FAILED) {
      close(fd);
      say(reason, reason_len, "writable mapping of the code memfd failed (%s)", strerror(errno));
      return kJcCodeNoMemory;
    }
    rx = mmap(NULL, bytes, PROT_READ | PROT_EXEC, MAP_SHARED, fd, 0);
    if (rx == MAP_FAILED) {
      munmap(rw, bytes);
      close(fd);
      say(reason, reason_len, "executable mapping of the code memfd failed (%s)", strerror(errno));
      return kJcCodeNoExecutePermission;
    }
    /* The fd may be closed: both mappings keep the object alive, and holding
       it open would leak one descriptor per region. */
    close(fd);
    out->write = (unsigned char *)rw;
    out->exec = (unsigned char *)rx;
    out->size = bytes;
    out->writable = 1;
    out->mechanism = (int)kMechDualMap;
    return kJcCodeOk;
  }
#endif

  say(reason, reason_len, "no code-memory mechanism resolved for this host");
  return kJcCodeNoExecutePermission;
#endif /* _WIN32 */
}

void jc_code_region_destroy(JcCodeRegion *r) {
  if (!r || !r->write) {
    return; /* safe on a zeroed region, so teardown needs no null checks */
  }
#if defined(_WIN32)
  VirtualFree(r->write, 0, MEM_RELEASE);
#else
  if (r->exec != r->write) {
    munmap(r->exec, r->size);
  }
  munmap(r->write, r->size);
#endif
  memset(r, 0, sizeof *r);
}

/* ---- publishing ----------------------------------------------------------- */

JcCodeStatus jc_code_publish(JcCodeRegion *r, size_t bytes_written) {
  return jc_code_publish_range(r, 0, bytes_written);
}

JcCodeStatus jc_code_publish_range(JcCodeRegion *r, size_t offset, size_t bytes_written) {
  if (!r || !r->write) {
    return kJcCodeBadArgument;
  }
  if (offset > r->size || bytes_written > r->size - offset) {
    /* REFUSED, not clamped. A caller that believes it wrote more than it
       reserved has already run off the end of something. */
    return kJcCodeBadArgument;
  }

#if defined(_WIN32)
  {
    DWORD old = 0;
    if (!VirtualProtect(r->write, r->size, PAGE_EXECUTE_READ, &old)) {
      return kJcCodeNoExecutePermission;
    }
    FlushInstructionCache(GetCurrentProcess(), r->exec + offset, bytes_written);
  }
#else
#if defined(__APPLE__) && defined(__aarch64__)
  if (r->mechanism == (int)kMechMapJit) {
    pthread_jit_write_protect_np(1);
    sys_icache_invalidate(r->exec + offset, bytes_written);
    r->writable = 0;
    return kJcCodeOk;
  }
#endif
  /* Only an in-place mapping is flipped. A dual-mapped region's exec view was
     created executable and its write view must STAY writable -- mprotecting
     either is wrong, and on the host that needs dual mapping, refused. */
  if (r->mechanism == (int)kMechMprotect) {
    if (mprotect(r->write, r->size, PROT_READ | PROT_EXEC) != 0) {
      return kJcCodeNoExecutePermission;
    }
  }
  /*
   * THE FLUSH IS UNCONDITIONAL AND HAPPENS HERE.
   *
   * On x86-64 it is a no-op the compiler discards, which is exactly why it must
   * not be left to callers: a JIT developed on x86-64 with no flush works
   * perfectly and then executes stale cache lines on ARM64, intermittently,
   * depending on cache pressure. Doing it in the one function that every
   * emitter must call makes forgetting it impossible.
   *
   * The flushed range is the EXEC mapping. Under dual mapping the bytes were
   * written through a different virtual address, and flushing that one leaves
   * the address actually being executed stale.
   */
  __builtin___clear_cache((char *)r->exec + offset, (char *)r->exec + offset + bytes_written);
#endif
  r->writable = 0;
  return kJcCodeOk;
}

JcCodeStatus jc_code_begin_write(JcCodeRegion *r) {
  if (!r || !r->write) {
    return kJcCodeBadArgument;
  }
#if defined(__APPLE__) && defined(__aarch64__)
  if (r->mechanism == (int)kMechMapJit) {
    /* Another region's publish may have protected this thread since this
       region became writable. Its local flag cannot describe that state. */
    pthread_jit_write_protect_np(0);
    r->writable = 1;
    return kJcCodeOk;
  }
#endif
  if (r->writable) {
    return kJcCodeOk;
  }
#if defined(_WIN32)
  {
    DWORD old = 0;
    if (!VirtualProtect(r->write, r->size, PAGE_READWRITE, &old)) {
      return kJcCodeNotWritable;
    }
  }
#else
  if (r->mechanism == (int)kMechMprotect) {
    if (mprotect(r->write, r->size, PROT_READ | PROT_WRITE) != 0) {
      return kJcCodeNotWritable;
    }
  }
  /* Under dual mapping the writable view was never made read-only, so there is
     nothing to undo -- which is one of the reasons that mechanism is pleasant
     to patch through. */
#endif
  r->writable = 1;
  return kJcCodeOk;
}
