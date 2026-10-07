#include "BlockBuffer.h"

#include <cstdlib>
#include <cstring>

// BlockBuffer(char blockType)  [constructor 1]
BlockBuffer::BlockBuffer(char blockType){
    /*
        allocate a block on the disk and a buffer in the memory to hold the new block of
        given type using getFreeBlock() and get the return error codes if any.
    */
    int intBlockType;
    if(blockType == 'R'){
        intBlockType = REC;
    }
    else if(blockType == 'I'){
        intBlockType = IND_INTERNAL;
    }
    else{
        intBlockType = IND_LEAF;
    }

    int ret = BlockBuffer::getFreeBlock(intBlockType);
    
    // set the object's blockNum to the return value.
    // if return value is an error code, set blockNum as the returned error code.
    this->blockNum = ret;
}

// BlockBuffer(int blockNum)    [constructor 2]
BlockBuffer :: BlockBuffer(int blockNum){
    // initialise this.blockNum with the argument
    this->blockNum = blockNum;
}

// RecBuffer()  [constructor 1]
RecBuffer::RecBuffer() : BlockBuffer('R'){}

// RecBuffer(int blockNum), calls the parent class constructor [constructor 2]
RecBuffer :: RecBuffer(int blockNum) : BlockBuffer :: BlockBuffer(blockNum) {}

/*
    Used to get the header of the block into the location pointed to by 'head'
    NOTE: This function expects the caller to allocate memory for 'head'
*/
int BlockBuffer :: getHeader(struct HeadInfo *head) {

    unsigned char *bufferPtr;
    int ret = loadBlockAndGetBufferPtr(&bufferPtr);
    if(ret != SUCCESS){
        return ret;
    }

    // populate the numEntries, numAttrs and numSlots fields in *head
    memcpy(&head->numSlots, bufferPtr + 24, 4);
    memcpy(&head->numEntries, bufferPtr + 16, 4);
    memcpy(&head->numAttrs, bufferPtr + 20, 4);
    memcpy(&head->rblock, bufferPtr + 12, 4);
    memcpy(&head->lblock, bufferPtr + 8, 4);

    return SUCCESS;
}

/*
    BlockBuffer::setHeader() => used to set the header of the block
*/
int BlockBuffer::setHeader(struct HeadInfo *head){

    unsigned char *bufferPtr;
    int ret = BlockBuffer::loadBlockAndGetBufferPtr(&bufferPtr);
    if(ret != SUCCESS){
        return ret;
    }

    // caste bufferPtr to type HeadInfo*
    struct HeadInfo *bufferHead = (struct HeadInfo *)bufferPtr;

    // copy the fields of the HeadInfo pointed to by head to the header of the block (pointed by bufferHead)
    // bufferHead->blockType = head->blockType;
    bufferHead->pblock = head->pblock;
    bufferHead->lblock = head->lblock;
    bufferHead->rblock = head->rblock;
    bufferHead->numEntries = head->numEntries;
    bufferHead->numAttrs = head->numAttrs;
    bufferHead->numSlots = head->numSlots;

    // update dirty bit by calling setDirtyBit()
    ret = StaticBuffer::setDirtyBit(this->blockNum);
    return ret; // either success or the error code will be returned.
}

/*
    BlockBuffer::setBlockType()
    set the type of the block with the input block type. This method sets the type in both the
    header of the block and also in the block allocation map.
*/
int BlockBuffer::setBlockType(int blockType){

    unsigned char *bufferPtr;
    int ret = BlockBuffer::loadBlockAndGetBufferPtr(&bufferPtr);
    if(ret != SUCCESS){
        return ret;
    }

    // blocktype is the 1st 4 bytes of the header, assign bufferPtr 1st 4 bytes with arg blockType
    *((int32_t *)bufferPtr) = blockType;

    // update the StaticBuffer::blockAllocMap entry corresponding to the object's block number to `blockType`.
    StaticBuffer::blockAllocMap[this->blockNum] = blockType;

    // update the dirty bit by calling setDirtyBit()
    ret = StaticBuffer::setDirtyBit(this->blockNum);
    return ret; // this will return ERRORs or SUCCESS
}

