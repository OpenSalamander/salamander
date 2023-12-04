// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#define MAX_BLOCK_COUNT 256
#define DEFAULT_BLOCK_SIZE 1024 * 1024 * 1 //1MB

class CRegionAllocator
{
protected:
    size_t _blockSize;
    int _blockCount;

    int _currentBlock;
    size_t _lastBlockRemSize;
    BYTE* _lastBlockPosition;

    BYTE* _blocks[MAX_BLOCK_COUNT];

public:
    CRegionAllocator(size_t blockSize = DEFAULT_BLOCK_SIZE)
    {
        this->_blockSize = blockSize;
        this->_currentBlock = -1;
        this->_blockCount = 0;
        this->_lastBlockRemSize = 0;
        this->_lastBlockPosition = NULL;
    }
    ~CRegionAllocator()
    {
        for (int i = 0; i < this->_blockCount; i++)
        {
            free(this->_blocks[i]);
        }
        this->_blockCount = 0;
    }
    void* Allocate(size_t size)
    {
#ifdef _DEBUG
        if (size > this->_blockSize)
        {
            //			throw std::bad_alloc();
            return NULL;
        }
#endif

        if (size > this->_lastBlockRemSize)
        {
            this->_currentBlock++;
            if (this->_currentBlock == this->_blockCount)
            {
                this->_blocks[this->_blockCount] = (BYTE*)malloc(this->_blockSize);
                if (this->_blocks[this->_blockCount] == NULL)
                {
                    this->_currentBlock--;
                    //					throw std::bad_alloc();
                    return NULL;
                }
                this->_blockCount++;
            }
            this->_lastBlockRemSize = this->_blockSize;
            this->_lastBlockPosition = this->_blocks[this->_currentBlock];
        }

        void* p = this->_lastBlockPosition;
        this->_lastBlockPosition += size;
        this->_lastBlockRemSize -= size;
        return p;
    }
    void FreeAll(BOOL deallocate = TRUE)
    {
        this->_currentBlock = -1;
        this->_lastBlockRemSize = 0;
        this->_lastBlockPosition = NULL;
        if (deallocate)
        {
            for (int i = 0; i < this->_blockCount; i++)
            {
                free(this->_blocks[i]);
            }
            this->_blockCount = 0;
        }
    }
};
