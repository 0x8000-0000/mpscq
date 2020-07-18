/**
 * @file tests/logger_spdlog.cpp
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

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

#include <fmt/format.h>
#include <fmt/ostream.h>

#include <array>
#include <chrono>
#include <random>
#include <thread>

using namespace std::chrono_literals;

namespace
{

const auto performWrites          = true;

void producerThread(std::atomic<bool>& done)
{
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

   size_t messagesSent = 0;

   while (!done.load(std::memory_order_acquire))
   {
      const std::chrono::time_point<std::chrono::system_clock> now      = std::chrono::system_clock::now();
      const auto                                               duration = now.time_since_epoch();

      const unsigned value = distrib(gen);

      if (performWrites)
      {
         spdlog::info("{}:{}:The lucky number is {}",
                      std::chrono::duration_cast<std::chrono::microseconds>(duration).count(),
                      threadId,
                      value);
      }

      ++ messagesSent;
   }

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

   auto file_logger = spdlog::basic_logger_mt("basic_logger", argv[2]);
   spdlog::set_default_logger(file_logger);

   std::atomic<bool> doneFlag{false};

   std::vector<std::thread> producerThreads;
   producerThreads.reserve(threadCount);
   for (int ii = 0; ii < threadCount; ++ii)
   {
      producerThreads.emplace_back(producerThread, std::ref(doneFlag));
   }

   const int testTime = 30;

   fmt::print("Counting down {} seconds\n", testTime);

   for (int ii = 0; ii < testTime; ++ii)
   {
      std::this_thread::sleep_for(1s);
   }

   fmt::print("Stimulus complete. Shutting down...\n");

   doneFlag.store(true, std::memory_order_release);

   for (auto& tt : producerThreads)
   {
      tt.join();
   }

   fmt::print("Test complete.\n");

   return 0;
}
