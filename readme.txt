File: readme.txt
Author: Natasha Ong
----------------------

implicit
--------
My main design decision for the implicit allocator was to use a best-fit search. This search type
sacrifices time/speed for utilization, since the best fit block can  only be determined after all
the blocks are parsed. Thus, it may  be slower than say something like first-fit  but should have a
higher  utilization than other search mechanisms. It therefore also suffers less from fragmentation.
By filling the smaller blocks first, we leave space/free blocks for potential larger allocations
(ie large malloc may not fail in best-fit but could in first fit/next fit).  I decided  togo  with
best-fit as I  wanted to maximize the utilization and see if there were  other ways in my code that
I   could try to minimize throughput

The  utilization tends to be at or above 70%, though the instruction count may be higher (but that also
depends  on script  and number of blocks, etc).

explicit
--------


Tell us about your quarter in CS107!
-----------------------------------



