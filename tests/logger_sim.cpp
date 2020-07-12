/**
 * @file tests/logger_sim.cpp
 * @brief Simulates multi-threaded logging to a simple sink
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

#include <fmt/format.h>
#include <fmt/ostream.h>

#include <array>
#include <chrono>
#include <fstream>
#include <memory_resource>
#include <random>
#include <string_view>
#include <thread>

using namespace std::chrono_literals;

namespace
{

const auto performWrites          = true;
const auto processorIdlePeriod    = 0ms;
const auto generatorCoolOffPeriod = 32ms;

struct LogEntry
{
   uint64_t         timestamp;
   std::string_view source;

   size_t                messageLength;
   std::array<char, 256> message;
};

class LoggerSink final : public sbit::mpscq::Processor<LogEntry>
{
public:
   explicit LoggerSink(sbit::mpscq::Queue& queue, std::ostream& os) : Processor{queue}, m_os{os}, m_messageCount{0}
   {
   }

   virtual ~LoggerSink()
   {
      m_os << "Observed " << m_messageCount << " messages.\n";
   }

protected:
   void process(const LogEntry& entry) override
   {
      ++m_messageCount;
      const std::string_view text{entry.message.data(), entry.messageLength};
      if (performWrites)
      {
         m_os << entry.timestamp << ':' << entry.source << ':' << text << '\n';
      }
   }

   void onIdle() override
   {
      if (processorIdlePeriod.count() > 0)
      {
         std::this_thread::sleep_for(processorIdlePeriod);
      }
      else
      {
         std::this_thread::yield();
      }
   }

private:
   std::ostream& m_os;
   size_t        m_messageCount;
};

void consumerThread(LoggerSink& sink)
{
   sink.startProcessing();
}

void producerThread(std::atomic<bool>& done, sbit::mpscq::Queue& queue)
{
   std::vector<char>                   dataBucket(/* __n = */ 16384, /* __v = */ 0);
   std::pmr::monotonic_buffer_resource resource{
      dataBucket.data(), dataBucket.size(), std::pmr::null_memory_resource()};
   sbit::mpscq::MessagePool<LogEntry> pool{8, &resource};

   const std::string threadId = fmt::format("Thread-{}", std::this_thread::get_id());

   std::random_device        rd;
   std::mt19937::result_type seed =
      rd() ^ ((std::mt19937::result_type)std::chrono::duration_cast<std::chrono::seconds>(
                 std::chrono::system_clock::now().time_since_epoch())
                 .count() +
              (std::mt19937::result_type)std::chrono::duration_cast<std::chrono::microseconds>(
                 std::chrono::high_resolution_clock::now().time_since_epoch())
                 .count());

   std::mt19937                            gen(seed);
   std::uniform_int_distribution<unsigned> distrib(0, 1024);

   size_t poolExhausted   = 0;
   size_t messgeAvailable = 0;

   while (!done.load(std::memory_order_acquire))
   {
      auto msg = pool.allocate();
      if (msg == nullptr)
      {
         // pool exhausted
         std::this_thread::sleep_for(generatorCoolOffPeriod);
         poolExhausted++;
         continue;
      }

      messgeAvailable++;

      const std::chrono::time_point<std::chrono::system_clock> now      = std::chrono::system_clock::now();
      const auto                                               duration = now.time_since_epoch();

      msg->payload.timestamp = std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
      msg->payload.source    = threadId;

      const unsigned value = distrib(gen);

      const auto len =
         fmt::format_to_n(msg->payload.message.data(), msg->payload.message.size(), "The lucky number is {}", value);
      msg->payload.messageLength = len.size;

      queue.append(&msg->envelope);
   }

   fmt::print("For {}, message available: {}, pool exhausted {}\n", threadId, messgeAvailable, poolExhausted);
}

} // anonymous namespace

int main(int argc, char* argv[])
{
   if (argc < 2)
   {
      fmt::print("Thread count and output file arguments are required.\n");
      return 1;
   }

   const auto threadCount = std::stoi(argv[1]);
   if ((threadCount < 1) || (threadCount > 512))
   {
      fmt::print("Invalid thread count: {} (converted to {})\n", argv[1], threadCount);
      return 1;
   }

   sbit::mpscq::Queue queue;

   std::ofstream out{argv[2]};
   LoggerSink    sink{queue, out};
   std::thread   sinkThread{consumerThread, std::ref(sink)};

   std::atomic<bool> doneFlag{false};

   std::vector<std::thread> producerThreads;
   producerThreads.reserve(threadCount);
   for (int ii = 0; ii < threadCount; ++ii)
   {
      producerThreads.emplace_back(producerThread, std::ref(doneFlag), std::ref(queue));
   }

   const int testTime = 30;

   fmt::print("Counting down {} seconds\n", testTime);

   for (int ii = 0; ii < testTime; ++ii)
   {
      std::this_thread::sleep_for(1s);
   }

   fmt::print("Stimulus complete. Shutting down...\n");

   sink.interrupt();
   doneFlag.store(true, std::memory_order_release);

   sinkThread.join();
   for (auto& tt : producerThreads)
   {
      tt.join();
   }

   fmt::print("Test complete.\n");

   return 0;
}
