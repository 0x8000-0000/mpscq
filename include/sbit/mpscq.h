/**
 * @file sbit/mpsc.h
 * @brief Multiple-Producer-Single-Consumer Queue
 * @copyright 2020 Florin Iucha <florin@signbit.net>
 */

/*
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SBIT_MPSC_H_INCLUDED
#define SBIT_MPSC_H_INCLUDED

#include <sbit/stats.h>

#include <atomic>
#include <cstdlib>
#include <vector>

namespace sbit
{

namespace mpscq
{
class Queue
{
public:
   /** Metadata for object pushed into the queue
    */
   struct Node
   {
      /// Pointer to user data
      void* ptr;

      /// Next element in the queue
      Node* next;

      /// What thread group uses this node
      uintptr_t affinity;

      /// Used to pad the node to 128 bytes to avoid false sharing between nodes
      uintptr_t padding;
   };

   void append(Node* elem)
   {
      elem->next = m_head.load(std::memory_order_relaxed);

      while (!std::atomic_compare_exchange_weak_explicit(
         &m_head, &elem->next, elem, std::memory_order_release, std::memory_order_relaxed))
      {
         // the body of the loop is empty
      }
   }

   Node* flushAll()
   {
      Node* values = std::atomic_exchange_explicit(&m_head, nullptr, std::memory_order_release);

      // TODO(florin): reverse the links so we return a pointer to the oldest element

      return values;
   }

private:
   std::atomic<Node*> m_head;
};

} // namespace mpscq

/** Lock-free object pool
 *
 */
class NodePool
{
public:
   NodePool(size_t threadCount, size_t poolSize);

   /** Allocates some nodes
    *
    * @param affinity Indicates what thread will use this node
    * @param maxCount Specifies the maximum desired number of allocated nodes
    * @return a pointer to the head node
    */
   mpscq::Queue::Node* allocate(uintptr_t affinity, size_t maxCount);

   /** Recycles a node list
    *
    * @param node Points to the head of the node list
    */
   void recycle(mpscq::Queue::Node* node);

private:
   struct ThreadStatistics
   {
      sbit::Stats requestSizes;
   };

   std::vector<mpscq::Queue::Node> m_nodePool;

   std::atomic<mpscq::Queue::Node*> m_globalFreeHead;

   std::vector<std::atomic<mpscq::Queue::Node*>> m_threadNodePool;

   std::vector<ThreadStatistics> m_statistics;
};
} // namespace sbit

#endif // SBIT_MPSC_H_INCLUDED
