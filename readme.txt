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
For the explicit allocator, I ended up using a first-fit  search to find usable blocks. I  did  so since
I was already trying to reduce fragmentation when reallocing to a smaller size and wanted better throughput
given the greater complexity of realloc. Of course, this design  decision was trying to balance out the utilization
and throughput give the linked list could be quite long and searching through every node could be slow. Thus,
hopefully this design balances the two well for  the reasons mentioned earlier. The average utilization of all the
.script files in samples was 77% showing the generally strong utilization of my design. One weakness is evidently the
fragmentation caused by choosing the first suitable block since the first block size might be quite large when what
was requested was much smaller. Thus, there is also a greater chance of  running out of heap space.

In designing my allocator, I found it really useful to have a pointer to the end of the heap so far ie the block of
the remaining segment. This way, I didn't have to keep parsing through all  the  blocks to get the address of where a
new allocation should go. It was also really helpful in checking if certain cases where true. A fun anecdote is that
I realized how much easier it was to erase a function and recode it from scratch once I realized what I was doing wrong. It was actually a lot harder to debug something after I decided to change one thing or an error since there were so many small places to change. This was true for my merge function, where I spent hours debugging it. I ended up drawing out all the possible cases and snippets of code on paper and ended up trashing/commenting out the old merge and coding it from scratch. That process only took an hour or so which would have been a much better use of my time in the first place. 


Tell us about your quarter in CS107!
-----------------------------------
Although I am unsatisfied with how I did on the midterm (which was below average), I am really proud of how I proceed
to debug my code and how dedicated and focused I can  get. After 107, I feel like I can actually code and have learned
so much about memory (vs 106B where I was quite confused with the  whole heap/stack concept).Thank you to all the teaching staff for making resources readily available!


