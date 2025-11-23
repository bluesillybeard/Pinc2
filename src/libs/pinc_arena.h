#ifndef PINC_ARENA_H
#define PINC_ARENA_H 1
// Arena allocator inspired by Zig's std.heap.ArenaAllocator.
// It's like any ordinary arena allocator, but it can do some extra things:
// - free the most recent allocation (stack-based allocations)
// - grow indefinitely (up until the root allocator stops giving up bytes)
// - uses a backing allocator. Pinc itself doesn't really use that functionality.

// This one in particular is optimized for a large number of small allocations - very large allocations (larger than say 100kib) should be rare.

// Differences from the Zig version:
// - This allocates the smallest number of needed fixed size blocks instead of creating buffers 1.5x the size of the requested allocation
//     - this decision was made on the assumption that the underlying allocator is optimized for aspecific large allocation size (which is the case for a raw OS page alloc)

// Note that, for now, this arena allocator is only tested indirectly by Pincs testing suite, so it's not even slightly guaranteed to work outside of Pinc.
// TODO(bluesillybeard): Make some proper tests for this arena allocator so we know it works reliably and consistently

#include "pinc_allocator.h"

// memory block, minus two size_t's worth of bytes for some metadata. Effectively a linked list.
typedef struct PincArenaAllocatorBlockStruct {
    // the full size of this block. Will be a multiple of the block size.
    size_t data;
    // null for the last block
    struct PincArenaAllocatorBlockStruct* next;
} PincArenaAllocatorBlock;

typedef struct {
    // The allocator will allocate in blocks of this size or a multiple of this size.
    size_t blockSize;
    PincAllocator back;
    // Blocks are placed backwards, so the last block in the list is the oldest.
    PincArenaAllocatorBlock* blocks;
    // empty blocks, reset() moves blocks into here instead of freeing them if the caller requests some additional reserve beyond the initial capacity
    PincArenaAllocatorBlock* emptyBlocks;
    // The allocator will only allocate from the last block.
    // So instead of storing the used amount for each block, we store it once for the 'surface' or 'top' block that we allocate from.
    // It does not include the space taken up by the block struct itself.
    size_t lastBlockUsed;
} PincArenaAllocator;

void PincArenaAllocator_init(PincArenaAllocator* this, PincAllocator back, size_t initialCapacity, size_t blockSize);

void* PincArenaAllocator_allocate(void* this, size_t size);

void* PincArenaAllocator_allocateAligned(void* this, size_t size, size_t alignment);

void* PincArenaAllocator_reallocate(void* this, void* ptr, size_t size, size_t newSize);

void PincArenaAllocator_free(void* this, void* ptr, size_t size);

/// Note: keep is merely a *hint* to how much memory to keep around, not an exact quantity.
/// Generally the allocator might have an extra block compared to keep if keep doesn't perfectly align with an integer number of blocks.
void PincArenaAllocator_reset(PincArenaAllocator* this, size_t keep);

void PincArenaAllocator_deinit(PincArenaAllocator* this);

#endif