# MemoryPoolAllocator
Implementation of memory pool allocator in C++. 

Buckets of fixed size are allocated at compile time and later on allocator uses that memory without the need to allocate more memory. The memory is allocated once which can improve performance when allocating a lot of objects. 
