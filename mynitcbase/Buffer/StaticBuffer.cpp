#include "StaticBuffer.h"

// the declarations for this class can be found at "StaticBuffer.h"

unsigned char StaticBuffer::blocks[BUFFER_CAPACITY][BLOCK_SIZE];
struct BufferMetaInfo StaticBuffer::metainfo[BUFFER_CAPACITY];

// declare the blockAllocMap array
unsigned char StaticBuffer::blockAllocMap[DISK_BLOCKS];

StaticBuffer::StaticBuffer() {
  // copy blockAllocMap blocks from disk to buffer (using readblock() of disk)
  // blocks 0 to 3
  for (int i = 0; i < 4; i++) {
    // BLOCK_SIZE is 2048. We read 4 blocks to cover the 8192 bytes of the array.
    Disk::readBlock(&blockAllocMap[i * BLOCK_SIZE], i);
  }

  // initialise metainfo of all the buffer blocks
  for (int bufferIndex = 0; bufferIndex < BUFFER_CAPACITY; bufferIndex++) {
    metainfo[bufferIndex].free = true;
    metainfo[bufferIndex].dirty = false;
    metainfo[bufferIndex].timeStamp = -1;
    metainfo[bufferIndex].blockNum = -1;
  }
}

StaticBuffer::~StaticBuffer() {
  // copy blockAllocMap blocks from buffer to disk(using writeblock() of disk)
  for (int i = 0; i < 4; i++) {
    Disk::writeBlock(&blockAllocMap[i * BLOCK_SIZE], i);
  }

  // iterate through all the buffer blocks, write back blocks with metainfo as free:false, dirty:true
  for (int bufferIndex = 0; bufferIndex < BUFFER_CAPACITY; bufferIndex++) {
    if (!metainfo[bufferIndex].free && metainfo[bufferIndex].dirty) {
      Disk::writeBlock(blocks[bufferIndex], metainfo[bufferIndex].blockNum);
    }
  }
}

int StaticBuffer::getFreeBuffer(int blockNum) {
  // Check if blockNum is valid (non zero and less than DISK_BLOCKS)
  if (blockNum < 0 || blockNum >= DISK_BLOCKS) {
    return E_OUTOFBOUND;
  }

  // increase the timeStamp in metaInfo of all occupied buffers.
  for (int i = 0; i < BUFFER_CAPACITY; i++) {
    if (!metainfo[i].free) {
      metainfo[i].timeStamp++;
    }
  }

  int bufferNum = -1;

  // iterate through metainfo and check if there is any buffer free
  for (int i = 0; i < BUFFER_CAPACITY; i++) {
    if (metainfo[i].free) {
      bufferNum = i;
      break;
    }
  }

  // if a free buffer is not available, find the buffer with the largest timestamp
  if (bufferNum == -1) {
    int maxTimestamp = -1;
    
    // Find the LRU (Least Recently Used) buffer
    for (int i = 0; i < BUFFER_CAPACITY; i++) {
      if (!metainfo[i].free && metainfo[i].timeStamp > maxTimestamp) {
        maxTimestamp = metainfo[i].timeStamp;
        bufferNum = i;
      }
    }

    // IF IT IS DIRTY, write back to the disk using Disk::writeBlock()
    if (metainfo[bufferNum].dirty) {
      Disk::writeBlock(blocks[bufferNum], metainfo[bufferNum].blockNum);
    }
  }

  // update the metaInfo entry corresponding to bufferNum with
  // free:false, dirty:false, blockNum:the input block number, timeStamp:0.
  metainfo[bufferNum].free = false;
  metainfo[bufferNum].dirty = false;
  metainfo[bufferNum].blockNum = blockNum;
  metainfo[bufferNum].timeStamp = 0;

  return bufferNum;
}

/* Get the buffer index where a particular block is stored
   or E_BLOCKNOTINBUFFER otherwise
*/
int StaticBuffer::getBufferNum(int blockNum) {
  // Check if blockNum is valid (between zero and DISK_BLOCKS - 1)
  // and return E_OUTOFBOUND if not valid.
  if (blockNum < 0 || blockNum >= DISK_BLOCKS) {
    return E_OUTOFBOUND;
  }

  // find and return the bufferIndex which corresponds to blockNum (check metainfo)
  for (int i = 0; i < BUFFER_CAPACITY; i++) {
    // A block matches if the buffer is NOT free AND the blockNum matches
    if (metainfo[i].free == false && metainfo[i].blockNum == blockNum) {
      return i;
    }
  }

  // if block is not in the buffer
  return E_BLOCKNOTINBUFFER;
}

int StaticBuffer::setDirtyBit(int blockNum) {
  // find the buffer index corresponding to the block using getBufferNum().
  int bufferNum = getBufferNum(blockNum);

  // if block is not present in the buffer
  if (bufferNum == E_BLOCKNOTINBUFFER) {
    return E_BLOCKNOTINBUFFER;
  }

  // if blockNum is out of bounds
  if (bufferNum == E_OUTOFBOUND) {
    return E_OUTOFBOUND;
  }

  // else (the bufferNum is valid)
  // set the dirty bit of that buffer to true in metainfo
  metainfo[bufferNum].dirty = true;

  return SUCCESS;
}