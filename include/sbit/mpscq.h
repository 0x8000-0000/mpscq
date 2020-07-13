/**
 * @file sbit/mpscq.h
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

namespace sbit::mpscq
{

/** \addtogroup concurrency
 *
 * Concurrency utilities.
 *
 * @{
 */

/** Multiple-producer Single-Consumer Queue
 *
 * Messages wrapped in (prepended by) Envelopes are appended by multiple
 * producers and popped in batches by a single consumer.
 *
 * This implementation is lock-free.
 */
class Queue
{
public:
   /** Header/metadata for a message
    */
   class Envelope
   {
   public:
      /** Chains the next envelope to this one
       *
       * @param next Is the next envelope in the unsynchronized list
       */
      void setNext(Envelope* next) noexcept
      {
         m_next = next;
      }

      /** @return the next element in the free list
       */
      Envelope* getNext() const noexcept
      {
         return m_next;
      }

      /** Sets the worker pool that currently owns this envelope
       *
       * @param poolMailbox Points to the mailbox for the pool
       */
      void setWorkerPool(std::atomic<Envelope*>* poolMailbox)
      {
         m_workerPool = poolMailbox;
      }

      /** Sets the pool that physically owns this envelope
       *
       * @param poolMailbox Points to the mailbox for the pool
       */
      void setOwnerPool(std::atomic<Envelope*>* poolMailbox)
      {
         m_ownerPool = poolMailbox;
      }

      /** Atomically appends this element to the specified queue
       *
       * @param poolMailbox is the head of the queue
       */
      void appendTo(std::atomic<Envelope*>* poolMailbox) noexcept
      {
         m_next = poolMailbox->load(std::memory_order_relaxed);

         while (!std::atomic_compare_exchange_weak_explicit(
            poolMailbox, &m_next, this, std::memory_order_release, std::memory_order_relaxed))
         {
            // the body of the loop is empty
         }
      }

      /** Recycles this element by returning it to its worker pool
       */
      void recycle() noexcept
      {
         appendTo(m_workerPool);
      }

      /** Releases this element by returning it to its owner pool
       */
      void release() noexcept
      {
         appendTo(m_ownerPool);
      }

      /** Sets the payload for this message
       *
       * @param payload Is a pointer to the useful data
       */
      void setPayload(void* payload) noexcept
      {
         m_payload = payload;
      }

      /** @return the payload of the message
       */
      void* getPayload() const noexcept
      {
         return m_payload;
      }

   private:
      /// Next element in the queue
      Envelope* m_next;

      /// Where to return the object after the message is processed
      std::atomic<Envelope*>* m_workerPool;

      /// Where to return the object when the worker is shut down
      std::atomic<Envelope*>* m_ownerPool;

      /// The payload wrapped in this envelope
      void* m_payload;
   };

   /** Appends this object to the queue
    *
    * @param elem is the new element
    */
   void append(Envelope* elem)
   {
      elem->appendTo(&m_head);
   }

   /** Atomically removes and returns all elements from the queue
    *
    * @return the entire contents of the queue
    */
   Envelope* flushAll()
   {
      Envelope* values = std::atomic_exchange_explicit(&m_head, nullptr, std::memory_order_release);

      // TODO(florin): reverse the links so we return a pointer to the oldest element

      return values;
   }

   /** Base class encapsulating the event loop of a consumer or processor
    */
   class Processor
   {
   public:
      /** Constructs a processor that consumes elements from a queue
       *
       * @param queue Is the queue from which elements are consumed
       */
      explicit Processor(Queue& queue) : m_queue(queue)
      {
      }

      /** Called for each element popped from the queue
       *
       * @param envelope Points to the element to be processed
       * @return nothing
       */
      virtual void processElement(Queue::Envelope* envelope) = 0;

      /** Called after each batch of elements is processed
       *
       * @return nothing
       */
      virtual void afterBatch()
      {
         // do nothing
      }

      /** Called when a new batch was requested, but no new elements were available
       *
       * @return nothing
       */
      virtual void onIdle()
      {
         // do nothing
      }

      /** Begin the "infinite" event loop
       */
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

