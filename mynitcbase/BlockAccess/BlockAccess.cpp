#include "BlockAccess.h"

#include <cstring>


RecId BlockAccess::linearSearch(int relId, char attrName[ATTR_SIZE], union Attribute attrVal, int op){

    // get the previous search index of the relation relId from the relation cache
    RecId prevRecId;
    RelCacheTable::getSearchIndex(relId,&prevRecId);

    // let block and slot denote the record id of the record beign currently checked
    int block, slot;
    // if the current search index record is invalid
    if(prevRecId.block == -1 && prevRecId.slot == -1){
        // it means no hits from the previous search, search should start from the first record itself
        // get the first record block of the relation from the relation cache using getRelCatEntry()
        RelCatEntry relCatEntry;
        RelCacheTable::getRelCatEntry(relId, &relCatEntry);

        // block = first record block of the relation
        // slot = 0
        block = relCatEntry.firstBlk;
        slot = 0;

    }
    else{
        // it means there is a hit from the previous search, should start from the record next to the search index record
        block = prevRecId.block;
        slot = prevRecId.slot + 1;
    }

    /*
        Now the following code will search for the next record in the relation
        that satisfies the given condition
        We will start from the record id (block, slot) and iterate over the
        remaining records of the relation
    */
    while( block != -1){
        // create a RecBuffer object for the block
        RecBuffer recBuffer(block);

        // get the record with id (block,slot) using RecBuffer::getRecord()
        // get header of the block using RecBuffer::getHeader()
        // get slot map of the block using RecBuffer::getSlotMap()
        struct HeadInfo head;
        recBuffer.getHeader(&head);
        Attribute record[head.numAttrs];
        recBuffer.getRecord(record,slot);
        unsigned char slotMap[head.numSlots];
        recBuffer.getSlotMap(slotMap);

        // if slot >= the number of slots per block
        if(slot >= head.numSlots){
            // update block = right block of block
            // update slot = 0
            block = head.rblock;
            slot = 0;
            continue;
        }
        /*
            my notes: if it is end of the record in a relation, this 
            loop will continue to reach the end of the block,
            eventually block = head.rblock = -1 and loop will terminate.
        */

        // if slot is free skip the loop
        // i.e. check if slot'th entry in slot map of block constains SLOT_UNOCCUPIED
        if(slotMap[slot] == SLOT_UNOCCUPIED){
            slot++;
            continue;
        }

        // now compare record's attribute value to the given attrVal as below:
        /*
            firstly get the attribute offset for the attrName attribute
            from the attribute cache entry of the relation using getAttrCatEntry
        */
        /* use the attribute offset to get the value of the attribute from
           current record */
        AttrCatEntry attrCatEntry;
        AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);

        Attribute currentAttrVal = record[attrCatEntry.offset];
        
        // store the difference b/w the attributes.
        int cmpVal = compareAttrs(currentAttrVal, attrVal, attrCatEntry.attrType);

        /* Next task is to check whether this record satisfies the given condition.
           It is determined based on the output of previous comparison and
           the op value received.
           The following code sets the cond variable if the condition is satisfied.
        */
        if(
            (op == NE && cmpVal != 0) ||
            (op == LT && cmpVal < 0) ||
            (op == LE && cmpVal <=0) ||
            (op == EQ && cmpVal == 0) ||
            (op == GT && cmpVal > 0) ||
            (op == GE && cmpVal >= 0)
        ){
            /*
                set the search index in the relation cache as
                the record id of the record that satisfies the given condition
                (use RelCacheTable::setSearchIndex function)
            */
            RecId recid = {block,slot};
            RelCacheTable::setSearchIndex(relId, &recid);

            return recid;
        }

        slot++;
    }

    // no record in the relation with Id relid satisfies the given condition
    return RecId{-1,-1};
}

