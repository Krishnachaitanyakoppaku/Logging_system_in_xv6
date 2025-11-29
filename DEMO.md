# Kernel Logging System


### Setup
```bash
cd xv6-public
make qemu-nox
```

---

## Demo Flow

### 1. Show Initial Boot Logs

**Action:**
```bash
$ ulog_tool
```

**Expected Output:**
```
Kernel Log (3 entries):
----------------------------------------
[0] INFO CPU0 PID0: klog: logging subsystem initialized
[1] INFO CPU0 PID1: exec: sh
[2] INFO CPU0 PID1: fork: pid 1 created child 2
```



---

### 2. Demonstrate File Operations 

**Action:**
```bash
$ ls
$ ulog_tool
```

**Expected Output:**
```
Kernel Log (20+ entries):
----------------------------------------
[0] INFO CPU0 PID0: klog: logging subsystem initialized
[1] INFO CPU0 PID1: exec: sh
[2] INFO CPU0 PID1: fork: pid 1 created child 2
[3] INFO CPU0 PID2: exec: ls
[4] INFO CPU0 PID2: open: . fd=3
[5] INFO CPU0 PID2: open: ./README fd=4
[6] INFO CPU0 PID2: open: ./cat fd=4
... (many file opens)
[20] INFO CPU0 PID2: exit: pid 2 exiting
```

*

---

### 3. Show Error Handling 

**Action:**
```bash
$ cat nonexistent_file
$ ulog_tool
```

**Expected Output:**
```
[X] WARN CPU0 PID3: open: file not found nonexistent_file
```



---

### 4. Trace Complete Workflow

**Action:**
```bash
$ cat README
$ ulog_tool
```

**Expected Output:**
```
[X] INFO CPU0 PID1: fork: pid 1 created child 4
[X+1] INFO CPU0 PID4: exec: cat
[X+2] INFO CPU0 PID4: open: README fd=3
[X+3] DEBUG CPU0 PID4: read: 512 bytes
[X+4] DEBUG CPU0 PID4: read: 512 bytes
[X+5] INFO CPU0 PID4: exit: pid 4 exiting
```



---

### 5. Run Test Suite

**Action:**
```bash
$ klog_test
```

**Expected Output:**
```
=== Kernel Logging System Test ===

Testing getklog() syscall...
Retrieved 25 log entries:
  [0] INFO CPU0 PID0: klog: logging subsystem initialized
  [1] INFO CPU0 PID1: exec: sh
  ...

Testing /dev/klog device...
  [0] INFO CPU0 PID0: klog: logging subsystem initialized
  ...
Read 5 entries from device

=== Test Complete ===
```



---



## Advanced Demo

### Show Multi-Process Activity
```bash
$ cat README & cat README & cat README
$ ulog_tool
```


### Demonstrate Device Interface
```bash
$ mknod klog 2 2
$ cat klog
```


---

