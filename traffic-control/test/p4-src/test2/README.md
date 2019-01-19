
Test 2
------

* 3 nodes, each with single PIFO
* 2 buffer partitions, one for each leaf node
* Classification logic selects buffer ID = 0, leaf node = 1
* At each node:
  * Enqueue logic sets rank = 10
  * Dequeue logic reads from PIFO ID = 0

