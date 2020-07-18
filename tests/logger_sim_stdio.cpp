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

#include <fmt/chrono.h>
#include <fmt/format.h>
#include <fmt/ostream.h>

#include <array>
#include <chrono>
#include <cstdio>
#include <memory_resource>
#include <random>
#include <string_view>
#include <thread>

using namespace std::chrono_literals;

namespace
{

const auto performWrites          = true;
const auto processorIdlePeriod    = 0ms;
const auto generatorCoolOffPeriod = 500us;

struct LogEntry
{
   uint64_t         timestamp;
   std::string_view source;

   fmt::memory_buffer message;
};

class LoggerSink final : public sbit::mpscq::Processor<LogEntry>
{
public:
   explicit LoggerSink(sbit::mpscq::Queue& queue, FILE* fp) : Processor{queue}, m_fp{fp}, m_messageCount{0}
   {
   }

   ~LoggerSink() override
   {
      fmt::print("Observed {} messages.\n", m_messageCount);
   }

protected:
   void process(const LogEntry& entry) override
   {
      ++m_messageCount;
      if (performWrites)
      {
         std::fwrite(entry.message.data(), 1, entry.message.size(), m_fp);
         std::fputc('\n', m_fp);
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
   FILE*  m_fp;
   size_t m_messageCount;
};

void consumerThread(LoggerSink& sink)
{
   sink.startProcessing();
}

void producerThread(std::atomic<bool>& done, sbit::mpscq::Queue& queue)
{
   std::vector<char>                   dataBucket(/* __n = */ 65536, /* __v = */ 0);
   std::pmr::monotonic_buffer_resource resource{
      dataBucket.data(), dataBucket.size(), std::pmr::null_memory_resource()};
   sbit::mpscq::MessagePool<LogEntry> pool{32, 64, &resource};

   const std::string threadId = fmt::format("Thread-{}", std::this_thread::get_id());

   std::random_device                      rd;
   std::mt19937::result_type               seed = 42U;
   std::mt19937                            gen(seed);
   std::uniform_int_distribution<unsigned> distrib(0, 1024);

   size_t poolExhausted    = 0;
   size_t messageAvailable = 0;
   size_t messagesSent     = 0;

   while (!done.load(std::memory_order_acquire))
   {
      auto* msg = pool.allocate();
      if (msg == nullptr)
      {
         // pool exhausted
         if (generatorCoolOffPeriod.count() > 0)
         {
            std::this_thread::sleep_for(generatorCoolOffPeriod);
         }
         else
         {
            std::this_thread::yield();
         }
         poolExhausted++;
         continue;
      }

      messageAvailable++;

      std::time_t now = std::time(nullptr);
      struct tm   tm;
      localtime_r(&now, &tm);

      msg->payload.timestamp = now;
      msg->payload.source    = threadId;

      const unsigned value = distrib(gen);

      const std::string_view sinkName{"basic_logger"};
      const std::string_view level{"info"};

      msg->payload.message.clear();
      fmt::format_to(msg->payload.message,
                     "[{:%Y-%m-%d %H:%M:%S}] [{}] [{}] {}:{}:The lucky number is {}",
                     tm,
                     sinkName,
                     level,
                     msg->payload.timestamp,
                     threadId,
                     value);

      queue.append(&msg->envelope);

      ++messagesSent;
   }

   // fmt::print("For {}, message available: {}, pool exhausted {}\n", threadId, messageAvailable, poolExhausted);
   fmt::print("{} sent {} messages\n", threadId, messagesSent);
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

   FILE* out = fopen(argv[2], "wb");
   {
      LoggerSink  sink{queue, out};
      std::thread sinkThread{consumerThread, std::ref(sink)};

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
      sinkThread.join();

      doneFlag.store(true, std::memory_order_release);

      for (auto& tt : producerThreads)
      {
         tt.join();
      }
   }

   fflush(out);
   fclose(out);

   fmt::print("Test complete.\n");

   return 0;
}
