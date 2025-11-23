#include "pinc_arena.h"
#include "libs/pinc_allocator.h"
#include "platform/pinc_platform.h"

static void guaranteeCapacity(PincArenaAllocator* this, size_t cap) {
    // We assume the user is asking for a *continuous* block of x bytes
    if(cap  == 0) {
        return;
    }

    if(this->blocks) {
        // See if there is already enough space on the top of the block stack
        size_t sizeRem = this->blocks->data - this->lastBlockUsed - sizeof(PincArenaAllocatorBlock);
        if(sizeRem >= cap) {
            return;
        }
    }

    // See if there are any empty blocks with enough space
    PincArenaAllocatorBlock* prevBlock = 0;
    PincArenaAllocatorBlock* block = this->emptyBlocks;
    size_t iteration = 0;
    // this would be slow if there are a lot of clear blocks and none/few of them are big enough, so there is an iteration limit at which point we just give up and make a new block
    while(block && iteration<50) {
        if(block->data - sizeof(PincArenaAllocatorBlock) >= cap) {
            // Nice, pull it out of the free list, put it on the stack, and get outa here
            if(prevBlock) {
                prevBlock->next = block->next;
            } else {
                this->emptyBlocks = block->next;
            }
            block->next = this->blocks;
            this->blocks = block;
            return;
        }
        prevBlock = block;
        block = block->next;
        iteration+=1;
    }

    // So, we couldn't find an existing block with enough space, gotta make a new allocation
    // round up to the nearest multiple of blocks including the overhead of the block metadata
    size_t capWithOverhead = ((cap + this->blockSize + sizeof(PincArenaAllocatorBlock) - 1) / this->blockSize) * this->blockSize;
    PincArenaAllocatorBlock* newBlock = PincAllocator_allocate(this->back, capWithOverhead);
    newBlock->data = capWithOverhead;
    newBlock->next = this->blocks;
    this->blocks = newBlock;
}

void PincArenaAllocator_init(PincArenaAllocator* this, PincAllocator back, size_t initialCapacity, size_t blockSize) {
    this->blockSize = blockSize;
    this->back = back;
    this->blocks = 0;
    this->emptyBlocks = 0;
    this->lastBlockUsed = 0;

    guaranteeCapacity(this, initialCapacity);
}

void* PincArenaAllocator_allocate(void* thisUncast, size_t size) {
    // C makes finding the fundamental alignment rather difficult...
    // ... So just like pretend it's always 16 for now.
    // Unlike a "normal" allocator impl, aligned alloc does not impart any meaningful downside over regular alloc.
    // Worst case is we allocate 15 more bytes than we wanted to
    return PincArenaAllocator_allocateAligned(thisUncast, size, 16);
}

void* PincArenaAllocator_allocateAligned(void* thisUncast, size_t size, size_t alignment) {
    PincArenaAllocator* this = (PincArenaAllocator*)thisUncast;
    guaranteeCapacity(this, size);
    // guaranteeCapacity puts the space on the top of the stack so we can just yoink some out willy nilly
    char* blockStart = (char*)this->blocks;
    uintptr_t firstFreeSpot = (uintptr_t) (blockStart + sizeof(PincArenaAllocatorBlock) + this->lastBlockUsed);
    // align the spot forward
    uintptr_t returnMe = ((firstFreeSpot + alignment - 1) / alignment) * alignment;
    this->lastBlockUsed += size + (returnMe - firstFreeSpot);
    return (void*)returnMe; //NOLINT: performance is fine here mate
}

void* PincArenaAllocator_reallocate(void* thisUncast, void* ptr, size_t size, size_t newSize) {
    if(newSize < size) {
        return ptr;
    }
    size_t extraSpaceNeeded = newSize - size;
    PincArenaAllocator* this = (PincArenaAllocator*)thisUncast;
    // Check if the pointer is at the top of the stack
    if(((char*)ptr) + size == ((char*)this->blocks + sizeof(PincArenaAllocatorBlock) + this->lastBlockUsed)) {
        // Check that there is enough additional space
        if(this->blocks->data - sizeof(PincArenaAllocatorBlock) - this->lastBlockUsed >= extraSpaceNeeded) {
            this->lastBlockUsed -= extraSpaceNeeded;
            return ptr;
        }
    }
    // Just redo the allocation at this point
    void* new = PincArenaAllocator_allocate(thisUncast, newSize);
    pincMemCopy(ptr, new, size);
    return new;
}

void PincArenaAllocator_free(void* thisUncast, void* ptr, size_t size) {
    PincArenaAllocator* this = (PincArenaAllocator*)thisUncast;
    // Check if the pointer is at the top of the stack
    if(((char*)ptr) + size == ((char*)this->blocks + sizeof(PincArenaAllocatorBlock) + this->lastBlockUsed)) {
        // Hooray, reclaim the space
        this->lastBlockUsed -= size;
    }
}

