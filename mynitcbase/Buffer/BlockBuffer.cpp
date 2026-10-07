#include "BlockBuffer.h"
#include <cstdlib>
#include <cstring>
#include "../define/constants.h"

// calls the parent class constructor
RecBuffer::RecBuffer(int blockNum) : BlockBuffer::BlockBuffer(blockNum) {}

BlockBuffer::BlockBuffer(int blockNum) {
  // initialise this.blockNum with the argument
  this->blockNum = blockNum;
}

/*
Used to load a block to the buffer and get a pointer to it.
NOTE: this function expects the caller to allocate memory for the argument
*/
int BlockBuffer::loadBlockAndGetBufferPtr(unsigned char ** buffPtr) {
  /* check whether the block is already present in the buffer
     using StaticBuffer.getBufferNum() */
  int bufferNum = StaticBuffer::getBufferNum(this->blockNum);

  if (bufferNum != E_BLOCKNOTINBUFFER) {
    // if present, increment the timestamps of all other occupied buffers
    for (int i = 0; i < BUFFER_CAPACITY; i++) {
      if (!StaticBuffer::metainfo[i].free) {
        StaticBuffer::metainfo[i].timeStamp++;
      }
    }
    // and set the timestamp of the corresponding buffer to 0
    StaticBuffer::metainfo[bufferNum].timeStamp = 0;
  } else {
    // else get a free buffer using StaticBuffer.getFreeBuffer()
    bufferNum = StaticBuffer::getFreeBuffer(this->blockNum);

    // if the call returns E_OUTOFBOUND, return E_OUTOFBOUND here as the blockNum is invalid
    if (bufferNum == E_OUTOFBOUND) {
      return E_OUTOFBOUND;
    }

    // Read the block into the free buffer using readBlock()
    Disk::readBlock(StaticBuffer::blocks[bufferNum], this->blockNum);
  }

  // store the pointer to this buffer (blocks[bufferNum]) in *buffPtr
  *buffPtr = StaticBuffer::blocks[bufferNum];

  return SUCCESS;
}

int BlockBuffer::setHeader(struct HeadInfo *head) {
  unsigned char *bufferPtr;
  
  // get the starting address of the buffer containing the block
  int ret = loadBlockAndGetBufferPtr(&bufferPtr);
  if (ret != SUCCESS) {
    return ret;
  }

  // cast bufferPtr to type HeadInfo*
  struct HeadInfo *bufferHeader = (struct HeadInfo *)bufferPtr;

  // copy the fields of the HeadInfo pointed to by head (except reserved)
  bufferHeader->blockType = head->blockType;
  bufferHeader->pblock = head->pblock;
  bufferHeader->lblock = head->lblock;
  bufferHeader->rblock = head->rblock;
  bufferHeader->numEntries = head->numEntries;
  bufferHeader->numAttrs = head->numAttrs;
  bufferHeader->numSlots = head->numSlots;

  // update dirty bit by calling StaticBuffer::setDirtyBit()
  int dirtyRet = StaticBuffer::setDirtyBit(this->blockNum);
  if (dirtyRet != SUCCESS) {
    return dirtyRet;
  }

  return SUCCESS;
}

int BlockBuffer::setBlockType(int blockType) {
  unsigned char *bufferPtr;
  
  // get the starting address of the buffer containing the block
  int ret = loadBlockAndGetBufferPtr(&bufferPtr);
  if (ret != SUCCESS) {
    return ret;
  }

  // store the input block type in the first 4 bytes of the buffer.
  // cast bufferPtr to int32_t* and then assign it
  *((int32_t *)bufferPtr) = blockType;

  // update the StaticBuffer::blockAllocMap entry corresponding to the
  // object's block number to `blockType`.
  StaticBuffer::blockAllocMap[this->blockNum] = blockType;

  // update dirty bit by calling StaticBuffer::setDirtyBit()
  int dirtyRet = StaticBuffer::setDirtyBit(this->blockNum);
  if (dirtyRet != SUCCESS) {
    return dirtyRet;
  }

  return SUCCESS;
}

