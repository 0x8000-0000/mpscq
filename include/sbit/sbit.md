Signbit Libraries                                        {#mainpage}
=================

Statistics
----------

High-performance and numerically accurate online statistics, implemented in
sbit::Stats.

Multiple-Producer Single-Consumer Queue
---------------------------------------

This library contains a lock-free multiple-producer single-consumer queue.

The core of the library is the sbit::mpscq::Queue class, aided by the
sbit::mpscq::MessagePool implementation.

The key insight that allows this simple and efficient lock-free
implementation is that pushing a single into a stack is easy using
compare-and-swap, but popping a single element from a queue or a stack
is quite difficult. However, if we consider the case of a single
consumer, there is no reason to pick elements one by one, when we can
atomically "steal" the entire contents of the stack, swapping it with an
empty stack. Transforming the stack into a queue then only requires the
reversing of the link order which can be done in linear time.

Having solved the enqueuing / dequeueing performance problem, the next
bottleneck becomes the allocation and release of the messages passing through
the queue. Most lock-free algorithms in the published literature just waive
away the cost of allocation and releasing, or use complicated hazard lists
to maintain the released block "for a while."

The second key insight for improving the performance is that releasing
a block after its contents was "consumed" is very much similar to
"enqueueing" it on a free list. So we can implement message pools
that are "processors" of free blocks. They maintain an unsynchronized
list of free blocks from which they perform the allocation operations
(unsynchronized, because the pools themselves are the only execution
contexts manipulating them), and a synchronized "recycling queue" where
actual consumers can return the released blocks. Pools allocate from
the unsynchronized list first, and when that is depleted, the contents
of the synchronized list are moved in-bulk to the unsynchronized list.
If both lists are depleted, then a pool has the option of blocking,
allocating more objects directly, or borrowing from an upstream pool.
