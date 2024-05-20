/**
 * Do not submit your assignment with a main function in this file.
 * If you submit with a main function in this file, you will get a zero.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "debug.h"
#include "sfmm.h"

double totalPayload = 0;
double maxUtilization = 0;

sf_block *setFree(void *block, size_t size)
{
    sf_block *freeblock = block;
    if (((freeblock->header ^ MAGIC) & PREV_BLOCK_ALLOCATED) == PREV_BLOCK_ALLOCATED)
    {
        freeblock->header = size;
        freeblock->header |= PREV_BLOCK_ALLOCATED;
        freeblock->header ^= MAGIC;
    }
    else
    {
        freeblock->header = size;
        freeblock->header ^= MAGIC;
    }

    sf_footer *footer = block + size;
    *footer = freeblock->header;
    int index = 0;
    if (size == 32)
    {
        index = 0;
    }
    else if (size <= 64)
    {
        index = 1;
    }
    else if (size <= 128)
    {
        index = 2;
    }
    else if (size <= 256)
    {
        index = 3;
    }
    else if (size <= 512)
    {
        index = 4;
    }
    else if(size <= 1024)
    {
        index = 5;
    }
    else if(size <= 2048)
    {
        index = 6;
    }
    else if(size <= 4096)
    {
        index = 7;
    }
    else if(size <= 8192)
    {
        index = 8;
    }
    else
    {
        index = 9;
    }
    freeblock->body.links.next = sf_free_list_heads[index].body.links.next;
    freeblock->body.links.prev = &sf_free_list_heads[index];
    sf_free_list_heads[index].body.links.next->body.links.prev = freeblock;
    sf_free_list_heads[index].body.links.next = freeblock;
    sf_header *nextHeader = block + size + sizeof(sf_footer);
    *nextHeader ^= MAGIC;
    if ((*nextHeader & PREV_BLOCK_ALLOCATED) == PREV_BLOCK_ALLOCATED)
    {
        *nextHeader ^= PREV_BLOCK_ALLOCATED;
    }
    size_t nextSize = (*nextHeader >> 3) << 3;
    if ((*nextHeader & THIS_BLOCK_ALLOCATED) != THIS_BLOCK_ALLOCATED)
    {
        if (nextSize != 0)
        {
            sf_footer *nextFooter = block + size + nextSize;
            *nextFooter = *nextHeader;
            *nextFooter ^= MAGIC;
        }
    }
    *nextHeader ^= MAGIC;
    return freeblock;
}

void coalesce(void *block)
{
    sf_block *freeblock = block;
    freeblock->header ^= MAGIC;
    size_t size = (freeblock->header >> 3) << 3;
    size_t totalSize = size;
    freeblock->body.links.prev->body.links.next = freeblock->body.links.next;
    freeblock->body.links.next->body.links.prev = freeblock->body.links.prev;
    freeblock->body.links.next = NULL;
    freeblock->body.links.prev = NULL;
    sf_block *startBlock = block;
    if ((freeblock->header & PREV_BLOCK_ALLOCATED) != PREV_BLOCK_ALLOCATED)
    {
        if ((void*)freeblock >= (sf_mem_start() + 40))
        {
            freeblock->prev_footer ^= MAGIC;
            size_t prevSize = (freeblock->prev_footer >> 3) << 3;
            freeblock->prev_footer ^= MAGIC;
            startBlock = block - prevSize;
            totalSize += prevSize;
            sf_block *prevBlock = block - prevSize;
            prevBlock->body.links.prev->body.links.next = prevBlock->body.links.next;
            prevBlock->body.links.next->body.links.prev = prevBlock->body.links.prev;
        }
    }
    freeblock->header ^= MAGIC;
    sf_block *nextBlock = block + size;
    nextBlock->header ^= MAGIC;
    if ((nextBlock->header & THIS_BLOCK_ALLOCATED) != THIS_BLOCK_ALLOCATED)
    {

        size_t nextSize = (nextBlock->header >> 3) << 3;
        totalSize += nextSize;

        nextBlock->body.links.prev->body.links.next = nextBlock->body.links.next;
        nextBlock->body.links.next->body.links.prev = nextBlock->body.links.prev;
        nextBlock->body.links.next = NULL;
        nextBlock->body.links.prev = NULL;
    }
    else
    {
        nextBlock->header ^= MAGIC;
    }
    setFree((void*) startBlock, totalSize);
}

sf_block *split(void *block, size_t size)
{
    sf_block *freeblock = block;
    freeblock->header ^= MAGIC;
    size_t originalSize = (freeblock->header>> 3) << 3;
    freeblock->header = size;
    freeblock->header |= THIS_BLOCK_ALLOCATED;
    freeblock->header |= PREV_BLOCK_ALLOCATED;
    freeblock->header ^= MAGIC;
    sf_block *newBlock = block + size;
    newBlock->header |= PREV_BLOCK_ALLOCATED;
    newBlock->header ^= MAGIC;
    setFree(block + size, originalSize - size);

    return block;
}

void *sf_malloc(size_t size) {
    if (size == 0)
    {
        return NULL;
    }
    if (sf_mem_end() - sf_mem_start() == 0)
    {
        if (sf_mem_grow() == NULL)
        {
            sf_errno = ENOMEM;
            return NULL;
        }
        sf_block *prologue = sf_mem_start();
        prologue->header = 32;
        prologue->header |= THIS_BLOCK_ALLOCATED;
        prologue->header |= PREV_BLOCK_ALLOCATED;
        prologue->header ^= MAGIC;

        sf_block *epilogue = sf_mem_end() - sizeof(sf_header) - sizeof(sf_footer);
        epilogue->header = 0;
        epilogue->header |= THIS_BLOCK_ALLOCATED;
        epilogue->header ^= MAGIC;
        for (int i = 0; i < NUM_FREE_LISTS; i++)
        {
            sf_free_list_heads[i].body.links.next = &sf_free_list_heads[i];
            sf_free_list_heads[i].body.links.prev = &sf_free_list_heads[i];
        }
        sf_block *newBlock = sf_mem_start() + 32;
        newBlock->header = 0;
        newBlock->header |= PREV_BLOCK_ALLOCATED;
        newBlock->header ^= MAGIC;
        setFree(sf_mem_start() + 32, 8144);
        //epilogue->prev_footer = *((sf_footer*) freeblock + 8144 - sizeof(sf_footer));
        //epilogue->prev_footer ^= MAGIC;
    }
    size_t finalSize = 0;
    if (size + 8 < 32)
    {
        finalSize = 32;
    }
    else
    {
        int multiplier = ((size + 8) / 16);
        if ((size + 8) % 16 != 0)
        {
            multiplier++;
        }
        finalSize = 16 * multiplier;
    }
    sf_block *allocatedBlock = NULL;
    if (finalSize <= 176)
    {
        if (sf_quick_lists[(finalSize - 32) / 16].length != 0)
        {
            allocatedBlock = sf_quick_lists[(finalSize - 32) / 16].first;
            sf_quick_lists[(finalSize - 32) / 16].length--;
            sf_quick_lists[(finalSize - 32) / 16].first = sf_quick_lists[(finalSize - 32) / 16].first->body.links.next;
            allocatedBlock->header ^= MAGIC;
            allocatedBlock->header ^= IN_QUICK_LIST;
            allocatedBlock->header ^= MAGIC;
            return &(allocatedBlock->body.payload);
        }
    }

    int index = 0;
    if (finalSize == 32)
    {
        index = 0;
    }
    else if (finalSize <= 64)
    {
        index = 1;
    }
    else if (finalSize <= 128)
    {
        index = 2;
    }
    else if (finalSize <= 256)
    {
        index = 3;
    }
    else if (finalSize <= 512)
    {
        index = 4;
    }
    else if(finalSize <= 1024)
    {
        index = 5;
    }
    else if(finalSize <= 2048)
    {
        index = 6;
    }
    else if(finalSize <= 4096)
    {
        index = 7;
    }
    else if(finalSize <= 8192)
    {
        index = 8;
    }
    else
    {
        index = 9;
    }
    sf_block *pointer = sf_free_list_heads[index].body.links.next;
    while (index <= 9)
    {
        while (pointer != &sf_free_list_heads[index])
        {
            if ((pointer->header ^ MAGIC) >= finalSize)
            {
                allocatedBlock = pointer;
                break;
            }
            else
            {
                pointer = pointer->body.links.next;
            }
        }
        index++;
        pointer = sf_free_list_heads[index].body.links.next;
    }
    while (allocatedBlock == NULL)
    {
        sf_block *newPage;
        if ((newPage = sf_mem_grow()) == NULL)
        {
            sf_errno = ENOMEM;
            return NULL;
        }
        newPage = (void*) newPage - sizeof(sf_header) - sizeof(sf_footer);
        sf_block *epilogue = sf_mem_end() - sizeof(sf_header) - sizeof(sf_footer);
        epilogue->header = 0;
        epilogue->header |= THIS_BLOCK_ALLOCATED;
        epilogue->header ^= MAGIC;
        setFree(newPage, 8192);
        coalesce(newPage);
        sf_block *newPageBlock = sf_free_list_heads[9].body.links.next;
        size_t blockSize = ((newPageBlock->header ^ MAGIC) >> 3) << 3;
        if (blockSize >= finalSize)
        {
            allocatedBlock = newPageBlock;
        }
    }
    sf_free_list_heads[index].body.links.next = allocatedBlock->body.links.next;
    allocatedBlock->body.links.prev->body.links.next = allocatedBlock->body.links.next;
    allocatedBlock->header ^= MAGIC;
    size_t blockSize = (allocatedBlock->header >> 3) << 3;
    allocatedBlock->header ^= THIS_BLOCK_ALLOCATED;
    allocatedBlock->header ^= MAGIC;
    if (blockSize - finalSize >= 32)
    {
        allocatedBlock = split(allocatedBlock, finalSize);
    }
    totalPayload = totalPayload + finalSize - sizeof(sf_header);
    return &(allocatedBlock->body.payload);
}

void flush(int index)
{
    sf_block *pointer = sf_quick_lists[index].first;
    for (int i = 0; i < QUICK_LIST_MAX; i++)
    {
        sf_block *block = pointer;
        pointer = pointer->body.links.next;
        block->header ^= MAGIC;
        size_t size = (block->header >> 3) << 3;
        block->header ^= MAGIC;
        setFree((void*)block, size);
        coalesce((void*)block);
    }
    sf_quick_lists[index].first = NULL;
    sf_quick_lists[index].length = 0;
}

void sf_free(void *pp) {
    if (pp == NULL || ((uintptr_t) pp & 0xF) != 0)
    {
        abort();
    }
    sf_block *block = pp - sizeof(sf_header) - sizeof(sf_footer);
    block->header ^= MAGIC;
    size_t size = (block->header >> 3) << 3;
    if (size < 32 || size % 16 != 0)
    {
        abort();
    }
    if (pp < sf_mem_start() + 40 || pp  > sf_mem_end() - 40)
    {
        abort();
    }
    if ((block->header & THIS_BLOCK_ALLOCATED) != THIS_BLOCK_ALLOCATED ||
        (block->header & IN_QUICK_LIST) == IN_QUICK_LIST)
    {
        abort();
    }
    if ((block->header & PREV_BLOCK_ALLOCATED) != PREV_BLOCK_ALLOCATED)
    {
        if (((block->prev_footer ^ MAGIC) & THIS_BLOCK_ALLOCATED) == THIS_BLOCK_ALLOCATED)
        {
            abort();
        }
    }
    if (size <= 176)
    {
        if (sf_quick_lists[(size - 32) / 16].length == QUICK_LIST_MAX)
        {
            block->header ^= MAGIC;
            flush((size - 32) / 16);
            block->header ^= MAGIC;
        }
        block->body.links.next = sf_quick_lists[(size - 32) / 16].first;
        sf_quick_lists[(size - 32) / 16].first = block;
        sf_quick_lists[(size - 32) / 16].length++;
        block->header ^= IN_QUICK_LIST;
        block->header ^= MAGIC;
        return;
    }
    block->header ^= MAGIC;
    setFree(pp - sizeof(sf_header) - sizeof(sf_footer), size);
    coalesce(pp - sizeof(sf_header) - sizeof(sf_footer));
    totalPayload -= size + sizeof(sf_header);
}

void *sf_realloc(void *pp, size_t rsize) {
    if (pp == NULL || ((uintptr_t) pp & 0xF) != 0)
    {
        sf_errno = EINVAL;
        return NULL;
    }
    sf_block *block = pp - sizeof(sf_header) - sizeof(sf_footer);
    block->header ^= MAGIC;
    size_t size = (block->header >> 3) << 3;
    if (size < 32 || size % 16 != 0)
    {
        sf_errno = EINVAL;
        return NULL;
    }
    if (pp < sf_mem_start() + 40 || pp  > sf_mem_end() - 40)
    {
        sf_errno = EINVAL;
        return NULL;
    }
    if ((block->header & THIS_BLOCK_ALLOCATED) != THIS_BLOCK_ALLOCATED ||
        (block->header & IN_QUICK_LIST) == IN_QUICK_LIST)
    {
        sf_errno = EINVAL;
        return NULL;
    }
    if ((block->header & PREV_BLOCK_ALLOCATED) != PREV_BLOCK_ALLOCATED)
    {
        if (((block->prev_footer ^ MAGIC) & THIS_BLOCK_ALLOCATED) == THIS_BLOCK_ALLOCATED)
        {
            sf_errno = EINVAL;
            return NULL;
        }
    }
    block->header ^= MAGIC;
    if (rsize == 0)
    {
        sf_free(block);
        return NULL;
    }
    size_t finalSize = 0;
    if (rsize + 8 < 32)
    {
        finalSize = 32;
    }
    else
    {
        int multiplier = ((rsize + 8) / 16);
        if ((rsize + 8) % 16 != 0)
        {
            multiplier++;
        }
        finalSize = 16 * multiplier;
    }
    if (finalSize > size)
    {
        void* *newBlock = sf_malloc(rsize);
        if (newBlock == NULL)
        {
            return NULL;
        }
        memcpy(newBlock, &(block->body.payload), size);
        sf_free(&(block->body.payload));
        return newBlock;
    }
    else
    {
        if (size - finalSize >= 32)
        {
            sf_block *splitBlock = (void* )split(block, finalSize) + finalSize;
            coalesce(splitBlock);
            totalPayload = totalPayload - (size - finalSize) + sizeof(sf_header);
        }
    }
    return &(block->body.payload);
}

double sf_fragmentation() {
    // To be implemented.
    abort();
}

double sf_utilization() {
    if (sf_mem_end() - sf_mem_start() == 0)
    {
        return 0.0;
    }
    double utilization = totalPayload / (sf_mem_end() - sf_mem_start());
    if (utilization > maxUtilization)
    {
        maxUtilization = utilization;
    }
    return maxUtilization;
}