/*
    BlockBuffer::getFreeBlock()
    1. Returns the block number of a free block. 
    2. It setups the header of the block with the input block type and updates the block allocation map with the same.
    3. A buffer is also allocated to the block. (try to answer why?)
    4. If a disk full, return E_DISKFULL is returned.
*/
int BlockBuffer::getFreeBlock(int blockType){
    //iterate through the StaticBuffer::blockAllocMap and find the block number of the free disk.
    int freeBlock = -1;
    for(int i = 0; i<DISK_BLOCKS; i++){
        if(StaticBuffer::blockAllocMap[i] == UNUSED_BLK){
            freeBlock = i;
            break;
        }
    }
    
    if(freeBlock == DISK_BLOCKS){
        // disk is full;
        return E_DISKFULL;
    }

    // set the object's blockNum to the free block number found.
    this->blockNum = freeBlock;

    // find a free buffer.
    int bufferNum = StaticBuffer::getFreeBuffer(freeBlock);

    // initialize the header of the block passing a struct HeadInfo with values
    // pblock: -1, lblock: -1, rblock: -1, numEntries: 0, numAttrs: 0, numSlots: 0 to setHeader()
    struct HeadInfo head;
    head.pblock = -1;
    head.lblock = -1;
    head.rblock = -1;
    head.numEntries = 0;
    head.numAttrs = 0;
    head.numSlots = 0;

    BlockBuffer::setHeader(&head);

    // update the block type of the block to the input block type using setBlockType()
    BlockBuffer::setBlockType(blockType);

    // return the block number of the free block
    return freeBlock;

}

/*
    Used to load a block to the buffer and get a pointer to it.
    NOTE: this function expects the caller to allocate memory for the argument
*/
int BlockBuffer :: loadBlockAndGetBufferPtr(unsigned char **buffPtr){
    // check whether the block is already present in the buffer using StaticBuffer.getBufferNum();
    int bufferNum = StaticBuffer :: getBufferNum(this->blockNum);

    // if buffer is present
        // set the timeStamp of the corresponding buffer at bufferMetaInfo to 0, rest increment by one
    if(bufferNum != E_BLOCKNOTINBUFFER){
        for(int bufferIndex = 0; bufferIndex < BUFFER_CAPACITY; bufferIndex++){
            if(bufferIndex == bufferNum){
                StaticBuffer::metainfo[bufferIndex].timeStamp = 0;
            }
            else{
                StaticBuffer::metainfo[bufferIndex].timeStamp += 1;
            }
        }
    }
    else{
        bufferNum = StaticBuffer :: getFreeBuffer(this->blockNum);
        if(bufferNum == E_OUTOFBOUND){
            return E_OUTOFBOUND;
        }

        Disk :: readBlock(StaticBuffer :: blocks[bufferNum], this->blockNum);
    }

    // store the pointer to this buffer (blocks[bufferNum]) in *buffPtr
    *buffPtr = StaticBuffer :: blocks[bufferNum];

    return SUCCESS;
}

/*
    getBlockNum(): Returns the block number of the block. Defined to access the private
    member field 'blocknum' of the class.
*/
int BlockBuffer::getBlockNum(){
    return this->blockNum;
}

/*
    releaseBlock(): 
    1.  The block number to which this instance of block is associated is freed
    from the buffer and the disk.
    2.  The blockNum field to the object is invalidated
*/
void BlockBuffer::releaseBlock(){
    // if blockNum is INVALID_BLOCKNUM, or already invalidated, do nothing, else...
    if(this->blockNum != INVALID_BLOCKNUM){
        /* 
            get the buffer number of the buffer assigned to the block.
        */
        int bufferNum = StaticBuffer::getBufferNum(this->blockNum);

        /*
            if block is present in the buffer: 
            1.  free the buffer by setting the free flag of
                tableMetaInfo entry to true.
            2.  Free the block disk by setting the data type of the entry
                corresponding to the block number in blockAllocMap to UNUSED_BLK

                (Don't worry about the slotMap for the block, because if the block is allocated
                for the 1st time is BlockAccess::insert(), we set the header accrodingly and 
                every slot in slotMap as UNOCCUPIED)
        */ 
        if(bufferNum != E_BLOCKNOTINBUFFER){
            StaticBuffer::metainfo[bufferNum].free = true;
            StaticBuffer::blockAllocMap[this->blockNum] = UNUSED_BLK;
        }

        // set the object's blockNum to INVALID_BLOCK
        this->blockNum = INVALID_BLOCKNUM;
    }
}

