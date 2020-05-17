# HeapAllocator
Implementation of Heap Allocator from scratch

Implicit Free List Allocator
--------
Features:
  - - Headers that track block information (size, status: used/free)
  - Free blocks recycled and  reused for subsequent malloc requests if possible
  - Malloc implementation searches heap for free blocks using an implicit list (traverse block by block)
  - Uses a best-fit search -- sacrifices time/speed for utilization, since the best fit block can only be determined after all the blocks are parsed 
    - higher utilization than other search mechanisms-- suffers less from fragmentation

Eplicit Free List Allocator
--------
Features:
  - Block headers that track block information (size, status: used/free)
  - Explicit free block list managed as doubly-linked list
    -  First 16 bytes of each free block's payload used to store pointers to previous, next block
  - Malloc implemented to search explicit list of free blocks
  - Freed block coalesced with neighbour block to the right if possible (O(1) time)
  - Realloc resizes block in-place if possible and absorbs adjacent free blocks as much as possible
  - first-fit  search to find usable blocks. I was already trying to reduce fragmentation when reallocing to a smaller size and wanted better throughput given the greater complexity of realloc. 
 
- The average utilization of all the .script files in samples was 77%: generally strong utilization of my design
- Analysis: fragmentation caused by choosing the first suitable block since the first block size might be quite large when what was requested was much smaller. 

