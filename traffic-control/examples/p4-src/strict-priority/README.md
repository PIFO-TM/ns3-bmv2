
Strict Priority
---------------

* Single node with single PIFO
* 3 buffer IDs and 3 partitions
* Each flow is assigned to a different buffer ID (and hence partition)
* Enqueue logic sets rank = buffer ID
* Dequeue logic reads from PIFO ID = 0