/*
    Used to get the record at slot 'slotNum' into the array 'rec'
    NOTE: this function expects the caller to allocate memory for 'rec'
*/
int RecBuffer :: getRecord(union Attribute *rec, int slotNum){

    unsigned char *bufferPtr;
    int ret = loadBlockAndGetBufferPtr(&bufferPtr);
    if(ret != SUCCESS){
        return ret;
    }

    struct HeadInfo head;

    // get the header using this.getHeader() function
    this->getHeader(&head);

    int attrCount = head.numAttrs;
    int slotCount = head.numSlots;

    /* record at slotNum will be at offset HEADER_SIZE + slotMapSize + (recordSize * slotNum)
        - each record will have size attrCount * ATTR_SIZE
        - slotMap will be of size slotCount
    */
    int recordSize = attrCount * ATTR_SIZE;
    unsigned char *slotPointer = bufferPtr + HEADER_SIZE + head.numSlots + (recordSize * slotNum);

    // load the record into the rec data structure
    memcpy(rec, slotPointer, recordSize);

    return SUCCESS;
}

/*  
    getSlotMap():
    used to get the slotmap from a record block
    NOTE: this functions expects the caller to allocate memory for "*slotmap"
*/
int RecBuffer::getSlotMap(unsigned char* slotMap){
    unsigned char* bufferPtr;

    // get the starting address of the buffer containing the block using loadBlockAndGetBufferPtr()
    int ret = loadBlockAndGetBufferPtr(&bufferPtr);
    if(ret != SUCCESS){
        return ret;
    }

    struct HeadInfo head;
    this->getHeader(&head);
    int slotCount = head.numSlots;

    // get a pointer to the beginning of the slotmap in memory by offsetting HEADER_SIZE
    unsigned char* slotMapInBuffer = bufferPtr + HEADER_SIZE;

    // copy the values from 'slotMapInBuffer' to 'slotMap' (size is 'slotCount')
    memcpy(slotMap, slotMapInBuffer,slotCount);

    return SUCCESS;
}

/*
    RecBuffer::setRecord()
    Sets the slotNum th record entry of the block with the input record content
*/
int RecBuffer::setRecord(union Attribute *rec, int slotNum){
    unsigned char *bufferPtr;
    /*
        get the starting address of the buffer containing the block
        using loadBlockAndGetBufferPtr(&bufferPtr);
    */
    int res = BlockBuffer::loadBlockAndGetBufferPtr(&bufferPtr);

    if(res != SUCCESS){
        return res;
    }

    /* get the header of the block using the getHeader() */
    struct HeadInfo head;
    this->getHeader(&head);

    // get the number of attributes
    int attrCount = head.numAttrs;

    // get the number of slots in the block
    int slotCount = head.numSlots;

    // if input slotNum is not in the permitted range return E_OUTOFBOUND
    if(slotNum >= slotCount){
        return E_OUTOFBOUND;
    }

    /*
        Offset bufferPtr to point to the beginning of the record at required slot.
        The block contains the header, the slotmap, followed by all the records.
    */
    int recordSize = attrCount * ATTR_SIZE;
    unsigned char *slotPointer = bufferPtr + HEADER_SIZE + slotCount + (slotNum * recordSize);

    // copy the record from rec to the buffer using memcpy
    memcpy(slotPointer, rec, recordSize);

    // update the dirty bit using setDirtyBit()
    StaticBuffer::setDirtyBit(this->blockNum);

    return SUCCESS;
}

/*
    setSoltMap() => sets the slotmap of the block
*/
int RecBuffer::setSlotMap(unsigned char* slotMap){
    unsigned char *bufferPtr;
    
    // get the starting address of the buffer containing the block
    int res = BlockBuffer::loadBlockAndGetBufferPtr(&bufferPtr);
    if(res != SUCCESS){
        return res;
    }

    /*
        get header, then copy the argument 'slotMap' to the slotmap of the block.
        Slotmap of block can be access by: bufferPtr + HEADER_SIZE;
    */
    struct HeadInfo head;
    this->getHeader(&head);
    int numSlots = head.numSlots;

    memcpy(bufferPtr + HEADER_SIZE, slotMap, numSlots);

    // update the dirty bit.
    res = StaticBuffer::setDirtyBit(this->blockNum);
    return res;

}




// compareAttrs()
int compareAttrs(union Attribute attr1, union Attribute attr2, int attrType){
    double diff;
    if(attrType == STRING){
        diff = strcmp(attr1.sVal, attr2.sVal);
    }
    else{
        diff = attr1.nVal - attr2.nVal;
    }

    if(diff > 0){
        return 1;
    }
    else if(diff < 0){
        return -1;
    }
    else{
        return 0;
    }
}

