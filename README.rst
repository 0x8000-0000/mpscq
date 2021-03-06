*********************
* Signbit Libraries *
*********************

Multiple-Producer Single-Consumer Lock-Free Queue
=================================================

This implementation is based on several key insights:

* Implementing lock-free push into a stack is easy.

* Implementing lock-free pop from a stack or a queue is very hard.

* However, if we only allow one single thread to remove elements, it does
  not have to traverse the container - it can "steal" the whole queue contents
  by atomically swapping the head pointer with a variable containing NULL.

* This will give the consumer the elements in a list, but in a reverse order
  (the head points to the element most recently appended). Reversing a linked
  list is a trivial linear operation.

* Making the push and popAll fast is one part of the story. This shifts the
  contention to the allocator, if all the nodes that transit the queue are
  dynamically allocated.

* However, a "release" after the consumer is done with the node back to the
  thread that queued the message can be implemented with a simple "push"
  operation.

Prerequisites
=============

A C++17 conforming compiler. Tested with GCC9/10 and Clang10 on Linux.

Conan package manager (mainly required for Google Test at this point.)

Documentation
=============

`Generated Doxygen documentation <https://0x8000-0000.github.io/mpscq/>`_

License
=======

Copyright 2020 Florin Iucha

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

