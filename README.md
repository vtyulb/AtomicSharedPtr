# AtomicSharedPtr
Lock-Free implementation of std::atomic&lt;std::shared_ptr> &amp; several Lock-Free data structures based on it

# Motivation
This project was created as a proof-of-concept for std::atomic&lt;std::shared_ptr>.
In [proposal N4058](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2014/n4058.pdf) Herb Sutter
suggested atomic specialization for shared_ptr. Such specialization gives ability to write Lock-Free algorithms
easily by avoiding [ABA-problem](https://en.wikipedia.org/wiki/ABA_problem). Some languages (java) use
garbage collection, so they never get same pointers, other languages are just too slow to get any
advantage from Lock-Free implementations. However, in C++ we have a lot of problems out of nowhere with
memory handling. All these problems can be evaded with AtomicSharedPtr, which can update it's value
in Lock-Free style and you will never receive same pointers which can break your program.

Current std::atomic&lt;std::shared_ptr> is implemented by using some mutexes (I checked libcxx-10.0.0). It is fast but it gives up
all Lock-Free guarantees this way. I was not satisfied it, so I just implemented Lock-Free version.
It is possible to create AtomicSharedPtr compatible with std::shared_ptr by using same control block,
but mine implementation uses it's own block, because it's a lot easier.

# Other implementations
I currently know about 2 different std::atomic&lt;std::shared_ptr> implementations.
First one is https://bitbucket.org/anthonyw/atomic_shared_ptr/src/default/atomic_shared_ptr
It uses 128-bit Compare-And-Swap (CAS) operations. Not every platform supports such operations
and they are too slow.

Second implementation is from facebook [folly/concurrency](https://github.com/facebook/folly/blob/master/folly/concurrency/AtomicSharedPtr.h)
It uses simple hack. Inside 64-bit pointer there is 16-bit reference counter (refcount).
You can do it as long as your addresses can fit in 48-bit. I don't think there are any
major problems with facebook's implementation except this hack and a fact that
I understood nothing in their code but big comment at the start of the main header.
So, it was just easier to figure out all details by writing my own implementation
(I was mistaken of course).

Same hack is used by me. First 48 bits in my packed structure is a pointer, last 16 bits
are refcount. This way by using fetch_add(1) I can increase some refcount and get pointer
to control block atomically. Global refcount inside control block is required anyway,
because there can be several atomic pointers for the same control block.

# Project structure
- AtomicSharedPtr, SharedPtr, ControlBlock and FastSharedPtr
- Stack, Queue, Map
- FastLogger

AtomicSharedPtr::getFast() -> FastSharedPtr:
- Destruction of AtomicSharedPtr during lifetime of FastSharedPtr is undefined behaviour
- Read is one-time fetch_add
- Destruction is one compare_exchange if nothing changed, one if AtomicSharedPtr
changed. One or more compare_exchanges might be required on active work

AtomicSharedPtr::get() -> SharedPtr:
- No lifetime dependencies
- Read is 2 fetch_add + 1 compare_exchange. One or more CAS might be required on active work
- Destruction if 1 fetch_sub
- Data pointer access is free
- Copying is 1 fetch_add

AtomicSharedPtr::compareExchange():
- This is actually a strong version
- 1 AtomicSharedPtr::getFast() + zero or more {fetch_add + CAS + fetch_sub} + one or more CAS

Stack:
- Push is one or more {AtomicSharedPtr::get() + AtomicSharedPtr::compareExchange}
- Pop is one or more {AtomicSharedPtr::getFast() + AtomicSharedPtr::compareExchange}

Queue:
- Push is AtomicSharedPtr::compareExchange + one or more {AtomicSharedPtr::getFast() + AtomicSharedPtr::get() + AtomicSharedPtr::compareExchange}
- Pop is one or more {AtomicSharedPtr::getFast() + test_and_set() + AtomicSharedPtr::compareExchange}

Map:
- upsert is 1 or more {AtomicSharedPtr::get() + log(N) non-atomic instructions on average}
- remove is 1 or more {AtomicSharedPtr::get() + log(N) non-atomic instructions on average}
- get is 1 AtomicSharedPtr::getFast() + log(N) non-atomic instruction on average

I suggest you to look at [queue](https://github.com/vtyulb/AtomicSharedPtr/blob/master/src/lfqueue.h) and
[stack](https://github.com/vtyulb/AtomicSharedPtr/blob/master/src/lfstack.h) code - life becomes
a lot easier when don't have to worry about memory.

# ABA problem and chain reaction at destruction
AtomicSharedPtr is not affected by ABA problem in any scenario. You can push same control block
to pointer over and over again, nothing bad will happen.

However, if you write your own Lock-Free structs based on AtomicSharedPtr you can encounter chain reaction problem.
For example, if you have stack with 1000000 elements and then you destroy it's top, than top will destroy next
pointer. Next pointer will destroy next one and so on. My implementation uses deferred destruction which is a little slower,
but it won't crash because of stack overflow. There will be a visible lag when whole chain would be destructed,
and there won't be any lag with mutexed std::stack.

# Proof-Of-Work
Code passes thread, memory and address sanitizers while under stress test for 10+ minutes.
There might be a false positive on std::map in memory sanitizer due to some external bug:
[stackoverflow](https://stackoverflow.com/questions/60097307/memory-sanitizer-reports-use-of-uninitialized-value-in-global-object-constructio),
[godbolt](https://godbolt.org/z/pZj6Lm).
Implementation was not tested in any big production yet and not recommended for production use.

# Build
```
git clone https://github.com/vtyulb/AtomicSharedPtr/
cd AtomicSharedPtr
mkdir build && cd build
cmake -DENABLE_FAST_LOGGING=ON -DCMAKE_BUILD_TYPE=Release -DTSAN=OFF -DMSAN=OFF -DASAN=OFF ..
make
./AtomicSharedPtr
```

# Speed
This is sample output with Core i7-6700hq processor. First column is number of operations push/pop divided around 50/50 by rand.
All other columns are time in milliseconds which took the test to finish. LF structs are based on AtomicSharedPtr.
Lockable structs use std::queue/std::stack/std::map and a mutex for synchronizations. Initial map size is 10000,
and most of map operations are reads. Lesser is better.

There are a lot of optimizations still pending.
```
vlad@vtyulb-thinkpad ~/AtomicSharedPtr/build (git)-[master] % ./AtomicSharedPtr
running simple LFMap test...

running correctness LFMap test...
0%  10%  20%  30%  40%  50%  60%  70%  80%  90%  100%  

running LFMap stress test...
        1       2       3       4       5       6       7       8
500000  317     463     469     458     446     540     539     555
1000000 555     708     723     810     768     833     923     1006
1500000 641     1090    1105    1044    1169    1306    1307    1382
2000000 926     1236    1304    1366    1426    1592    1682    1913

running lockable map stress test
        1       2       3       4       5       6       7       8
500000  60      280     202     248     268     308     322     321
1000000 110     571     412     495     543     606     674     641
1500000 162     868     616     793     815     905     945     966
2000000 221     1142    883     1035    1074    1198    1281    1296


running simple LFQueue test...

running LFQueue stress test...
        1       2       3       4       5       6       7       8
500000  262     471     369     341     355     358     384     438
1000000 559     1021    788     770     704     728     783     869
1500000 824     1433    1188    1098    1095    1077    1170    1288
2000000 1112    2040    1629    1424    1449    1438    1557    1761

running lockable queue stress test...
        1       2       3       4       5       6       7       8
500000  25      109     81      110     118     139     150     158
1000000 47      226     157     219     240     289     302     311
1500000 74      341     241     342     351     423     447     474
2000000 97      445     317     452     472     564     604     631

running simple LFStack test...

running LFStack stress test...
        1       2       3       4       5       6       7       8
500000  141     324     357     448     455     576     650     814
1000000 282     613     719     955     970     1142    1339    1639
1500000 423     965     1103    1446    1554    1651    2094    2488
2000000 572     1255    1465    1887    1900    2250    2664    3338

running lockable stack stress test...
        1       2       3       4       5       6       7       8
500000  23      111     78      108     115     138     147     157
1000000 51      220     157     219     232     276     299     312
1500000 71      327     235     331     346     418     444     466
2000000 101     455     310     446     470     554     591     625

./AtomicSharedPtr  425,42s user 145,48s system 407% cpu 2:20,18 total
```

# Debugging with FastLogger
FastLogger is very completed but highly specialized tool. It captures events in a thread_local
ring buffer, which you can view on segfault or abortion, thus understanding what happend.
rdtsc is used to +- synchronize time. I wasted something like 20+ hours on single bug, and
then I wrote FastLogger. After several more hours bug was fixed.

Due to no active synchronization (except rdtsc call) FastLogger is quite fast.
If you run FAST_LOG() 2 times in a row, you would be able to see that it took around
30-50 clock cycles between log entries. Atomic operations take 700-1600 cycles, so
FastLogger's impacts measurement result quite a little. Logs to debug your
crashing once-per-day algorithm are invaluable. It is also very interesting to see
how processor cores bounce across your tasks.

On next motivational screen you can see, that local refcount was moved to global and dropped after CAS. Then thread woke
only to see, that it can't decrease local refcount anymore despite the same pointer address (internal ABA problem, already fixed).
<p>
  <img src="https://raw.githubusercontent.com/vtyulb/AtomicSharedPtr/master/resources/Screenshot_20200523_190342.png">
</p>

Second bug with [heap-use-after-free](https://raw.githubusercontent.com/vtyulb/AtomicSharedPtr/master/resources/00007fffec016880_sample_race_at_destruction)
This one is easier. Thread went to sleep right after CAS (operation 12) at address 00007fffec016880.
It did not increase refcount for threads from which it stole local refcount. Then foreign threads
destroyed their objects decreasing refcount (operation 51) leading to object destruction (operation 100).
Then thread finally woke up just to panic as it wanted to use destroyed object.

# Other things
I also recommend reading simple wait-free queue algorithm by Alex Kogan and Erez Petrank in article
[Wait-Free Queues With Multiple Enqueuers and Dequeuers](http://www.cs.technion.ac.il/~erez/Papers/wfquque-ppopp.pdf).
It looks like their algorithm is not possible to implement without proper garbage collection
(they used java). It even looks that I can't implement it with any available hacks for now.
Some ideas were taken from that algorithm. Continous global refcount updating looks very
much alike thread helping tasks from wait-free queue.