int BlockAccess::renameRelation(char oldRelName[ATTR_SIZE], char newRelName[ATTR_SIZE]){

    // reset the search index of the relation catalog
    RelCacheTable::resetSearchIndex(RELCAT_RELID);

    Attribute newRelationName; // set newRelationName with newName
    strcpy(newRelationName.sVal, newRelName);

    // search the relation catalog for an entry with "RelName" = newRelationName
    char *relCatAttrRelName;
    strcpy(relCatAttrRelName, RELCAT_ATTR_RELNAME);
    RecId recId = BlockAccess::linearSearch(RELCAT_RELID, relCatAttrRelName, newRelationName, EQ);
    // check if relation already exist
    if(recId.block != -1 && recId.slot != -1){
        return E_RELEXIST;
    }

    // reset the search index of the relation catalog again
    RelCacheTable::resetSearchIndex(RELCAT_RELID);

    Attribute oldRelationName; // set oldRelationName with oldName
    strcpy(oldRelationName.sVal, oldRelName);

    // search the relation catalog for an entry with "RelName" = oldRelationName
    recId = BlockAccess::linearSearch(RELCAT_RELID, relCatAttrRelName, oldRelationName, EQ);
    // check if relation does not exist
    if(recId.block == -1 && recId.slot == -1){
        return E_RELNOTEXIST;
    }

    /*
        get the relation catalog record of the relation to rename using RecBuffer on
        the relation catalog [RELCAT_BLOCK] and RecBuffer.gerRecord function.
    */
    Attribute record[RELCAT_NO_ATTRS];
    RecBuffer recBuffer(RELCAT_BLOCK);
    recBuffer.getRecord(record,recId.slot);

    /*
        update the relation name attribute in the record with newName
        (use RELCAT_REL_NAME_INDEX)
    */
    // then set back the reord value using RecBuffer.setRecord()
    strcpy(record[RELCAT_REL_NAME_INDEX].sVal, newRelName);
    recBuffer.setRecord(record, recId.slot);

    /*
        update all the attribute catalog entries in the attribute catalog corresponding
        to the relation with relation name oldName to the relation name newName.
    */

    //  reset the search index of the attribute catalog
    RelCacheTable::resetSearchIndex(ATTRCAT_RELID);
            // Relation 'Student' can have 10 attributes, thus Attribute Catalog will have 10 entries correspoding to 'Student'
    int numOfAttrs = record[RELCAT_NO_ATTRIBUTES_INDEX].nVal;
    char *attrCatAttrRelName;
    strcpy(attrCatAttrRelName, ATTRCAT_ATTR_RELNAME);
    for(int i = 0; i < numOfAttrs; i++){
        RecId rec_id = BlockAccess::linearSearch(ATTRCAT_RELID, attrCatAttrRelName, oldRelationName, EQ);
        Attribute attrRecord[ATTRCAT_NO_ATTRS];
        RecBuffer attrBuffer(rec_id.block);
        attrBuffer.getRecord(attrRecord, rec_id.slot);

        attrRecord[ATTRCAT_REL_NAME_INDEX] = newRelationName;
        attrBuffer.setRecord(attrRecord, rec_id.slot);
    }

    return SUCCESS;
    
}


