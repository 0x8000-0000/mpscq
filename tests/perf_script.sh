#!/bin/bash

set -x

for tt in 4 8 12; do

   rm -f report.spd.$tt
   rm -f report.sim.$tt

   for ii in {1..6}; do
      /usr/bin/time -a -o report.spd.$tt tests/logger_spdlog $tt foo.spd.$tt.$ii >> report.spd.$tt
      wc -l foo.spd.$tt.$ii >> report.spd.$tt
      rm foo.spd.$tt.$ii

      /usr/bin/time -a -o report.sim.$tt tests/logger_sim_stdio $tt foo.sim.$tt.$ii >> report.sim.$tt
      wc -l foo.sim.$tt.$ii >> report.sim.$tt
      rm foo.sim.$tt.$ii
   done

done
