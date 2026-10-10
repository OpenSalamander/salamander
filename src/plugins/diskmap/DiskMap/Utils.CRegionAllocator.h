// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <vector>
#define DEFAULT_BLOCK_SIZE 1024 * 1024 * 1 //1MB

class CRegionAllocator
{
protected:
    size_t _blockSize;
    size_t _currentBlock;
    size_t _lastBlockRemSize;
    BYTE* _lastBlockPosition;

    std::vector<std::unique_ptr<BYTE[]>> _blocks;

public:
    CRegionAllocator(size_t blockSize = DEFAULT_BLOCK_SIZE)
    {
        this->_blockSize = blockSize;
        this->_currentBlock = 0;
        this->_lastBlockRemSize = 0;
        this->_lastBlockPosition = NULL;
    }
    ~CRegionAllocator() = default;
    void* Allocate(size_t size)
    {
        if (size > this->_blockSize)
        {
            throw std::bad_alloc();
        }

        if (size > this->_lastBlockRemSize)
        {
            if (this->_currentBlock == this->_blocks.size())
            {
                this->_blocks.push_back(std::make_unique_for_overwrite<BYTE[]>(this->_blockSize));
            }
            this->_lastBlockRemSize = this->_blockSize;
            this->_lastBlockPosition = this->_blocks[this->_currentBlock].get();
            this->_currentBlock++;
        }

        void* p = this->_lastBlockPosition;
        this->_lastBlockPosition += size;
        this->_lastBlockRemSize -= size;
        return p;
    }
    void FreeAll(BOOL deallocate = TRUE)
    {
        this->_currentBlock = 0;
        this->_lastBlockRemSize = 0;
        this->_lastBlockPosition = NULL;
        if (deallocate)
        {
            this->_blocks.clear();
        }
    }
};