int BlockBuffer::getFreeBlock(int blockType) {
  int freeBlockNum = -1;

  // iterate through the StaticBuffer::blockAllocMap and find the block number
  // of a free block in the disk.
  for (int i = 0; i < DISK_BLOCKS; i++) {
    if (StaticBuffer::blockAllocMap[i] == UNUSED_BLK) {
      freeBlockNum = i;
      break;
    }
  }

  // if no block is free, return E_DISKFULL.
  if (freeBlockNum == -1) {
    return E_DISKFULL;
  }

  // set the object's blockNum to the block number of the free block.
  this->blockNum = freeBlockNum;

  // find a free buffer using StaticBuffer::getFreeBuffer() .
  // This claims a RAM slot for the newly allocated disk block.
  StaticBuffer::getFreeBuffer(this->blockNum);

  // initialize the header of the block passing a struct HeadInfo
  struct HeadInfo head;
  head.pblock = -1;
  head.lblock = -1;
  head.rblock = -1;
  head.numEntries = 0;
  head.numAttrs = 0;
  head.numSlots = 0;

  // to the setHeader() function.
  this->setHeader(&head);

  // update the block type of the block to the input block type using setBlockType().
  this->setBlockType(blockType);

  // return block number of the free block.
  return freeBlockNum;
}

BlockBuffer::BlockBuffer(char blockType) {
  // allocate a block on the disk and a buffer in memory to hold the new block of
  // given type using getFreeBlock function and get the return error codes if any.
  int allocatedBlock = this->getFreeBlock(blockType);

  // set the blockNum field of the object to that of the allocated block
  // number if the method returned a valid block number,
  // otherwise set the error code returned as the block number.
  this->blockNum = allocatedBlock;
}

RecBuffer::RecBuffer() : BlockBuffer('R') {}

int RecBuffer::setRecord(union Attribute *rec, int slotNum) {
  unsigned char *bufferPtr;
  
  // get the starting address of the buffer containing the block
  int ret = loadBlockAndGetBufferPtr(&bufferPtr);
  if (ret != SUCCESS) {
    return ret;
  }

  // get the header of the block
  HeadInfo head;
  this->getHeader(&head);

  // get number of attributes and slots
  int numAttrs = head.numAttrs;
  int numSlots = head.numSlots;

  // if input slotNum is not in the permitted range
  if (slotNum < 0 || slotNum >= numSlots) {
    return E_OUTOFBOUND;
  }

  // offset bufferPtr to point to the beginning of the record at required slot.
  // Note: The slot map takes up exactly 'numSlots' bytes immediately after the header.
  int recordSize = numAttrs * ATTR_SIZE;
  unsigned char *slotPointer = bufferPtr + HEADER_SIZE + numSlots + (slotNum * recordSize);

  // copy the record from `rec` to buffer
  memcpy(slotPointer, rec, recordSize);

  // update dirty bit using setDirtyBit()
  StaticBuffer::setDirtyBit(this->blockNum);

  return SUCCESS;
}

/*
Used to get the header of the block into the location pointed to by `head`
NOTE: this function expects the caller to allocate memory for `head`
*/
int BlockBuffer::getHeader(struct HeadInfo *head) {
  unsigned char *bufferPtr;
  
  // Load the block and get the pointer to the cached memory
  int ret = loadBlockAndGetBufferPtr(&bufferPtr);
  if (ret != SUCCESS) {
    return ret;   // return any errors that might have occured in the process
  }

  // populate the header fields by reading from the cached bufferPtr
  memcpy(&head->blockType, bufferPtr, 4);    // Bytes 0-3 store block type
  memcpy(&head->pblock, bufferPtr + 4, 4);     // Bytes 4-7 store parent block pointer
  memcpy(&head->lblock, bufferPtr + 8, 4);     // Bytes 8-11 store left block pointer
  memcpy(&head->rblock, bufferPtr + 12, 4);    // Bytes 12-15 store right block pointer
  memcpy(&head->numEntries, bufferPtr + 16, 4); // Bytes 16-19 store number of records
  memcpy(&head->numAttrs, bufferPtr + 20, 4);  // Bytes 20-23 store number of attributes
  memcpy(&head->numSlots, bufferPtr + 24, 4);  // Bytes 24-27 store number of slots

  return SUCCESS;
}

int RecBuffer::setSlotMap(unsigned char *slotMap) {
    unsigned char *bufferPtr;
    
    // get the starting address of the buffer containing the block
    int ret = loadBlockAndGetBufferPtr(&bufferPtr);
    if (ret != SUCCESS) {
        return ret;
    }

    // get the header of the block using the getHeader() function
    HeadInfo head;
    this->getHeader(&head);

    int numSlots = head.numSlots;

    // the slotmap starts at bufferPtr + HEADER_SIZE. Copy the contents
    memcpy(bufferPtr + HEADER_SIZE, slotMap, numSlots);

    // update dirty bit using StaticBuffer::setDirtyBit
    int dirtyRet = StaticBuffer::setDirtyBit(this->blockNum);
    if (dirtyRet != SUCCESS) {
        return dirtyRet;
    }

    return SUCCESS;
}

