// /dev/klog character device implementation
#include "types.h"
#include "defs.h"
#include "param.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"
#include "mmu.h"
#include "proc.h"
#include "klog.h"

// Device state for streaming reads
static struct {
  struct spinlock lock;
  uint last_seq;  // Last sequence number read
} klogdev;

void
klogdev_init(void)
{
  initlock(&klogdev.lock, "klogdev");
  klogdev.last_seq = 0;
  
  // Register device
  devsw[KLOG].read = klogdev_read;
  devsw[KLOG].write = klogdev_write;
}

// Read from /dev/klog
int
klogdev_read(struct inode *ip, char *dst, int n)
{
  struct klog_entry entries[64];
  int count, i, copied = 0;
  
  // Snapshot current logs
  count = klog_snapshot(entries, 64);
  
  // Copy entries to user space
  for(i = 0; i < count && copied + sizeof(struct klog_entry) <= n; i++){
    if(copyout(myproc()->pgdir, (uint)dst + copied, &entries[i], 
               sizeof(struct klog_entry)) < 0)
      return -1;
    copied += sizeof(struct klog_entry);
  }
  
  return copied;
}

// Write to /dev/klog (not supported)
int
klogdev_write(struct inode *ip, char *buf, int n)
{
  return -1;  // Read-only device
}