int BlockAccess::renameAttribute(char relName[ATTR_SIZE], char oldName[ATTR_SIZE], char newName[ATTR_SIZE]){

    // 1.   first of all we should check whether the given relation exist or not

    /* reset the searchIndex of the relation catalog*/
    RelCacheTable::resetSearchIndex(RELCAT_RELID);

    Attribute relNameAttr; // set relNameAtrr to relName
    strcpy(relNameAttr.sVal, relName);
    
    // Search for the relation with name relName in the Relation Catalog using linearSearch()
    char *relCatAttrRelName;
    strcpy(relCatAttrRelName, RELCAT_ATTR_RELNAME);
    RecId recId = BlockAccess::linearSearch(RELCAT_RELID,relCatAttrRelName,relNameAttr, EQ);

    // check if it doesn't exist
    if(recId.block == -1 && recId.slot == -1){
        return E_RELNOTEXIST;
    }

    /* reset the searchIndex of the attribute catalog */
    RelCacheTable::resetSearchIndex(ATTRCAT_RELID);
    RecId attrToRenameRecId;
    attrToRenameRecId.block = -1;
    attrToRenameRecId.slot = -1;
    Attribute attrCatEntryRecord[ATTRCAT_NO_ATTRS];

    /* 
        1   Iterating over all the entries in the attribute catalog to find relation with relname = relName (paramter)
        2.  If found then the check if attrname == oldAttrname and take action accordingly.
    */
    char *attrCatAttrRelName;
    strcpy(attrCatAttrRelName, ATTRCAT_ATTR_RELNAME);
    while(true){
        // linearSearch
        RecId rec_id = BlockAccess::linearSearch(ATTRCAT_RELID, attrCatAttrRelName, relNameAttr, EQ);

        // if there is no more attribute left, then break
        if(rec_id.block == -1 && rec_id.slot == -1){
            break;
        }

        /* Get the record from the attribute catalog using RecBuffer.getRecord() */
        RecBuffer attrBuffer(rec_id.block);
        attrBuffer.getRecord(attrCatEntryRecord, rec_id.slot);

        // if attrCatEntryRecord's attrName == oldName
        //      set attrToRenameRecId with rec_id
        if(strcmp(attrCatEntryRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, oldName) == 0){
            attrToRenameRecId.block = rec_id.block;
            attrToRenameRecId.slot = rec_id.slot;
        }

        // if attrCatEntryRecord.attrName = newName
        //      return E_ATTREXIST
        if(strcmp(attrCatEntryRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, newName) == 0){
            return E_ATTREXIST;
        }
    }

    // if attrToRenameRecId = {-1,-1} => E_ATTRNOTEXIST
    if(attrToRenameRecId.block == -1 && attrToRenameRecId.slot == -1){
        return E_ATTRNOTEXIST;
    }

    // Update the entry corresponding to the attribute in the Attribute Catalog Relation.
    RecBuffer attrToRenameBuff(attrToRenameRecId.block);
    attrToRenameBuff.getRecord(attrCatEntryRecord, attrToRenameRecId.slot);
    strcpy(attrCatEntryRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, newName);
    // set back the record with RecBuffer.setRecord
    attrToRenameBuff.setRecord(attrCatEntryRecord, attrToRenameRecId.slot);

    return SUCCESS;
}

