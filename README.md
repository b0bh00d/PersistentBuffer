# PersistentBuffer
A high-performance Singleton class provides heap-allocated buffers on
demand.

## Summary
This class is designed to replace direct memory allocations within an
application.  It provides a chunk of heap memory of any size (up to
maximum application quotas).  The two main benefits of the class are

1. Buffers persist in a pool throughout the run of the application
2. Buffers allocated by previous requests are re-used if required sizes fall within the range of an existing buffer

The process of re-using buffers is extremely fast.  Reusing buffers
provides the following benefits:

* Removes the need for additional heap allocations
* Reduces memory fragmentation during a process run
* Reduces CPU cycle overhead by making repeated new/delete calls on the heap.

The included test main.cpp file shows how it might be used, and also
shows statistics of the speed of the class via various methods:

```
     single_buffer(): 167.532 ms
single_buffer_from(): 167.342 ms
   release_buffers(): 405.371 ms

10 buffers were allocated out of 12000000 buffer requests.
```

On Windows, compile with: cl /O2 /EHsc main.cpp PersistentBuffer.cpp

I hope you find this useful.

## Documentation
See the main.cpp module for example usage.