int BlockBuffer::getBlockNum() {
    // return corresponding block number.
    return this->blockNum;
}
/*
Used to get the record at slot `slotNum` into the array `rec`
NOTE: this function expects the caller to allocate memory for `rec`
*/
int RecBuffer::getRecord(union Attribute *rec, int slotNum) {
  struct HeadInfo head;
  
  // get the header using this.getHeader() function
  this->getHeader(&head);

  int attrCount = head.numAttrs;
  int slotCount = head.numSlots;

  unsigned char *bufferPtr;
  
  // Load the block and get the pointer to the cached memory
  int ret = loadBlockAndGetBufferPtr(&bufferPtr);
  if (ret != SUCCESS) {
    return ret;
  }

  /* record at slotNum will be at offset HEADER_SIZE + slotMapSize + (recordSize * slotNum)
     - each record will have size attrCount * ATTR_SIZE
     - slotMap will be of size slotCount
  */
  int recordSize = attrCount * ATTR_SIZE;
  
  // HEADER_SIZE in NITCbase is typically 32 bytes
  unsigned char *slotPointer = bufferPtr + 32 + slotCount + (recordSize * slotNum);

  // load the record into the rec data structure
  memcpy(rec, slotPointer, recordSize);

  return SUCCESS;
}

/* used to get the slotmap from a record block
NOTE: this function expects the caller to allocate memory for `*slotMap`
*/
int RecBuffer::getSlotMap(unsigned char *slotMap) {
  unsigned char *bufferPtr;

  // get the starting address of the buffer containing the block using loadBlockAndGetBufferPtr().
  int ret = loadBlockAndGetBufferPtr(&bufferPtr);
  if (ret != SUCCESS) {
    return ret;
  }

  struct HeadInfo head;
  
  // get the header of the block using getHeader() function
  // We can call getHeader() directly because RecBuffer inherits from BlockBuffer
  this->getHeader(&head);

  // number of slots in block from header
  int slotCount = head.numSlots; 

  // get a pointer to the beginning of the slotmap in memory by offsetting HEADER_SIZE
  // HEADER_SIZE is defined as 32 bytes in NITCbase
  unsigned char *slotMapInBuffer = bufferPtr + HEADER_SIZE;

  // copy the values from `slotMapInBuffer` to `slotMap` (size is `slotCount`)
  memcpy(slotMap, slotMapInBuffer, slotCount);

  return SUCCESS;
}

int compareAttrs(union Attribute attr1, union Attribute attr2, int attrType) {
    double diff;

    // if attrType == STRING
    if (attrType == STRING) {
        // strcmp returns > 0 if str1 > str2, < 0 if str1 < str2, and 0 if they match
        diff = strcmp(attr1.sVal, attr2.sVal);
    } 
    // else (it is a NUMBER)
    else {
        diff = attr1.nVal - attr2.nVal;
    }

    // if diff > 0 then return 1
    if (diff > 0) {
        return 1;
    } 
    // if diff < 0 then return -1
    else if (diff < 0) {
        return -1;
    } 
    // if diff = 0 then return 0
    else {
        return 0;
    }
}

void BlockBuffer::releaseBlock() {
    // if blockNum is INVALID_BLOCKNUM (-1), or it is invalidated already, do nothing
    if (this->blockNum == INVALID_BLOCKNUM) {
        return;
    }

    // get the buffer number of the buffer assigned to the block
    int bufferNum = StaticBuffer::getBufferNum(this->blockNum);

    // if the block is present in the buffer, free the buffer
    if (bufferNum != E_BLOCKNOTINBUFFER) {
        StaticBuffer::metainfo[bufferNum].free = true;
    }

    // free the block in disk by setting the data type of the entry
    // corresponding to the block number in StaticBuffer::blockAllocMap to UNALLOCATED
    StaticBuffer::blockAllocMap[this->blockNum] = UNUSED_BLK;

    // set the object's blockNum to INVALID_BLOCKNUM (-1)
    this->blockNum = INVALID_BLOCKNUM;
}