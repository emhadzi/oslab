# 1.1, 1.2

time ./simplesync-atomic (local)
```
real    0m0,149s
user    0m0,273s
sys     0m0,002s
```
- user time > real time: Threads running on different cores
- sys time close to 0: No need for syncronization

time ./simplesync-mutex (local)
```
real    0m0,999s
user    0m1,348s
sys     0m0,583s
```

- sys time != 0: Significant amount of time spent in kernel mode for syncronization
- real time mutex > real time atomic: Makes sense
- user + sys = 2*real: Running on 2 cores

time ./simplesync-atomic (oslab)
```
real	0m1,603s
user	0m0,335s
sys	0m0,017s
```

time ./simplesync-mutex (oslab)
real	0m5,641s
user	0m1,088s
sys	0m0,142s

- real > user + sys beacause of resource sharing between multiple users
- user times about the same as in local, makes sense
- sys times a little different: 
  - Nonzero for atomic, maybe some unavoidable overhead to pass control between different users
  - Relatively smaller for mutex: Maybe not as frequent mandatory content switches (timeouts)

# 1.3

For `increase_fn`:

For atomic:
Ctrl + f `.loc 1 51`

```
	movq	%rdi, %rbx
	movl	$10000000, %eax
.L2:
	lock addl	$1, (%rbx)          
	subl	$1, %eax
	jne	.L2
```

Το `lock` prefix σε μία εντολή δηλώνει ότι "κλειδώνει" το cache line της L3 ώστε αν ένας άλλος επεξεργαστής επιχειρήσει να διαβάσει από την ίδια διεύθυνση μνήμης να αναγκαστεί να κάνει stall μέχρι να ξεκλειδώσει

Why is MESI Needed?
In a multiprocessor system:

Each core typically has its own private L1 and L2 caches.
The L3 cache (or last-level cache) is often shared among all cores.
When multiple cores access or modify the same memory location, the system must ensure that all cores see a consistent view of memory.
The MESI protocol ensures that:

Data consistency is maintained across all caches.
Race conditions and stale data are avoided when multiple cores access the same memory.
How the MESI Protocol Works:
The MESI protocol assigns one of four states to each cache line (a block of memory stored in the cache):

Modified (M):

The cache line contains data that has been modified by the core and is different from the value in main memory.
The cache line is dirty (i.e., it must be written back to main memory before being evicted).
The data is exclusive to this cache (no other core has a copy).
Exclusive (E):

The cache line contains data that is identical to the value in main memory.
The data is exclusive to this cache (no other core has a copy).
The core can modify the data without notifying other cores, transitioning the state to Modified (M).
Shared (S):

The cache line contains data that is identical to the value in main memory.
The data is shared among multiple caches (other cores may also have a copy).
The core must notify other cores if it wants to modify the data, transitioning the state to Modified (M).
Invalid (I):

The cache line is invalid and does not contain valid data.
If the core tries to access this cache line, it must fetch the data from another cache or main memory.

On modern CPUs, the lock prefix does not lock the entire memory bus (as it did on older CPUs). Instead, it locks the cache line containing the memory address being accessed.

The CPU ensures that the cache line is in the exclusive state (MESI protocol) for the duration of the operation.

The cache coherence protocol ensures that the other processor cannot access the memory until the lock is released.

The lock prefix ensures that the cache line containing the memory address is in the exclusive state in the MESI protocol.


State Transitions in MESI:
The MESI protocol defines rules for transitioning between these states based on read and write operations by the core and snooping (monitoring) of other cores' operations.

1. Read Operation:
If the cache line is in the Invalid (I) state:
The core fetches the data from another cache or main memory.
The state transitions to Shared (S) (if other cores also have the data) or Exclusive (E) (if no other core has the data).
2. Write Operation:
If the cache line is in the Shared (S) or Exclusive (E) state:
The core invalidates the copies in other caches (via a broadcast message).
The state transitions to Modified (M).
3. Snooping:
Cores monitor the memory bus for operations by other cores (this is called snooping).
If another core writes to a cache line that is in the Shared (S) or Exclusive (E) state, the current core invalidates its copy (transitioning to Invalid (I)).


Example:
Core A Reads Data:

Core A reads a memory location.
If the data is not in its cache, it fetches it from main memory and stores it in the Exclusive (E) state.
Core B Reads the Same Data:

Core B reads the same memory location.
Core A transitions the cache line to the Shared (S) state and shares the data with Core B.
Both cores now have the data in the Shared (S) state.
Core A Writes to the Data:

Core A wants to modify the data.
It sends an invalidate message to Core B, forcing Core B to transition the cache line to the Invalid (I) state.
Core A transitions the cache line to the Modified (M) state.
How MESI Relates to the lock Prefix:
The lock prefix ensures that the cache line containing the memory address is in the Exclusive (E) or Modified (M) state.
If another core tries to access the same cache line during a lock-prefixed operation, it will stall until the operation is complete.
This guarantees atomicity for operations like lock addl $1, (%rbx).
Summary:
The MESI protocol ensures cache coherence in multiprocessor systems by maintaining consistent states for cache lines.
It uses four states: Modified (M), Exclusive (E), Shared (S), and Invalid (I).
The protocol relies on snooping to monitor other cores' operations and enforce state transitions.
The lock prefix works with MESI to ensure atomicity by locking the cache line in the Exclusive (E) or Modified (M) state, preventing other cores from accessing it.





Προφανώς σε περίπτωση ενός πυρήνα (concurrency) δεν υπάρχει πρόβλημα αφου δεν μπορεί να πραγματοποιηθεί context switch στην μέση της εντολής



For mutexes:
Ctrl + f `.loc 1 55`
```
movq	%rdi, %rbp
leaq	lock(%rip), %r12                ; Get the address of lock (&lock) into r12
movl	$10000000, %ebx                 ; Load immediate for remaining loop iterations
.L2:
	movq	%r12, %rdi                  ; Pass it as argument to pthread_mutex_lock
	call	pthread_mutex_lock@PLT      ; PLT for dynamic linking
	movl	0(%rbp), %eax               ; %eax = *ip
	movq	%r12, %rdi                  ; Pass &lock as argument for pthread_mutex_unlock
	addl	$1, %eax                    ; %eax++
	movl	%eax, 0(%rbp)               ; *ip = %eax
	call	pthread_mutex_unlock@PLT     
	subl	$1, %ebx                    
	jne	.L2
```