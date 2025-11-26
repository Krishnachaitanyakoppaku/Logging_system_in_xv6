// Kernel logging subsystem header
#ifndef KLOG_H
#define KLOG_H

#include "types.h"
#include "spinlock.h"

// Log entry structure
struct klog_entry {
  uint seq;           // Global sequence number
  uint timestamp_hi;  // High 32 bits of timestamp (nanoseconds)
  uint timestamp_lo;  // Low 32 bits of timestamp (nanoseconds)
  uint cpu;           // CPU ID
  uint pid;           // Process ID (0 for kernel)
  uint level;         // Log level (DEBUG, INFO, WARN, ERROR)
  char msg[64];       // Log message
};

#define KLOG_BUF_SIZE 256  // Per-CPU buffer size (must be power of 2)

// Per-CPU log buffer
struct klog_cpu_buf {
  struct spinlock lock;
  struct klog_entry entries[KLOG_BUF_SIZE];
  uint head;          // Next write position
  uint dropped;       // Count of dropped entries due to overflow
};

// Log levels
#define KLOG_DEBUG 0
#define KLOG_INFO  1
#define KLOG_WARN  2
#define KLOG_ERROR 3

// Kernel logging functions
void klog_init(void);
void klog_printf(const char *fmt, ...);
void klog_printf_level(int level, const char *fmt, ...);
int klog_snapshot(struct klog_entry *buf, int max_entries);
void klog_clear(void);
uint klog_get_dropped(void);

// Convenience macros
#define klog_debug(fmt, ...) klog_printf_level(KLOG_DEBUG, fmt, ##__VA_ARGS__)
#define klog_info(fmt, ...)  klog_printf_level(KLOG_INFO, fmt, ##__VA_ARGS__)
#define klog_warn(fmt, ...)  klog_printf_level(KLOG_WARN, fmt, ##__VA_ARGS__)
#define klog_error(fmt, ...) klog_printf_level(KLOG_ERROR, fmt, ##__VA_ARGS__)

#endif // KLOG_H