/*
    insert(): function to insert a record into a relation
*/
int BlockAccess::insert(int relId, Attribute *record){
    // get the relation catalog entry from the relation cache table
    RelCatEntry relCatEntry;
    RelCacheTable::getRelCatEntry(relId, &relCatEntry);

    int blockNum = relCatEntry.firstBlk; // first record block of the relation.

    // rec_id will be used to store where the new record will be inserted.
    RecId rec_id = {-1, -1};

    int numOfSlots = relCatEntry.numSlotsPerBlk;    // number of slots per record block
    int numOfAttributes = relCatEntry.numAttrs;     // number of attributes of the relation
    int prevBlockNum = -1;                          // block number of the last element in the linked list

    /*
        Traversing the linked list of existing record block of the relation
        until a free slot is found or until the end of the list is reached.
    */
    while(blockNum != -1){
        //get the slotmap of the block and search for a free slot.
        RecBuffer recBuffer(blockNum);
        struct HeadInfo head;
        recBuffer.getHeader(&head);
        unsigned char slotMap[head.numSlots];
        recBuffer.getSlotMap(slotMap);

        // iterate over slotmap and store the free in the rec_id
        for(int i = 0; i < head.numSlots; i++){
            if(slotMap[i] == SLOT_UNOCCUPIED){
                rec_id.block = blockNum;
                rec_id.slot = i;
                break;
            }
        }

        // if slot is found, then break the traversal.
        if(rec_id.block != -1 && rec_id.slot != -1){
            break;
        }

        // otherwise, continue to check the next block by updating the block number
        prevBlockNum = blockNum;
        blockNum = head.rblock;
    }

    // if slot not found
    if(rec_id.block == -1 && rec_id.slot == -1){
        // if relation is RELCAT, do not allocate any more blocks
        if(relId == RELCAT_RELID){
            return E_MAXRELATIONS; // DOUBTFULL
        }

        // else, get a new record block
        // get the block number of newly allocated block
        RecBuffer newRecBuffer;
        int ret = newRecBuffer.getBlockNum();

        if(ret == E_DISKFULL){
            return E_DISKFULL;
        }

        // Assign rec_id.block = new block number (i.e. ret) and rec_id.slot = 0
        rec_id.block = ret;
        rec_id.slot = 0;

        /*
            set the header of the new record block such that it links with the existing record
            blocks of the relation
         */
        struct HeadInfo newHead;
        newHead.blockType = REC;
        newHead.pblock = -1;
        newHead.lblock = prevBlockNum;
        newHead.rblock = -1;
        newHead.numEntries = 0;
        newHead.numSlots = numOfSlots;
        newHead.numAttrs = numOfAttributes;

        newRecBuffer.setHeader(&newHead);

        /*
            set block's slot map with all slots marked as free
        */
        unsigned char newSlotMap[numOfSlots];
        for(int i = 0; i < numOfSlots; i++){
            newSlotMap[i] = SLOT_UNOCCUPIED;
        }
        newRecBuffer.setSlotMap(newSlotMap);

        // now if there was a previous block, then it's rblock should be set as rec_id.block
        if(prevBlockNum != -1){
            // create a RecBuffer object for prevBlockNum
            // get the header and update the rblock field.
            // then set the header using setHeader()
            RecBuffer prevRecBuffer(prevBlockNum);
            struct HeadInfo prevHead;
            prevRecBuffer.getHeader(&prevHead);
            prevHead.rblock = rec_id.block;
            prevRecBuffer.setHeader(&prevHead);
        }
        else{
            // means it is the 1st block of the relation
            // update first block field in the relCatEntry to the new block
            RelCatEntry relCatEntry;
            RelCacheTable::getRelCatEntry(relId, &relCatEntry);
            relCatEntry.firstBlk = rec_id.block;
            RelCacheTable::setRelCatEntry(relId, &relCatEntry);
        }

        // update the lastBlk field of the relCatEntry to the new block
        RelCatEntry relCatEntry;
        RelCacheTable::getRelCatEntry(relId, &relCatEntry);
        relCatEntry.lastBlk = rec_id.block;
        RelCacheTable::setRelCatEntry(relId, &relCatEntry); // is it neccessary because we are setting it up again at the end of this function.
    }

    // Now create a RecBuffer object for rec_id.block
    // and insert the record into the rec_id.slot using the setRecord()
    RecBuffer recBuffer(rec_id.block); // i am using the old recBuffer I created initially.
    recBuffer.setRecord(record, rec_id.slot);

    /*
        update the slotmap of the block by making the entry of the slot to which
        record was inserted as occupied
        Get the slotMap using getSlotMap() ==> update the rec_id.slot-th index as OCCUPIED ==> set the slotMap using setSlotMap()
    */
    struct HeadInfo head;
    recBuffer.getHeader(&head);
    unsigned char slotMap[head.numSlots];
    recBuffer.getSlotMap(slotMap);
    slotMap[rec_id.slot] = SLOT_OCCUPIED;
    recBuffer.setSlotMap(slotMap);

    /*
        increment the numEntries field in the header of the block to
        which record was inserted. Then set the header back using BlockBuffer::setHeader()
    */
    head.numEntries += 1;
    recBuffer.setHeader(&head);

    /*
        Increment the number of records field in the relation cache entry for the relation.
    */
    RelCacheTable::getRelCatEntry(relId, &relCatEntry);
    relCatEntry.numRecs += 1;
    RelCacheTable::setRelCatEntry(relId, &relCatEntry);

    return SUCCESS;
    
}


/*
    NOTE: This function will copy the result of the search to the 'record' argument.
    The caller should ensure that space is allocated for 'record' array based on the
    number of attributes in the relation.
*/
int BlockAccess::search(int relId, Attribute *record, char attrName[ATTR_SIZE], Attribute attrVal, int op){
    // Declare a variable called recId to store the searched record
    RecId recId;

    /*
        Search for the record id (recId) corresponding to the attribute with attribute
        name attrName, with value attrVal and satisfying the condition op
        using linearSearch()
    */
    recId = BlockAccess::linearSearch(relId, attrName, attrVal, op);

    // if record not found return E_NOTFOUND
    if(recId.block == -1 && recId.slot == -1){
        return E_NOTFOUND;
    }

    /*
        Copy the record with recId to the record buffer (record)
    */
    RecBuffer recBuffer(recId.block);
    recBuffer.getRecord(record,recId.slot); //content of that record will be stored in argument record

    return SUCCESS;
}

