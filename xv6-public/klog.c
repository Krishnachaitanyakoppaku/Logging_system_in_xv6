// Kernel logging subsystem implementation
#include "types.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "klog.h"

// Per-CPU log buffers
static struct klog_cpu_buf cpu_logs[NCPU];

// Global sequence counter
static uint global_seq = 0;
static struct spinlock seq_lock;

// Get high-resolution timestamp (using TSC)
static void
get_timestamp(uint *hi, uint *lo)
{
  uint tsc_lo, tsc_hi;
  asm volatile("rdtsc" : "=a"(tsc_lo), "=d"(tsc_hi));
  *hi = tsc_hi;
  *lo = tsc_lo;
}

// Get next sequence number
static uint
next_seq(void)
{
  uint seq;
  acquire(&seq_lock);
  seq = global_seq++;
  release(&seq_lock);
  return seq;
}

// Initialize kernel logging subsystem
void
klog_init(void)
{
  int i;
  
  initlock(&seq_lock, "klog_seq");
  
  for(i = 0; i < NCPU; i++){
    initlock(&cpu_logs[i].lock, "klog_cpu");
    cpu_logs[i].head = 0;
    cpu_logs[i].dropped = 0;
  }
  
  klog_printf("klog: logging subsystem initialized");
}

// Helper: format integer
static int
snprintf_int(char *buf, int size, int val)
{
  char tmp[16];
  int i = 0, j = 0;
  int neg = 0;
  
  if(size <= 0)
    return 0;
  
  if(val < 0){
    neg = 1;
    val = -val;
  }
  
  if(val == 0){
    tmp[i++] = '0';
  } else {
    while(val > 0 && i < sizeof(tmp)){
      tmp[i++] = '0' + (val % 10);
      val /= 10;
    }
  }
  
  if(neg && j < size-1)
    buf[j++] = '-';
  
  while(i > 0 && j < size-1)
    buf[j++] = tmp[--i];
  
  buf[j] = 0;
  return j;
}

// Helper: format hex
static int
snprintf_hex(char *buf, int size, uint val)
{
  char tmp[16];
  int i = 0, j = 0;
  static char digits[] = "0123456789abcdef";
  
  if(size <= 2)
    return 0;
  
  buf[j++] = '0';
  buf[j++] = 'x';
  
  if(val == 0){
    tmp[i++] = '0';
  } else {
    while(val > 0 && i < sizeof(tmp)){
      tmp[i++] = digits[val & 0xf];
      val >>= 4;
    }
  }
  
  while(i > 0 && j < size-1)
    buf[j++] = tmp[--i];
  
  buf[j] = 0;
  return j;
}

// Internal logging function with level
static void
klog_printf_internal(int level, const char *fmt, uint *ap)
{
  int cpu_id;
  struct klog_cpu_buf *log;
  struct klog_entry *entry;
  uint idx;
  int pid = 0;
  char buf[64];
  int i;
  char *s;
  
  // Get current CPU
  pushcli();
  cpu_id = cpuid();
  if(cpu_id < 0 || cpu_id >= NCPU){
    popcli();
    return;
  }
  
  log = &cpu_logs[cpu_id];
  
  // Get current process ID if available
  if(myproc())
    pid = myproc()->pid;
  
  // Format the message into buffer
  i = 0;
  for(; *fmt && i < sizeof(buf)-1; fmt++){
    if(*fmt != '%'){
      buf[i++] = *fmt;
      continue;
    }
    fmt++;
    if(*fmt == 0)
      break;
    switch(*fmt){
    case 'd':
      i += snprintf_int(buf + i, sizeof(buf) - i, *ap++);
      break;
    case 'x':
      i += snprintf_hex(buf + i, sizeof(buf) - i, *ap++);
      break;
    case 's':
      s = (char*)*ap++;
      if(s == 0)
        s = "(null)";
      for(; *s && i < sizeof(buf)-1; s++)
        buf[i++] = *s;
      break;
    case '%':
      buf[i++] = '%';
      break;
    default:
      buf[i++] = '%';
      buf[i++] = *fmt;
      break;
    }
  }
  buf[i] = 0;
  
  // Add entry to log buffer
  acquire(&log->lock);
  
  idx = log->head % KLOG_BUF_SIZE;
  entry = &log->entries[idx];
  
  entry->seq = next_seq();
  get_timestamp(&entry->timestamp_hi, &entry->timestamp_lo);
  entry->cpu = cpu_id;
  entry->pid = pid;
  entry->level = level;
  
  // Copy message
  for(i = 0; i < sizeof(entry->msg)-1 && buf[i]; i++)
    entry->msg[i] = buf[i];
  entry->msg[i] = 0;
  
  log->head++;
  
  release(&log->lock);
  popcli();
}

// Log a formatted message (default INFO level)
void
klog_printf(const char *fmt, ...)
{
  uint *ap = (uint*)(void*)&fmt + 1;
  klog_printf_internal(KLOG_INFO, fmt, ap);
}

// Log a formatted message with specific level
void
klog_printf_level(int level, const char *fmt, ...)
{
  uint *ap = (uint*)(void*)&fmt + 1;
  klog_printf_internal(level, fmt, ap);
}

// Snapshot all logs into user buffer
int
klog_snapshot(struct klog_entry *buf, int max_entries)
{
  int cpu_id, count = 0;
  struct klog_cpu_buf *log;
  uint start, end, i, idx, j;
  
  // Collect entries from all CPUs directly into output buffer
  for(cpu_id = 0; cpu_id < NCPU; cpu_id++){
    log = &cpu_logs[cpu_id];
    
    acquire(&log->lock);
    
    // Determine range to copy
    if(log->head > KLOG_BUF_SIZE){
      start = log->head - KLOG_BUF_SIZE;
      end = log->head;
    } else {
      start = 0;
      end = log->head;
    }
    
    // Copy entries directly to output buffer
    for(i = start; i < end && count < max_entries; i++){
      idx = i % KLOG_BUF_SIZE;
      buf[count++] = log->entries[idx];
    }
    
    release(&log->lock);
  }
  
  // Sort by sequence number (simple bubble sort)
  for(i = 0; i < count - 1; i++){
    for(j = 0; j < count - i - 1; j++){
      if(buf[j].seq > buf[j+1].seq){
        struct klog_entry t = buf[j];
        buf[j] = buf[j+1];
        buf[j+1] = t;
      }
    }
  }
  
  return count;
}

// Clear all logs
void
klog_clear(void)
{
  int cpu_id;
  struct klog_cpu_buf *log;
  
  for(cpu_id = 0; cpu_id < NCPU; cpu_id++){
    log = &cpu_logs[cpu_id];
    acquire(&log->lock);
    log->head = 0;
    log->dropped = 0;
    release(&log->lock);
  }
}

// Get total dropped count
uint
klog_get_dropped(void)
{
  int cpu_id;
  uint total = 0;
  struct klog_cpu_buf *log;
  
  for(cpu_id = 0; cpu_id < NCPU; cpu_id++){
    log = &cpu_logs[cpu_id];
    acquire(&log->lock);
    total += log->dropped;
    release(&log->lock);
  }
  
  return total;
}
