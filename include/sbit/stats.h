/**
 * @file sbit/stats.h
 * @brief Running statistics
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

#ifndef SBITS_STATS_H_INCLUDED
#define SBITS_STATS_H_INCLUDED

#include <cstdlib>

namespace sbit
{

/** \addtogroup statistics
 *
 * Statistics utilities
 *
 * @{
 */

/** Maintain running statistics for a data stream
 *
 * Implementation adapted from:
 *
 *    https://www.johndcook.com/blog/standard_deviation/
 *
 *    "There is a way to compute variance that is more accurate and is
 *    guaranteed to always give positive results. Furthermore, the method
 *    computes a running variance. That is, the method computes the variance
 *    as the x‘s arrive one at a time. The data do not need to be saved for
 *    a second pass.
 *
 *    This better way of computing variance goes back to a 1962 paper by
 *    B. P. Welford and is presented in Donald Knuth’s Art of Computer
 *    Programming, Vol 2, page 232, 3rd edition."
 */
class Stats
{
public:
   /** Resets the statistics so the object can be reused
    */
   void reset() noexcept
   {
      m_count = 0;
   }

   /** Observe a new value and mix it in the statistics
    *
    * @param value is the value that was observed
    */
   void observe(double value) noexcept
   {
      ++m_count;

      if (1 == m_count)
      {
         m_mean = value;
      }
      else
      {
         const double oldMean     = m_mean;
         const double oldVariance = m_variance;

         m_mean     = oldMean + (value - oldMean) / m_count;
         m_variance = oldVariance + (value - oldMean) * (value - m_mean);
      }
   }

   /** @return the number of observations
    */
   size_t getCount() const noexcept
   {
      return m_count;
   }

   /** @return the arithmetic mean statistic of the observations
    */
   double getMean() const noexcept
   {
      return m_mean;
   }

   /** @return the variance statistic of the observations
    */
   double getVariance() const noexcept
   {
      if (m_count > 1)
      {
         return m_variance / static_cast<double>(m_count - 1);
      }

      return 0.0;
   }

private:
   size_t m_count = 0;

   double m_mean     = 0.0;
   double m_variance = 0.0;
};

/** @}*/

} // namespace sbit

#endif // SBITS_STATS_H_INCLUDED
