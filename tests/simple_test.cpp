/**
 * @file tests/simple_test.cpp
 * @brief Tests for Multiple-Producer-Single-Consumer Queue
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

#include <sbit/mpscq.h>

#include <gtest/gtest.h>

#include <array>
#include <memory_resource>

namespace
{
struct Data
{
   int value;
};

class Sender
{
public:
   Sender()
   {
   }

   void sendNumbers(size_t count, sbit::mpscq::Queue& queue)
   {
      for (size_t ii = 0; ii < count; ++ii)
      {
         auto* msg          = m_pool.allocate();
         msg->payload.value = ii;
         queue.append(&msg->envelope);
      }
   }

private:
   std::array<char, 1024> m_dataBucket = {};

   std::pmr::monotonic_buffer_resource m_resource{m_dataBucket.data(), m_dataBucket.size()};

   sbit::mpscq::MessagePool<Data> m_pool{8, 16, &m_resource};
};

class Receiver
{
public:
   void receive(sbit::mpscq::Queue& queue)
   {
      auto* envelope = queue.flushAll();

      while (envelope != nullptr)
      {
         auto* next = envelope->getNext();

         auto* payload = static_cast<Data*>(envelope->getPayload());

         m_sum += payload->value;

         envelope->recycle();

         envelope = next;
      }
   }

   int getSum() const noexcept
   {
      return m_sum;
   }

private:
   int m_sum = 0;
};

class IntegerSummer : public sbit::mpscq::Processor<Data>
{
public:
   explicit IntegerSummer(sbit::mpscq::Queue& queue) : Processor{queue}
   {
   }

   int getSum() const noexcept
   {
      return m_sum;
   }

protected:
   void process(const Data& data) override
   {
      m_sum += data.value;
   }

   void afterBatch() override
   {
      interrupt();
   }

private:
   int m_sum = 0;
};
} // anonymous namespace

TEST(SimpleTest, SendReceive)
{
   sbit::mpscq::Queue queue;

   Sender   sender;
   Receiver receiver;

   sender.sendNumbers(5, queue);
   receiver.receive(queue);

   ASSERT_EQ(10, receiver.getSum());
}

TEST(SimpleTest, UseProcessor)
{
   sbit::mpscq::Queue queue;

   Sender        sender;
   IntegerSummer receiver{queue};

   sender.sendNumbers(5, queue);
   receiver.startProcessing();

   ASSERT_EQ(10, receiver.getSum());
}
