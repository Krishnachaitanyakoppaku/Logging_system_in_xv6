#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "klog.h"

int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return myproc()->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

// Get kernel log snapshot
int
sys_getklog(void)
{
  int buf_addr;
  int max_entries;
  struct klog_entry *kbuf;
  int count, i;
  struct proc *curproc = myproc();
  
  // Get arguments
  if(argint(0, &buf_addr) < 0)
    return -1;
  if(argint(1, &max_entries) < 0)
    return -1;
  
  if(max_entries <= 0 || max_entries > 1024)
    return -1;
  
  // Check if user buffer is valid
  if(buf_addr < 0 || buf_addr >= curproc->sz)
    return -1;
  if(buf_addr + max_entries * sizeof(struct klog_entry) > curproc->sz)
    return -1;
  
  // Allocate kernel buffer
  kbuf = (struct klog_entry*)kalloc();
  if(kbuf == 0)
    return -1;
  
  // Get snapshot (limited to one page worth)
  int max_fit = PGSIZE / sizeof(struct klog_entry);
  if(max_entries > max_fit)
    max_entries = max_fit;
  
  count = klog_snapshot(kbuf, max_entries);
  
  // Copy to user space
  for(i = 0; i < count; i++){
    if(copyout(curproc->pgdir, (uint)buf_addr + i * sizeof(struct klog_entry),
               &kbuf[i], sizeof(struct klog_entry)) < 0){
      kfree((char*)kbuf);
      return -1;
    }
  }
  
  kfree((char*)kbuf);
  return count;
}