/*
    deleteRelation()
*/
int BlockAccess::deleteRelation(char relName[ATTR_SIZE]){
    // if the relation to delete is either Relation Catalog or Attribute Catalog, return E_NOTPERMITTED
    if(strcmp(relName, RELCAT_RELNAME) == 0 || strcmp(relName, ATTRCAT_RELNAME) == 0){
        return E_NOTPERMITTED;
    }

    // reset the searchIndex of the relation catalog
    RelCacheTable::resetSearchIndex(RELCAT_RELID);

    Attribute relNameAttr;
    strcpy(relNameAttr.sVal, relName);

    char relCatAttrRelname[]=RELCAT_ATTR_RELNAME;

    // linearSearch on the relation catalog for RelName = relName
    RecId recId = BlockAccess::linearSearch(RELCAT_RELID, relCatAttrRelname, relNameAttr,EQ);
    if(recId.block == -1 && recId.slot == -1){
        return E_RELNOTEXIST;
    }

    /* Store the relation catalog */
    Attribute relCatEntryRecord[RELCAT_NO_ATTRS];
    RecBuffer recBuffer(recId.block);
    recBuffer.getRecord(relCatEntryRecord,recId.slot);

    /* get the first record block and number of attributes of the relation */
    int firstBlock = relCatEntryRecord[RELCAT_FIRST_BLOCK_INDEX].nVal;  
    int numAttrs = relCatEntryRecord[RELCAT_NO_ATTRIBUTES_INDEX].nVal;

    /***  Delete all the record blocks of the relation ***/
    int currBlock = firstBlock;
    while(currBlock != -1){
        // get the block header
        // get the next block from header
        // release the current block

        struct HeadInfo head;
        RecBuffer currRecBuffer(currBlock);
        currRecBuffer.getHeader(&head);
        currBlock = head.rblock;
        currRecBuffer.releaseBlock();
    }

    /*** 
        Deleting attributes catalog entries corresponding the relation and
        index block corresponding to the relation with relName on its attributes
    ***/
    // reset searchIndex of the attribute catalog
    RelCacheTable::resetSearchIndex(ATTRCAT_RELID);

    int numberOfAttributesDeleted = 0;

    // NOTE: ATTRCAT_ATTR_RELNAME = RELCAT_ATTR_RELNAME = Relname
    
    while(true){
        RecId attrCatRecId = BlockAccess::linearSearch(ATTRCAT_RELID, relCatAttrRelname, relNameAttr, EQ);
        if(attrCatRecId.block == -1 && attrCatRecId.slot == -1){
            break;
        }

        numberOfAttributesDeleted++;

        // create a RecBuffer for the attrCatRecId.block and get header and record at attrCatRecId.slot
        RecBuffer recBuffer(attrCatRecId.block);
        struct HeadInfo head;
        recBuffer.getHeader(&head);
        Attribute record[head.numAttrs];
        recBuffer.getRecord(record, attrCatRecId.slot);
        
        // declare the rootBlock which will be used to store the root block field from the attribute catalog record.
        // int rootBlock = /* get root block from the record */

        // update the SlotMap for the block by setting the slot as SLOT_UNOCCUPIED
        unsigned char slotMap[head.numSlots];
        recBuffer.getSlotMap(slotMap);
        slotMap[attrCatRecId.slot] = SLOT_UNOCCUPIED;
        recBuffer.setSlotMap(slotMap);

        /*
            Decrement the numEntries in the header of the block corresponding to
            the attribute catalog entry and then set back the header.
        */
        head.numEntries--;
        recBuffer.setHeader(&head);

        /*
            If number of entries become 0, releaseBlock is called after fixing
            the linkedList
        */
        if(head.numEntries == 0){
            /*
                Standard Linked List Delete for a block:
                    Get header of left block and sets its rblock to rblock of
                    current block
            */

            RecBuffer leftRecBuffer(head.lblock);
            struct HeadInfo leftHead;
            leftRecBuffer.getHeader(&leftHead);
            leftHead.rblock = head.rblock;
            leftRecBuffer.setHeader(&leftHead);

            if(head.rblock != -1){
                /*
                    Get the header of the right blocl and set it's lblock to
                    this block's lbock
                */
                RecBuffer rightRecBuffer(head.rblock);
                struct HeadInfo rightHead;
                rightRecBuffer.getHeader(&rightHead);
                rightHead.lblock = head.lblock;
                rightRecBuffer.setHeader(&rightHead);
            }
            else{
                // so this is the last block being released of the relation
                /*
                    Update the Relation Catalog entry'a LastBlock field for this
                    relation (Attribute catalog) with the block number of the previous block
                 */
                 RelCatEntry relCatEntry;
                 RelCacheTable::getRelCatEntry(ATTRCAT_RELID, &relCatEntry);
                 relCatEntry.lastBlk = head.lblock;
                 RelCacheTable::setRelCatEntry(ATTRCAT_RELID, &relCatEntry);
            }
            
            // call releaseBlock()
            recBuffer.releaseBlock();
        }

        // next part is for later stages.
          
    }

    /*** Delete the entry corresponding to the relation from relation catalog ***/

    // Fetch header of the relcat block
    struct HeadInfo head;
    recBuffer.getHeader(&head);

    // decrement the numEntries in the header and set it back
    head.numEntries--;
    recBuffer.setHeader(&head);

    // get the slotmap of relation catalog and mark the corresponding slot unoccupied
    unsigned char slotMap[head.numSlots];
    recBuffer.getSlotMap(slotMap);
    slotMap[recId.slot] = SLOT_UNOCCUPIED;
    recBuffer.setSlotMap(slotMap);

    /*** Updating the Relation Cache Table ***/

    // update relation catalog entry [number of record decreased by 1]
    RelCatEntry relCatEntry;
    RelCacheTable::getRelCatEntry(RELCAT_RELID, &relCatEntry);
    relCatEntry.numRecs--;
    RelCacheTable::setRelCatEntry(RELCAT_RELID, &relCatEntry);

    // update attribute catalog entry [num of records decreased by numberOfDeletedAttr]
    RelCatEntry attrCatEntry;
    RelCacheTable::getRelCatEntry(ATTRCAT_RELID, &attrCatEntry);
    attrCatEntry.numRecs -= numberOfAttributesDeleted;
    RelCacheTable::setRelCatEntry(ATTRCAT_RELID, &attrCatEntry);

    return SUCCESS;
}

