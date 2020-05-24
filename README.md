# AtomicSharedPtr
Lock-Free implementation of std::atomic&lt;std::shared_ptr> &amp; several Lock-Free data structures based on it

# Motivation
This project was created as a proof-of-concept for std::atomic&lt;std::shared_ptr>.
In [proposal N4058](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2014/n4058.pdf) Herb Sutter
suggested atomic specialization for shared_ptr. Such specialization gives ability to write Lock-Free algorithms
easily by avoiding [ABA-problem](https://en.wikipedia.org/wiki/ABA_problem). Some languages (java) use
garbage collection, so they never get same pointers, other languages are just to slow to get any
advantage from Lock-Free implementation. However, in C++ we get a lot of problems out of nowhere with
memory handling. All these problems can be evaded with AtomicSharedPtr, which can update it's value
in Lock-Free style and never same pointers when it can break your program.

Current std::atomic&lt;std::shared_ptr> is implemented by using some mutexes (I checked libcxx-10.0.0). It is fast but it gives up
all Lock-Free guarantees this way. I was not satisfied it, so I just implemented Lock-Free version.
It is possible to create AtomicSharedPtr compatible with std::shared_ptr (by using same control block),
but mine implementation uses it's own block, because it's a lot easier.

# Other implementations
I currently know about 2 different std::atomic&lt;std::shared_ptr> implementations.
First one is https://bitbucket.org/anthonyw/atomic_shared_ptr/src/default/atomic_shared_ptr
It uses 128-bit Compare-And-Swap (CAS) operations. It's not supported on all platforms and it is slow.

Second implementation is from facebook [folly/concurrency](https://github.com/facebook/folly/blob/master/folly/concurrency/AtomicSharedPtr.h)
It uses simple hack. Inside 64-bit pointer there is 16 bit reference counter (refcount).
You can do it as long as your addresses can fit in 48 bit. I don't think there are any
major problems with facebook's implementation except this hack and a fact that
I understood nothing in their code but big comment at the start of the main header.
So, it was just easier to figure out all details by writing my own implementation
(I was mistaken of course).

Same hack is used by me. First 48 bits in my packed structure is pointer, last 16 bits
are refcount. This way by using fetch_add(1) I can increase some refcount and get pointer
to control block atomically. Global refcount inside control block is required anyway,
because there can be several atomic pointers for the same control block.

# Project structure
- AtomicSharedPtr, SharedPtr and ControlBlock
- Stack, Queue
- Map is under construction
- FastLogger

# Proof-Of-Work

# Build
```
git clone https://github.com/vtyulb/AtomicSharedPtr/
cd AtomicSharedPtr
mkdir build && cd build
cmake -DENABLE_FAST_LOGGING=ON -DCMAKE_BUILD_TYPE=Release ..
make
./AtomicSharedPtr
```

# Speed
This is sample output with Core i7-6700hq processor.
```
vlad@vtyulb-thinkpad ~/AtomicSharedPtr/build (git)-[master] % ./AtomicSharedPtr 
running simple LFQueue test...

running lf queue stress test...
        1       2       3       4       5       6       7       8
500000  768     734     572     525     535     514     509     611
1000000 1166    1566    1301    1295    1774    1374    1150    1451
1500000 2285    2274    1956    1658    2499    2414    2745    2630
2000000 3918    3162    2356    2123    2246    2872    2378    2395
2500000 4252    4584    2932    2300    3491    2429    2694    2992
3000000 3645    4047    4442    3646    3095    4181    3886    3937

running lockable queue stress test...
        1       2       3       4       5       6       7       8
500000  29      105     88      109     121     132     146     146
1000000 50      202     162     213     234     269     293     294
1500000 75      343     242     329     348     399     435     445
2000000 102     440     328     422     470     519     523     596
2500000 213     571     439     557     609     699     729     733
3000000 176     673     501     605     711     770     823     897

running simple LFStack test...

running lf stack stress test...
        1       2       3       4       5       6       7       8
500000  331     449     465     534     629     745     824     902
1000000 676     864     1012    1214    1378    1535    1668    1789
1500000 885     1279    1382    1907    2284    2311    2398    2864
2000000 1449    1653    2028    2335    2793    2941    3317    3659
2500000 2136    2145    2386    3056    3492    3900    4326    4821
3000000 2000    3742    3373    3787    4264    4592    4983    5616

running lockable stack stress test...
        1       2       3       4       5       6       7       8
500000  30      122     90      117     105     122     142     150
1000000 59      205     181     206     233     231     289     281
1500000 99      355     251     317     291     322     419     415
2000000 108     426     338     434     457     532     534     571
2500000 129     535     408     553     575     644     706     735
3000000 152     651     491     664     693     799     851     899
./AtomicSharedPtr  807,75s user 126,48s system 362% cpu 4:17,78 tota
```

# Debugging with FastLogger
