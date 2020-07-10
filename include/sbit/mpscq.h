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
#include <list>
#include <memory>
#include <vector>

namespace sbit
{

namespace mpscq
{
class Queue
{
public:
   struct Envelope
   {
      /// Next element in the queue
      Envelope* next;

      /// Where to return the node after the message is processed
      std::atomic<Envelope*>* returnHead;
   };

   template <typename Payload>
   struct Message
   {
      Envelope envelope;

      /// The payload
      Payload payload;
   };

   void append(Envelope* elem)
   {
      elem->next = m_head.load(std::memory_order_relaxed);

      while (!std::atomic_compare_exchange_weak_explicit(
         &m_head, &elem->next, elem, std::memory_order_release, std::memory_order_relaxed))
      {
         // the body of the loop is empty
      }
   }

   Envelope* flushAll()
   {
      Envelope* values = std::atomic_exchange_explicit(&m_head, nullptr, std::memory_order_release);

      // TODO(florin): reverse the links so we return a pointer to the oldest element

      return values;
   }

private:
   std::atomic<Envelope*> m_head;
};

template <typename Payload>
class MessagePool
{
public:
   using Envelope = Queue::Envelope;
   using Message  = Queue::Message<Payload>;

   MessagePool(size_t allocationGroupSize, std::pmr::memory_resource* memoryResource) :
      m_pool{allocationGroupSize, memoryResource}
   {
   }

   /** Allocates a message
    *
    * @return a pointer to the message
    */
   Message* allocate();

private:
   std::atomic<Envelope*> m_recycleHead;

   Message* m_freeMessages = nullptr;

   std::list<std::vector<Message>> m_pool;
};

} // namespace mpscq

} // namespace sbit

#endif // SBIT_MPSC_H_INCLUDED
