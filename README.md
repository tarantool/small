# small - a collection of Specialized Memory ALLocators for small allocations

[![Build Status](https://travis-ci.org/tarantool/small.png?branch=master)](https://travis-ci.org/tarantool/small)

The library provides the following facilities:

# quota

Set a limit on the amount of memory all allocators use.
Thread-safe.

## slab_arena

To initialize an arena, you need a quota. Multiple arenas
can use a shared quota object. Thread safe.

Defines an API with two methods: map() and unmap().
Map returns a memory area. Unmap returns this area to the arena.
All objects returned by arena have the same size, defined in
initialization-time constant SLAB_MAX_SIZE.
By default, SLAB_MAX_SIZE is 4M. All objects returned by arena
are aligned by SLAB_MAX_SIZE: (ptr & (SLAB_MAX_SIZE - 1)) is
always 0. SLAB_MAX_SIZE therefore must be a power of 2. Limiting
SLAB_MAX_SIZE is important to avoid internal fragmentation.
Multiple arenas can exist, an object must be returned to the same
arena in which it was allocated.

There is a number of different implementations of slab_arena
API:

- huge_arena: this implementation maps at initialization
  time a huge region of memory, and then uses this region to
  produce objects. Can be configured to use shared or private
  mappings.
- grow_arena - mmaps() each individual block. Thus can incur
  fragmentation of the address space, but actually
  returns objects to the OS on unmap.

Use of instances of slab_arena is thread-safe: multiple
threads can use the same arena.
  
## slab_cache

Requires an arena for initialization, which works
as a memory source for slab_cache.
Returns power-of-two sized slabs, with size-aligned address.
Uses a buddy system to deal with memory fragmentation.
Is expected to be thread-local.

## mempool

A memory pool for objects of the same size. Thread local.
Requires a slab cache, which works as a source of memory.
Automatically defines the optimal slab size, given 
the object size. Supports alloc() and free().

## region

A typical region allocator. Very cheap allocation,
but all memory can be freed at once only. Supports savepoints,
i.e. an allocation point to which it can roll back, i.e.
free all memory allocated after a savepoint.
Uses slab_cache as a memory source.

## small

A typical slab allocator. Built as a collection 
of mempool allocators, each mempool suited for a particular
object size. Has stepped pools, i.e. pools for small objects 
up to 500 bytes, and factored pools, for larger objects.
The difference between stepped and factored pools is that
object size in stepped pools grows step by step, each
next pool serving objects of prev_pool_object_size + 8.
In factored pools a growth factor is used, i.e. 
given a factor of 1.1 and previous pool for objects
of size up to 1000, next pool will serve objects in range
1001-1100.
Since is based on mempool, uses slab_cache as a memory source.

## ibuf

A typical input buffer, which could be seen as a memory allocator
as well, which reallocates itself when it gets full.
Uses slab_cache as a memory source.

## obuf

Another implementation of an output buffer, which, for growth,
instead of reallocation, used a collection of buffers,
size of each next buffer in a collection twice the
size the prevoius one. 
Uses slab_cache as a memory source.

## matras

This is the best one. Memory Address Translating
allocator. Only allows to allocate objects of size
which is a power of two. Provides a 32-bit
id for each allocated object. Supports multi-versioning
for all allocated objects, i.e. it's possible to create
a consistent read view of all allocated memory.

Uses slab_cache as a memory source.
