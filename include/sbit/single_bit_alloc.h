/**
 * @file sbit/single_bit_alloc.h
 * @brief Lock-free allocator for a single bit
 * @copyright 2021 Florin Iucha <florin@signbit.net>
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

#ifndef SBIT_SINGLE_BIT_ALLOC_H_INCLUDED
#define SBIT_SINGLE_BIT_ALLOC_H_INCLUDED

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <new>
#include <vector>

namespace sbit::alloc
{

/** \addtogroup concurrency
 *
 * @{
 */

/** Allocator for a single bit in the [0..capacity) range
 */
class SingleBitAllocator
{
public:
   SingleBitAllocator(size_t capacity) : m_elements(capacity / sizeof(uint64_t))
   {
   }

   ~SingleBitAllocator() = default;

   SingleBitAllocator(const SingleBitAllocator& other) = delete;
   SingleBitAllocator& operator=(const SingleBitAllocator& other) = delete;

   bool isEmpty();

   bool isFull();

   size_t tryAllocate();

   size_t waitUntilAllocate();

   void release();

private:
   std::atomic<int> m_availableElements;

#ifdef __cpp_lib_hardware_interference_size
   static constexpr size_t hardware_destructive_interference_size = std::hardware_destructive_interference_size;
#else
   // 64 bytes on x86-64 │ L1_CACHE_BYTES │ L1_CACHE_SHIFT │ __cacheline_aligned │ ...
   static constexpr std::size_t hardware_destructive_interference_size  = 2 * sizeof(std::max_align_t);
#endif

   struct Bitmap
   {
      alignas(hardware_destructive_interference_size) std::atomic<size_t> m_bitmap;
   };

   size_t m_lastBitmap{0};

   std::vector<uint64_t> m_elements;
};

/** @} */

} // namespace sbit::alloc

#endif // SBIT_SINGLE_BIT_ALLOC_H_INCLUDED