void PincArenaAllocator_reset(PincArenaAllocator* this, size_t keep) { //NOLINT: yes we all know how complex this function is, deal with it
    // Move all of the blocks to the clear list to start
    PincArenaAllocatorBlock* block = this->blocks;
    while(block) {
        PincArenaAllocatorBlock* next = block->next;
        block->next = this->emptyBlocks;
        this->emptyBlocks = block;
        block = next;
    }
    this->blocks = 0;
    this->lastBlockUsed = 0;

    // Bucket sort (theoretically) works well here since blocks are always going to be an integer number of the block size
    // And there should be a good distribution of mostly smaller blocks with a few much larger ones,
    // as most temp allocations (in Pinc anyways) are either super tiny temp strings or large objects like files.
    // Blocks larger than 32*blockSize will be dealt with later
    PincArenaAllocatorBlock* buckets[32] = {0};
    // pincMemSet(0, buckets, sizeof(buckets));

    PincArenaAllocatorBlock* prevBlock = 0;
    block = this->emptyBlocks;
    while(block) {
        PincArenaAllocatorBlock* next = block->next;
        // -1 so index 0 has size of 1 * blockSize
        size_t bucketIndex = (block->data / this->blockSize) - 1;
        if(bucketIndex < 32) {
            // remove the block from the list maintaining emptyBlocks as valid
            if(prevBlock) {
                prevBlock->next = next;
            } else {
                this->emptyBlocks = next;
            }
            // add the block into the appropriate bucket
            block->next = buckets[bucketIndex];
            buckets[bucketIndex] = block;
        } else {
            // if block was not moved into a bucket, it's the previous block in the list
            // If block WAS moved into a bucket, prevBlock remains the same
            prevBlock = block;
        }
        block = next;
    }

    // Count buckets from the largest to smallest until there is enough kept
    // After that just free them
    size_t kept = 0;
    for(size_t i = 31;; --i) {
        block = buckets[i];
        if(kept >= keep) {
            buckets[i] = 0;
        } else {
            while(block) {
                if(kept >= keep) {
                    // sever the list since everything after block will be freed
                    PincArenaAllocatorBlock* next = block->next;
                    block->next = 0;
                    block = next;
                    break;
                }
                kept += block->data;
                block = block->next;
            }
        }
        // block and everything after it shall be freed
        while(block) {
            PincArenaAllocatorBlock* next = block->next;
            PincAllocator_free(this->back, block, block->data);
            block = next;
        }
        // Iterating backwards to and including zero with an unsigned integer is super nice and not annoying at all
        if(i == 0) {
            break;
        }
    }

    // Collect all the buckets into a list so the head is the largest one
    // The ones added first end up at the bottom, and we want the smallest ones at the bottom
    // Putting the largest ones on top maximizes the chance of guaranteeCapacity finding a large empty block if the user requests a large capacity.
    PincArenaAllocatorBlock* newList = 0;
    for(size_t i = 0; i < 32; ++i) {
        block = buckets[i];
        while(block) {
            PincArenaAllocatorBlock* next = block->next;
            block->next = newList;
            newList = block;
            block = next;
        }
    }

    // Now add whatever was left (no need to maintain emptyBlocks now as we're emptying it anyways)
    block = this->emptyBlocks;
    while(block && kept < keep) {
        PincArenaAllocatorBlock* next = block->next;
        // except blocks larger than keep, those should be super rare anyways assuming a decently large keep
        if(block->data <= keep) {
            kept += block->data;
            block->next = newList;
            newList = block;
        } else {
            PincAllocator_free(this->back, block, block->data);
        }
        block = next;
    }

    this->emptyBlocks = newList;

    // So, we've managed to reset the allocator, select the largest blocks until we hit the keep limit, and free everything that remains,
    // all in O(n) time and O(1) memory - with the caveats being that blocks larger than keep are deleted, and blocks larger than blockSize*32 are not sorted at the end
}

void PincArenaAllocator_deinit(PincArenaAllocator* this) {
    // Just free everything lol
    PincArenaAllocatorBlock* block = this->blocks;
    while(block) {
        PincArenaAllocatorBlock* thisBlock = block;
        block = block->next;
        PincAllocator_free(this->back, thisBlock, thisBlock->data);
    }
    block = this->emptyBlocks;
    while(block) {
        PincArenaAllocatorBlock* thisBlock = block;
        block = block->next;
        PincAllocator_free(this->back, thisBlock, thisBlock->data);
    }
    // caller owns this, just reset so deinit() can be used clear the arena
    this->blocks = 0;
    this->emptyBlocks = 0;
    this->lastBlockUsed = 0;
}
