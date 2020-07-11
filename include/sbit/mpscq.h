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
#include <cstddef>
#include <list>
#include <memory_resource>
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

      void appendTo(std::atomic<Envelope*>* head)
      {
         next = head->load(std::memory_order_relaxed);

         while (!std::atomic_compare_exchange_weak_explicit(
            head, &next, this, std::memory_order_release, std::memory_order_relaxed))
         {
            // the body of the loop is empty
         }
      }

      void recycle()
      {
         appendTo(returnHead);
      }
   };

   template <typename Payload>
   struct Message
   {
      Envelope envelope;

      /// The payload
      Payload payload;

      void recycle()
      {
         envelope.recycle();
      }
   };

   void append(Envelope* elem)
   {
      elem->appendTo(&m_head);
   }

   Envelope* flushAll()
   {
      Envelope* values = std::atomic_exchange_explicit(&m_head, nullptr, std::memory_order_release);

      // TODO(florin): reverse the links so we return a pointer to the oldest element

      return values;
   }

private:
   std::atomic<Envelope*> m_head{nullptr};
};

template <typename Payload>
class MessagePool
{
public:
   using Envelope = Queue::Envelope;
   using Message  = Queue::Message<Payload>;

   MessagePool(size_t allocationGroupSize, std::pmr::memory_resource* memoryResource) :
      m_allocationGroupSize{allocationGroupSize}, m_recycleHead{nullptr}, m_pool{allocationGroupSize, memoryResource}
   {
   }

   /** Allocates a message
    *
    * @return a pointer to the message
    */
   Message* allocate()
   {
      if (m_freeMessages == nullptr)
      {
         m_freeMessages = std::atomic_exchange_explicit(&m_recycleHead, nullptr, std::memory_order_release);
      }

      if (m_freeMessages == nullptr)
      {
         auto& elems = m_pool.emplace_back(std::pmr::vector<Message>{m_allocationGroupSize, m_pool.get_allocator()});
         for (auto& msg : elems)
         {
            msg.envelope.next       = m_freeMessages;
            m_freeMessages          = &msg.envelope;
            msg.envelope.returnHead = &m_recycleHead;
         }
      }

      auto* msg      = reinterpret_cast<Message*>(m_freeMessages);
      m_freeMessages = m_freeMessages->next;
      return msg;
   }

private:
   const size_t m_allocationGroupSize;

   std::atomic<Envelope*> m_recycleHead;

   Envelope* m_freeMessages = nullptr;

   std::pmr::list<std::pmr::vector<Message>> m_pool;
};

class ProcessorBase
{
public:
   explicit ProcessorBase(Queue& queue) : m_queue(queue)
   {
   }

   virtual ~ProcessorBase() = default;

   virtual void processElement(Queue::Envelope* envelope) = 0;

   virtual void afterBatch()
   {
      // do nothing
   }

   virtual void onIdle()
   { // do nothing
   }

   void startProcessing()
   {
      while (!m_done.load(std::memory_order_acquire))
      {
         auto* envelope = m_queue.flushAll();

         if (envelope == nullptr)
         {
            onIdle();
            continue;
         }

         while (envelope != nullptr)
         {
            processElement(envelope);

            auto* next = envelope->next;
            envelope->recycle();
            envelope = next;
         }

         afterBatch();
      }
   }

   void interrupt()
   {
      m_done.store(true, std::memory_order_release);
   }

private:
   Queue&            m_queue;
   std::atomic<bool> m_done{false};
};

template <typename Payload>
class Processor : public ProcessorBase
{
public:
   explicit Processor(Queue& queue) : ProcessorBase(queue)
   {
   }

protected:
   using Message = Queue::Message<Payload>;

   virtual void process(const Payload& payload) = 0;

   void processElement(Queue::Envelope* envelope) override
   {
      auto* message = reinterpret_cast<Message*>(envelope);
      process(message->payload);
   }
};

} // namespace mpscq

} // namespace sbit

#endif // SBIT_MPSC_H_INCLUDED
