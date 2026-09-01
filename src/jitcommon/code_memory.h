/*
 * code_memory.h -- memory a JIT can write to and then execute.
 *
 * Every framework needs this before it can emit a single instruction, and it is
 * shared rather than per-framework because none of what makes it hard is
 * specific to a guest CPU: it is all host and OS policy.
 *
 * THE CENTRAL TRAP IS THAT WRITING AND EXECUTING MAY NOT BE THE SAME ADDRESS.
 * A JIT wants memory that is writable while it emits and executable afterwards.
 * Three platforms refuse that in three different ways:
 *
 *   - Apple Silicon requires MAP_JIT and a PER-THREAD toggle
 *     (pthread_jit_write_protect_np). One address, but the thread that writes
 *     must not be executing, and the thread that executes must not be writing.
 *   - Android's SELinux `execmem` policy and hardened Linux refuse to make an
 *     anonymous mapping executable AT ALL. The way through is to map the same
 *     memfd twice -- once RW, once RX -- so the two ARE different addresses.
 *   - Windows wants VirtualAlloc/VirtualProtect and is the easy case.
 *
 * So the interface hands back TWO pointers. On most hosts they are equal; on a
 * dual-mapped host they are not, and code that assumes one pointer works
 * perfectly on the developer's Linux box and fails on a user's phone. Making
 * them separate fields means that bug cannot be written by accident -- you have
 * to pick one, and picking the wrong one fails immediately and locally rather
 * than at some later jump into a page nobody wrote.
 *
 * THE FAILURE IS LOUD. A region that could not be made executable is refused by
 * name, with the mechanism that was tried. It is never quietly returned as
 * writable memory: that turns a policy refusal into a jump to a non-executable
 * page thousands of instructions later, which is the least debuggable outcome
 * available.
 */
#ifndef JITCOMMON_CODE_MEMORY_H
#define JITCOMMON_CODE_MEMORY_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Outcomes, named. A caller that cannot tell "the kernel refused execute
   permission" from "we asked for too much memory" cannot report anything
   useful, and those two want opposite responses from the user. */
typedef enum JcCodeStatus {
  kJcCodeOk = 0,
  kJcCodeBadArgument,
  kJcCodeNoMemory,
  kJcCodeNoExecutePermission, /* the host's policy refused, not a resource limit */
  kJcCodeNotWritable,         /* publish/unpublish sequencing was violated */
  kJcCodeStatusCount          /* MUST stay last */
} JcCodeStatus;

const char *jc_code_status_name(JcCodeStatus s);

/*
 * A region of code memory.
 *
 * `write` and `exec` are the same address on hosts that permit RW->RX flipping
 * in place, and DIFFERENT addresses under dual mapping. Always emit through
 * `write`; always branch to `exec`. Any address recorded in a block cache, a
 * chained branch, or a backtrace must be an `exec` address, and any fixup
 * applied after the fact must go through `write`.
 *
 * `offset` between them is stated explicitly so a translator that has one
 * pointer can derive the other without keeping the region around.
 */
typedef struct JcCodeRegion {
  unsigned char *write;
  unsigned char *exec;
  size_t size;  /* bytes usable, rounded up to whole pages */
  int writable; /* 1 between create/begin_write and publish */
  /* Which mechanism created this region. OPAQUE to callers -- but recorded,
     because publish and begin_write must act on what the region IS, not infer
     it by testing whether `write` and `exec` happen to be equal. They are equal
     under mprotect and MAP_JIT and differ only under dual mapping, so the
     inference is right until something makes them equal by accident, at which
     point publish quietly mprotects a shared mapping: correct-looking on Linux,
     refused on Android. Found by mutation testing, which is the only reason it
     is written down. */
  int mechanism;
} JcCodeRegion;

/*
 * Reserve `size` bytes of code memory, writable, not yet executable.
 *
 * `reason` receives a human-readable explanation on failure -- which mechanism
 * was attempted and what the OS said -- because "could not allocate executable
 * memory" without naming the policy that refused is a bug report nobody can
 * act on. Pass NULL if you genuinely do not want it.
 */
JcCodeStatus jc_code_region_create(size_t size, JcCodeRegion *out, char *reason, unsigned reason_len);

/* Release it. Safe on a zeroed region, so teardown paths need no null checks. */
void jc_code_region_destroy(JcCodeRegion *r);

/*
 * Make the region executable, and make the instructions actually visible to the
 * instruction fetcher.
 *
 * THE CACHE FLUSH IS NOT OPTIONAL, AND ITS ABSENCE IS INVISIBLE ON x86.
 * x86-64 keeps its instruction cache coherent with stores in hardware, so a JIT
 * developed there works with no flush at all. ARM64 does not: the data written
 * sits in the data cache while the instruction fetcher reads stale memory, and
 * the result is executing whatever was in those pages before -- intermittently,
 * depending on cache pressure. That is the single most confusing class of bug
 * in this whole subsystem, so the flush happens here, unconditionally, where it
 * cannot be forgotten by a caller.
 *
 * `bytes_written` is what to flush; passing more than `size` is refused rather
 * than clamped, because a caller that thinks it wrote more than it reserved has
 * already corrupted something.
 */
JcCodeStatus jc_code_publish(JcCodeRegion *r, size_t bytes_written);

/*
 * Make it writable again, to patch a chained branch or an inline cache.
 *
 * On Apple Silicon this toggles the CALLING THREAD's write protection, so the
 * thread that patches must be the thread that publishes. Cross-thread patching
 * needs its own design and is deliberately not offered here rather than being
 * silently wrong.
 */
JcCodeStatus jc_code_begin_write(JcCodeRegion *r);

/*
 * Which mechanism this build resolved at run time -- "mprotect", "dual-mapped
 * memfd", "MAP_JIT", "VirtualProtect". For run reports: "the JIT worked on my
 * machine" and "the JIT worked using the same mechanism as the user's machine"
 * are different claims, and only one of them is evidence.
 */
const char *jc_code_mechanism(void);

/*
 * Force a mechanism by name, for TESTING.
 *
 * Every host picks exactly one mechanism, which means the others are dead code
 * on that host -- and the one that matters most, dual mapping, is the one a
 * Linux or macOS development machine never selects, because it is chosen only
 * where SELinux refuses anonymous execute. That is Android. Left alone, the
 * Android path would first execute on a user's phone.
 *
 * So the mechanism is selectable, and the suite runs the FULL emit/publish/
 * call/patch sequence through dual mapping on an ordinary Linux box, where
 * memfd makes it available even though it would not be chosen. This is not a
 * test hook bolted onto production behaviour: which mechanism is in use is a
 * legitimate thing for a diagnostic build or a bug report to pin down.
 *
 * Returns 0 if that mechanism is not available on this host, leaving the
 * current selection alone -- a forced mechanism that silently did not take
 * would make the test claim coverage it does not have. Pass NULL to return to
 * automatic probing.
 */
int jc_code_select_mechanism(const char *name);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* JITCOMMON_CODE_MEMORY_H */