               auto* next = envelope->getNext();
               envelope->recycle();
               envelope = next;
            }

            afterBatch();
         }
      }

      /** Interrupts the "infinite" event loop
       */
      void interrupt()
      {
         m_done.store(true, std::memory_order_release);
      }

      Processor(const Processor& other) = delete;
      Processor& operator=(const Processor& other) = delete;

      Processor(Processor&& other) = delete;
      Processor& operator=(Processor&& other) = delete;

      virtual ~Processor() = default;

   private:
      Queue&            m_queue;
      std::atomic<bool> m_done{false};
   };

private:
   std::atomic<Envelope*> m_head{nullptr};
};

/** Efficient object pool for messages
 *
 * Allocates objects in batches, using the passed-in memory resource.
 *
 * @tparam Payload Is the type of the useful payload processed via the queue
 */
template <typename Payload>
class MessagePool
{
public:
   /** Aggregation of a payload and an envelope
    *
    * @tparam Payload Is the type of the useful payload processed via the queue
    */
   struct Message
   {
      /// The envelope
      Queue::Envelope envelope;

      /// The payload
      Payload payload;

      /** Recycles this element by returning it to its worker pool
       */
      void recycle()
      {
         envelope.recycle();
      }
   };

   /** Constructs a message pool
    *
    * @param allocationGroupSize Specifies how many objects to allocate at once
    * @param memoryResource Indicates the memory resource backing the allocations
    */
   MessagePool(size_t allocationGroupSize, std::pmr::memory_resource* memoryResource) :
      m_allocationGroupSize{allocationGroupSize},
      m_recyclePool{nullptr},
      m_objectPool{allocationGroupSize, memoryResource}
   {
   }

   /** Allocates a message
    *
    * @return a pointer to the message
    */
   Message* allocate()
   {
      if (m_readyPool == nullptr)
      {
         m_readyPool = std::atomic_exchange_explicit(&m_recyclePool, nullptr, std::memory_order_release);
      }

      if (m_readyPool == nullptr)
      {
         try
         {
            auto& elems = m_objectPool.emplace_back(
               std::pmr::vector<Message>{m_allocationGroupSize, m_objectPool.get_allocator()});
            for (auto& msg : elems)
            {
               msg.envelope.setNext(m_readyPool);
               m_readyPool = &msg.envelope;
               msg.envelope.setWorkerPool(&m_recyclePool);
               msg.envelope.setOwnerPool(&m_recyclePool);
               msg.envelope.setPayload(&msg.payload);
            }
         }
         catch (...)
         {
            return nullptr;
         }
      }

      auto* msg   = reinterpret_cast<Message*>(m_readyPool);
      m_readyPool = m_readyPool->getNext();
      return msg;
   }

private:
   const size_t m_allocationGroupSize;

   /** Holds the atomically synchronized pool where processors are returning
    * objects
    */
   std::atomic<Queue::Envelope*> m_recyclePool;

   /** Holds the objects that are next in line for allocations
    */
   Queue::Envelope* m_readyPool = nullptr;

   /** Holds the objects
    */
   std::pmr::list<std::pmr::vector<Message>> m_objectPool;
};

/** Strongly-typed consumer
 *
 * Wraps the cast from the Queue::Envelope type to the Payload type
 *
 * @tparam Payload Is the type of the useful payload processed via the queue
 */
template <typename Payload>
class Processor : public Queue::Processor
{
public:
   /** Constructs a processor
    *
    * @param queue Is the queue from which elements are consumed
    */
   explicit Processor(Queue& queue) : Queue::Processor(queue)
   {
   }

protected:
   /** Process a message
    *
    * @param payload Is the message contents to be processed
    * @return nothing
    */
   virtual void process(const Payload& payload) = 0;

private:
   using Message = typename MessagePool<Payload>::Message;

   void processElement(Queue::Envelope* envelope) override
   {
      auto* payload = static_cast<Payload*>(envelope->getPayload());
      process(*payload);
   }
};

/** @}*/

} // namespace sbit::mpscq

#endif // SBIT_MPSC_H_INCLUDED