/*
    Project(): is used to fetch one record of the relation. Each subsequent call would retuen the next record
    until there are no more records to be returned. It also updates the searchIndex in the cache.
*/
int BlockAccess::project(int relId, Attribute *record){
    // get the prevouse search index of the relation with relId
    RecId prevRecId;
    RelCacheTable::getSearchIndex(relId, &prevRecId);

    int block, slot;

    /*
        If the current search index record is invalid (i.e. {-1,-1})
        it means the caller has reset the searchIndex.
    */
    if(prevRecId.block == -1 && prevRecId.slot == -1){
        // the new projection operation [ from the beginning]
        
        // get the first record block of the relation from the relation cache
        RelCatEntry relCatEntry;
        int ret = RelCacheTable::getRelCatEntry(relId, &relCatEntry);

        // error handling
        if(ret != SUCCESS){
            return ret;
        }

        block = relCatEntry.firstBlk;
        slot = 0;
    }
    else{
        // means the projection has already in progress.
        block = prevRecId.block;
        slot = prevRecId.slot + 1;
    }

    // now find the next record of the relation
    /*
        start from record id (block, slot) and iterate over the remaining records of the
        relation.
    */
    while(block != -1){
        // create a RecBuffer for the current block and get the header and slotMap
        RecBuffer recBuffer(block);

        struct HeadInfo head;
        recBuffer.getHeader(&head);

        unsigned char slotMap[head.numSlots];
        recBuffer.getSlotMap(slotMap);

        // if slot >= the number of slots in the block
        if(slot >= head.numSlots){
            // it means no more slot in the block
            // change the record Id to next block
            block = head.rblock;
            slot = 0;
        }

        // if slot is free.
        else if(slotMap[slot] == SLOT_UNOCCUPIED){
            slot++;
        }
        else{
            break; // next occupied slot and record has been found.
        }
    }

    if( block == -1){
        // record not found, all records exhausted.
        return E_NOTFOUND;
    }

    // store the record Id found
    RecId nextRecId{block, slot};

    // set the searchIndex to nextRecId
    RelCacheTable::setSearchIndex(relId, &nextRecId);

    RecBuffer recBuffer(nextRecId.block);
    recBuffer.getRecord(record, nextRecId.slot);

    return SUCCESS;

}
