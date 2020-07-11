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

#include <array>
#include <fstream>
#include <memory_resource>
#include <string_view>

namespace
{

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
   explicit LoggerSink(sbit::mpscq::Queue& queue, std::ostream& os) : Processor{queue}, m_os{os}
   {
   }

protected:
   void process(const LogEntry& entry) override
   {
      const std::string_view text{entry.message.data(), entry.messageLength};
      m_os << entry.timestamp << ':' << entry.source << ':' << text << '\n';
   }

   void afterBatch() override
   {
      interrupt();
   }

   void onIdle() override
   {
      // do nothing
   }

private:
   std::ostream& m_os;
};
} // anonymous namespace

int main(int argc, char* argv[])
{
   if (argc < 2)
   {
      fmt::print("Thread count and output file arguments are required.");
      return 1;
   }

   sbit::mpscq::Queue queue;

   std::ofstream out{argv[1]};
   LoggerSink    sink(queue, out);

   return 0;
}
