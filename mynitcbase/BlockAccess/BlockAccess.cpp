#include "BlockAccess.h"

#include <cstring>

RecId BlockAccess::linearSearch(int relId, char attrName[ATTR_SIZE], union Attribute attrVal, int op) {
    // get the previous search index of the relation relId from the relation cache
    // (use RelCacheTable::getSearchIndex() function)
    RecId prevRecId;
    RelCacheTable::getSearchIndex(relId, &prevRecId);

    // let block and slot denote the record id of the record being currently checked
    int block, slot;

    // if the current search index record is invalid(i.e. both block and slot = -1)
    if (prevRecId.block == -1 && prevRecId.slot == -1) {
        // (no hits from previous search; search should start from the
        // first record itself)

        // get the first record block of the relation from the relation cache
        // (use RelCacheTable::getRelCatEntry() function of Cache Layer)
        RelCatEntry relCatBuf;
        RelCacheTable::getRelCatEntry(relId, &relCatBuf);

        // block = first record block of the relation
        // slot = 0
        block = relCatBuf.firstBlk;
        slot = 0;
    }
    else {
        // (there is a hit from previous search; search should start from
        // the record next to the search index record)

        // block = search index's block
        // slot = search index's slot + 1
        block = prevRecId.block;
        slot = prevRecId.slot + 1;
    }

    /* The following code searches for the next record in the relation
       that satisfies the given condition
       We start from the record id (block, slot) and iterate over the remaining
       records of the relation
    */
    while (block != -1) {
        /* create a RecBuffer object for block (use RecBuffer Constructor for
           existing block) */
        RecBuffer recBuffer(block);

        // get header of the block using RecBuffer::getHeader() function
        HeadInfo head;
        recBuffer.getHeader(&head);

        // get slot map of the block using RecBuffer::getSlotMap() function
        unsigned char slotMap[head.numSlots];
        recBuffer.getSlotMap(slotMap);

        // If slot >= the number of slots per block(i.e. no more slots in this block)
        if (slot >= head.numSlots) {
            // update block = right block of block
            // update slot = 0
            block = head.rblock;
            slot = 0;
            continue;  // continue to the beginning of this while loop
        }

        // if slot is free skip the loop
        // (i.e. check if slot'th entry in slot map of block contains SLOT_UNOCCUPIED)
        if (slotMap[slot] == SLOT_UNOCCUPIED) {
            // increment slot and continue to the next record slot
            slot++;
            continue;
        }

        // get the record with id (block, slot) using RecBuffer::getRecord()
        Attribute record[head.numAttrs];
        recBuffer.getRecord(record, slot);

        // compare record's attribute value to the the given attrVal as below:
        /*
            firstly get the attribute offset for the attrName attribute
            from the attribute cache entry of the relation using
            AttrCacheTable::getAttrCatEntry()
        */
        AttrCatEntry attrCatBuf;
        AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatBuf);

        /* use the attribute offset to get the value of the attribute from
           current record */
        int cmpVal;  // will store the difference between the attributes
        
        // set cmpVal using compareAttrs()
        cmpVal = compareAttrs(record[attrCatBuf.offset], attrVal, attrCatBuf.attrType);

        /* Next task is to check whether this record satisfies the given condition.
           It is determined based on the output of previous comparison and
           the op value received.
           The following code sets the cond variable if the condition is satisfied.
        */
        if (
            (op == NE && cmpVal != 0) ||    // if op is "not equal to"
            (op == LT && cmpVal < 0) ||     // if op is "less than"
            (op == LE && cmpVal <= 0) ||    // if op is "less than or equal to"
            (op == EQ && cmpVal == 0) ||    // if op is "equal to"
            (op == GT && cmpVal > 0) ||     // if op is "greater than"
            (op == GE && cmpVal >= 0)       // if op is "greater than or equal to"
        ) {
            /*
            set the search index in the relation cache as
            the record id of the record that satisfies the given condition
            (use RelCacheTable::setSearchIndex function)
            */
            RecId newRecId = {block, slot};
            RelCacheTable::setSearchIndex(relId, &newRecId);

            return newRecId;
        }

        slot++;
    }

    // no record in the relation with Id relid satisfies the given condition
    return RecId{-1, -1};
}

int BlockAccess::renameRelation(char oldName[ATTR_SIZE], char newName[ATTR_SIZE]) {
  // reset the searchIndex of the relation catalog
  RelCacheTable::resetSearchIndex(RELCAT_RELID);

  Attribute newRelationName;
  strcpy(newRelationName.sVal, newName);

  // search the relation catalog for an entry with "RelName" = newRelationName
  RecId newRelRecId = BlockAccess::linearSearch(RELCAT_RELID, (char*)"RelName", newRelationName, EQ);

  // If relation with name newName already exists
  if (newRelRecId.block != -1 && newRelRecId.slot != -1) {
    return E_RELEXIST;
  }

  // reset the searchIndex of the relation catalog
  RelCacheTable::resetSearchIndex(RELCAT_RELID);

  Attribute oldRelationName;
  strcpy(oldRelationName.sVal, oldName);

  // search the relation catalog for an entry with "RelName" = oldRelationName
  RecId oldRelRecId = BlockAccess::linearSearch(RELCAT_RELID, (char*)"RelName", oldRelationName, EQ);

  // If relation with name oldName does not exist
  if (oldRelRecId.block == -1 && oldRelRecId.slot == -1) {
    return E_RELNOTEXIST;
  }

  /* get the relation catalog record of the relation to rename */
  RecBuffer relBuffer(oldRelRecId.block);
  Attribute relRecord[RELCAT_NO_ATTRS];
  relBuffer.getRecord(relRecord, oldRelRecId.slot);

  /* update the relation name attribute in the record with newName */
  strcpy(relRecord[RELCAT_REL_NAME_INDEX].sVal, newName);
  
  // set back the record value
  relBuffer.setRecord(relRecord, oldRelRecId.slot);

  /* update all the attribute catalog entries */
  int numAttrs = relRecord[RELCAT_NO_ATTRIBUTES_INDEX].nVal;

  // reset the searchIndex of the attribute catalog
  RelCacheTable::resetSearchIndex(ATTRCAT_RELID);

  for (int i = 0; i < numAttrs; i++) {
    // linearSearch on the attribute catalog for relName = oldRelationName
    RecId attrRecId = BlockAccess::linearSearch(ATTRCAT_RELID, (char*)"RelName", oldRelationName, EQ);

    // get the record
    RecBuffer attrBuffer(attrRecId.block);
    Attribute attrRecord[ATTRCAT_NO_ATTRS];
    attrBuffer.getRecord(attrRecord, attrRecId.slot);

    // update the relName field in the record to newName
    strcpy(attrRecord[ATTRCAT_REL_NAME_INDEX].sVal, newName);
    
    // set back the record using RecBuffer.setRecord
    attrBuffer.setRecord(attrRecord, attrRecId.slot);
  }

  return SUCCESS;
}

int BlockAccess::renameAttribute(char relName[ATTR_SIZE], char oldName[ATTR_SIZE], char newName[ATTR_SIZE]) {

    /* reset the searchIndex of the relation catalog using
       RelCacheTable::resetSearchIndex() */
    RelCacheTable::resetSearchIndex(RELCAT_RELID);

    Attribute relNameAttr;    // set relNameAttr to relName
    strcpy(relNameAttr.sVal, relName);

    // Search for the relation with name relName in relation catalog using linearSearch()
    RecId relRecId = BlockAccess::linearSearch(RELCAT_RELID, (char*)"RelName", relNameAttr, EQ);
    
    // If relation with name relName does not exist (search returns {-1,-1})
    if (relRecId.block == -1 && relRecId.slot == -1) {
        return E_RELNOTEXIST;
    }

    /* reset the searchIndex of the attribute catalog using
       RelCacheTable::resetSearchIndex() */
    RelCacheTable::resetSearchIndex(ATTRCAT_RELID);

    /* declare variable attrToRenameRecId used to store the attr-cat recId
    of the attribute to rename */
    RecId attrToRenameRecId{-1, -1};
    Attribute attrCatEntryRecord[ATTRCAT_NO_ATTRS];

    /* iterate over all Attribute Catalog Entry record corresponding to the
       relation to find the required attribute */
    while (true) {
        // linear search on the attribute catalog for RelName = relNameAttr
        RecId searchRecId = BlockAccess::linearSearch(ATTRCAT_RELID, (char*)"RelName", relNameAttr, EQ);

        // if there are no more attributes left to check (linearSearch returned {-1,-1})
        if (searchRecId.block == -1 && searchRecId.slot == -1) {
            break;
        }

        /* Get the record from the attribute catalog using RecBuffer.getRecord
          into attrCatEntryRecord */
        RecBuffer attrBuffer(searchRecId.block);
        attrBuffer.getRecord(attrCatEntryRecord, searchRecId.slot);

        // if attrCatEntryRecord.attrName = oldName
        //     attrToRenameRecId = block and slot of this record
        if (strcmp(attrCatEntryRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, oldName) == 0) {
            attrToRenameRecId = searchRecId;
        }

        // if attrCatEntryRecord.attrName = newName
        //     return E_ATTREXIST;
        if (strcmp(attrCatEntryRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, newName) == 0) {
            return E_ATTREXIST;
        }
    }

    // if attrToRenameRecId == {-1, -1}
    //     return E_ATTRNOTEXIST;
    if (attrToRenameRecId.block == -1 && attrToRenameRecId.slot == -1) {
        return E_ATTRNOTEXIST;
    }

    // Update the entry corresponding to the attribute in the Attribute Catalog Relation.
    /*   declare a RecBuffer for attrToRenameRecId.block and get the record at
         attrToRenameRecId.slot */
    RecBuffer updateBuffer(attrToRenameRecId.block);
    Attribute updateRecord[ATTRCAT_NO_ATTRS];
    updateBuffer.getRecord(updateRecord, attrToRenameRecId.slot);
    
    //   update the AttrName of the record with newName
    strcpy(updateRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, newName);
    
    //   set back the record with RecBuffer.setRecord
    updateBuffer.setRecord(updateRecord, attrToRenameRecId.slot);

    return SUCCESS;
}

int BlockAccess::insert(int relId, Attribute *record) {
    // get the relation catalog entry from relation cache
    RelCatEntry relCatEntry;
    RelCacheTable::getRelCatEntry(relId, &relCatEntry);

    int blockNum = relCatEntry.firstBlk;

    // rec_id will be used to store where the new record will be inserted
    RecId rec_id = {-1, -1};

    int numOfSlots = relCatEntry.numSlotsPerBlk;
    int numOfAttributes = relCatEntry.numAttrs;

    int prevBlockNum = -1;

    /*
        Traversing the linked list of existing record blocks of the relation
        until a free slot is found OR
        until the end of the list is reached
    */
    while (blockNum != -1) {
        // create a RecBuffer object for blockNum 
        RecBuffer blockBuffer(blockNum);

        // get header of block(blockNum) 
        HeadInfo header;
        blockBuffer.getHeader(&header);

        // get slot map of block(blockNum) 
        unsigned char slotMap[numOfSlots];
        blockBuffer.getSlotMap(slotMap);

        // search for free slot in the block 'blockNum' and store it's rec-id in rec_id
        for (int slot = 0; slot < numOfSlots; slot++) {
            if (slotMap[slot] == SLOT_UNOCCUPIED) {
                rec_id.block = blockNum;
                rec_id.slot = slot;
                break;
            }
        }

        /* if a free slot is found, set rec_id and discontinue the traversal */
        if (rec_id.block != -1 && rec_id.slot != -1) {
            break;
        }

        /* otherwise, continue to check the next block */
        prevBlockNum = blockNum;
        blockNum = header.rblock;
    }

    //  if no free slot is found in existing record blocks (rec_id = {-1, -1})
    if (rec_id.block == -1 && rec_id.slot == -1) {
        // if relation is RELCAT, do not allocate any more blocks
        if (relId == RELCAT_RELID) {
            return E_MAXRELATIONS;
        }

        // Otherwise, get a new record block 
        RecBuffer newBlockBuffer;
        
        // get the block number of the newly allocated block
        int ret = newBlockBuffer.getBlockNum();
        if (ret == E_DISKFULL) {
            return E_DISKFULL;
        }

        // Assign rec_id.block = new block number(i.e. ret) and rec_id.slot = 0
        rec_id.block = ret;
        rec_id.slot = 0;

        /* set the header of the new record block */
        HeadInfo header;
        header.blockType = REC;
        header.pblock = -1;
        header.lblock = prevBlockNum;
        header.rblock = -1;
        header.numEntries = 0;
        header.numSlots = numOfSlots;
        header.numAttrs = numOfAttributes;
        newBlockBuffer.setHeader(&header);

        /* set block's slot map with all slots marked as free */
        unsigned char slotMap[numOfSlots];
        for (int i = 0; i < numOfSlots; i++) {
            slotMap[i] = SLOT_UNOCCUPIED;
        }
        newBlockBuffer.setSlotMap(slotMap);

        if (prevBlockNum != -1) {
            // create a RecBuffer object for prevBlockNum
            RecBuffer prevBlockBuffer(prevBlockNum);
            
            // get the header of the block prevBlockNum
            HeadInfo prevHeader;
            prevBlockBuffer.getHeader(&prevHeader);
            
            // update the rblock field of the header to the new block number
            prevHeader.rblock = ret;
            prevBlockBuffer.setHeader(&prevHeader);
        } else {
            // update first block field in the relation catalog entry to the new block 
            relCatEntry.firstBlk = ret;
        }

        // update last block field in the relation catalog entry to the new block 
        relCatEntry.lastBlk = ret;
        RelCacheTable::setRelCatEntry(relId, &relCatEntry);
    }

    // create a RecBuffer object for rec_id.block
    RecBuffer insertBuffer(rec_id.block);
    
    // insert the record into rec_id'th slot 
    insertBuffer.setRecord(record, rec_id.slot);

    /* update the slot map of the block */
    unsigned char slotMap[numOfSlots];
    insertBuffer.getSlotMap(slotMap);
    slotMap[rec_id.slot] = SLOT_OCCUPIED;
    insertBuffer.setSlotMap(slotMap);

    // increment the numEntries field in the header of the block 
    HeadInfo insertHeader;
    insertBuffer.getHeader(&insertHeader);
    insertHeader.numEntries++;
    insertBuffer.setHeader(&insertHeader);

    // Increment the number of records field in the relation cache entry 
    RelCacheTable::getRelCatEntry(relId, &relCatEntry);
    relCatEntry.numRecs++;
    RelCacheTable::setRelCatEntry(relId, &relCatEntry);

    return SUCCESS;
}

int BlockAccess::search(int relId, Attribute *record, char attrName[ATTR_SIZE], Attribute attrVal, int op) {
    // Declare a variable called recid to store the searched record
    RecId recId;

    /* search for the record id (recid) corresponding to the attribute with
    attribute name attrName, with value attrval and satisfying the condition op
    using linearSearch() */
    recId = BlockAccess::linearSearch(relId, attrName, attrVal, op);

    // if there's no record satisfying the given condition (recId = {-1, -1})
    if (recId.block == -1 && recId.slot == -1) {
        return E_NOTFOUND;
    }

    /* Copy the record with record id (recId) to the record buffer (record)
       For this Instantiate a RecBuffer class object using recId and
       call the appropriate method to fetch the record
    */
    RecBuffer searchBuffer(recId.block);
    searchBuffer.getRecord(record, recId.slot);

    return SUCCESS;
}

int BlockAccess::deleteRelation(char relName[ATTR_SIZE]) {
    // if the relation to delete is either Relation Catalog or Attribute Catalog,
    //     return E_NOTPERMITTED
    if (strcmp(relName, RELCAT_RELNAME) == 0 || strcmp(relName, ATTRCAT_RELNAME) == 0) {
        return E_NOTPERMITTED;
    }

    /* reset the searchIndex of the relation catalog using
       RelCacheTable::resetSearchIndex() */
    RelCacheTable::resetSearchIndex(RELCAT_RELID);

    Attribute relNameAttr; // (stores relName as type union Attribute)
    // assign relNameAttr.sVal = relName
    strcpy(relNameAttr.sVal, relName);

    //  linearSearch on the relation catalog for RelName = relNameAttr
    RecId relCatRecId = BlockAccess::linearSearch(RELCAT_RELID, (char*)"RelName", relNameAttr, EQ);

    // if the relation does not exist (linearSearch returned {-1, -1})
    if (relCatRecId.block == -1 && relCatRecId.slot == -1) {
        return E_RELNOTEXIST;
    }

    Attribute relCatEntryRecord[RELCAT_NO_ATTRS];
    /* store the relation catalog record corresponding to the relation in
       relCatEntryRecord using RecBuffer.getRecord */
    RecBuffer relCatBlock(relCatRecId.block);
    relCatBlock.getRecord(relCatEntryRecord, relCatRecId.slot);

    /* get the first record block of the relation (firstBlock) using the
       relation catalog entry record */
    int firstBlock = relCatEntryRecord[RELCAT_FIRST_BLOCK_INDEX].nVal;
    
    /* get the number of attributes corresponding to the relation (numAttrs)
       using the relation catalog entry record */
    int numAttrs = relCatEntryRecord[RELCAT_NO_ATTRIBUTES_INDEX].nVal;

    /*
     Delete all the record blocks of the relation
    */
    int currentBlock = firstBlock;
    while (currentBlock != -1) {
        RecBuffer blockBuffer(currentBlock);
        HeadInfo header;
        blockBuffer.getHeader(&header);
        
        int nextBlock = header.rblock;
        blockBuffer.releaseBlock();
        
        currentBlock = nextBlock;
    }


    /***
        Deleting attribute catalog entries corresponding the relation and index
        blocks corresponding to the relation with relName on its attributes
    ***/

    // reset the searchIndex of the attribute catalog
    RelCacheTable::resetSearchIndex(ATTRCAT_RELID);

    int numberOfAttributesDeleted = 0;

    while(true) {
        // attrCatRecId = linearSearch on attribute catalog for RelName = relNameAttr
        RecId attrCatRecId = BlockAccess::linearSearch(ATTRCAT_RELID, (char*)"RelName", relNameAttr, EQ);

        // if no more attributes to iterate over (attrCatRecId == {-1, -1})
        if (attrCatRecId.block == -1 && attrCatRecId.slot == -1) {
            break;
        }

        numberOfAttributesDeleted++;

        // create a RecBuffer for attrCatRecId.block
        RecBuffer attrCatBlock(attrCatRecId.block);
        
        // get the header of the block
        HeadInfo header;
        attrCatBlock.getHeader(&header);
        
        // get the record corresponding to attrCatRecId.slot
        Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
        attrCatBlock.getRecord(attrCatRecord, attrCatRecId.slot);

        // declare variable rootBlock which will be used to store the root
        // block field from the attribute catalog record.
        int rootBlock = attrCatRecord[ATTRCAT_ROOT_BLOCK_INDEX].nVal;

        // Update the Slotmap for the block by setting the slot as SLOT_UNOCCUPIED
        unsigned char slotMap[header.numSlots];
        attrCatBlock.getSlotMap(slotMap);
        slotMap[attrCatRecId.slot] = SLOT_UNOCCUPIED;
        attrCatBlock.setSlotMap(slotMap);

        /* Decrement the numEntries in the header of the block corresponding to
           the attribute catalog entry and then set back the header */
        header.numEntries--;
        attrCatBlock.setHeader(&header);

        /* If number of entries become 0, releaseBlock is called after fixing
           the linked list.
        */
        if (header.numEntries == 0) {
            /* Standard Linked List Delete for a Block
               Get the header of the left block and set it's rblock to this
               block's rblock
            */
            RecBuffer lBlock(header.lblock);
            HeadInfo lHeader;
            lBlock.getHeader(&lHeader);
            lHeader.rblock = header.rblock;
            lBlock.setHeader(&lHeader);

            if (header.rblock != -1) {
                /* Get the header of the right block and set it's lblock to
                   this block's lblock */
                RecBuffer rBlock(header.rblock);
                HeadInfo rHeader;
                rBlock.getHeader(&rHeader);
                rHeader.lblock = header.lblock;
                rBlock.setHeader(&rHeader);
            } else {
                // (the block being released is the "Last Block" of the relation.)
                /* update the Relation Catalog entry's LastBlock field for this
                   relation with the block number of the previous block. */
                RelCatEntry attrCatEntry;
                RelCacheTable::getRelCatEntry(ATTRCAT_RELID, &attrCatEntry);
                attrCatEntry.lastBlk = header.lblock;
                RelCacheTable::setRelCatEntry(ATTRCAT_RELID, &attrCatEntry);
            }

            // call releaseBlock()
            attrCatBlock.releaseBlock();
        }

        // if index exists for the attribute (rootBlock != -1), call bplus destroy
        // if (rootBlock != -1) {
        //     BPlusTree::bPlusDestroy(rootBlock);
        // }
    }

    /*** Delete the entry corresponding to the relation from relation catalog ***/
    
    // Fetch the header of Relcat block
    HeadInfo relCatHeader;
    relCatBlock.getHeader(&relCatHeader);

    /* Decrement the numEntries in the header of the block corresponding to the
       relation catalog entry and set it back */
    relCatHeader.numEntries--;
    relCatBlock.setHeader(&relCatHeader);

    /* Get the slotmap in relation catalog, update it by marking the slot as
       free(SLOT_UNOCCUPIED) and set it back. */
    unsigned char relCatSlotMap[relCatHeader.numSlots];
    relCatBlock.getSlotMap(relCatSlotMap);
    relCatSlotMap[relCatRecId.slot] = SLOT_UNOCCUPIED;
    relCatBlock.setSlotMap(relCatSlotMap);

    /*** Updating the Relation Cache Table ***/
    
    /** Update relation catalog record entry (number of records in relation
        catalog is decreased by 1) **/
    RelCatEntry relCatCacheEntry;
    RelCacheTable::getRelCatEntry(RELCAT_RELID, &relCatCacheEntry);
    relCatCacheEntry.numRecs--;
    RelCacheTable::setRelCatEntry(RELCAT_RELID, &relCatCacheEntry);

    /** Update attribute catalog entry (number of records in attribute catalog
        is decreased by numberOfAttributesDeleted) **/
    RelCatEntry attrCatCacheEntry;
    RelCacheTable::getRelCatEntry(ATTRCAT_RELID, &attrCatCacheEntry);
    attrCatCacheEntry.numRecs -= numberOfAttributesDeleted;
    RelCacheTable::setRelCatEntry(ATTRCAT_RELID, &attrCatCacheEntry);

    return SUCCESS;
}

int BlockAccess::project(int relId, Attribute *record) {
    // get the previous search index of the relation relId from the relation cache
    RecId prevRecId;
    RelCacheTable::getSearchIndex(relId, &prevRecId);

    // declare block and slot which will be used to store the record id of the slot we need to check.
    int block, slot;

    /* if the current search index record is invalid(i.e. = {-1, -1})
       (this only happens when the caller reset the search index)
    */
    if (prevRecId.block == -1 && prevRecId.slot == -1) {
        // (new project operation. start from beginning)
        RelCatEntry relCatEntry;
        RelCacheTable::getRelCatEntry(relId, &relCatEntry);

        block = relCatEntry.firstBlk;
        slot = 0;
    } else {
        // (a project/search operation is already in progress)
        block = prevRecId.block;
        slot = prevRecId.slot + 1;
    }

    // The following code finds the next record of the relation
    while (block != -1) {
        // create a RecBuffer object for block 
        RecBuffer blockBuffer(block);

        // get header of the block 
        HeadInfo header;
        blockBuffer.getHeader(&header);

        // get slot map of the block 
        unsigned char slotMap[header.numSlots];
        blockBuffer.getSlotMap(slotMap);

        if (slot >= header.numSlots) {
            // (no more slots in this block)
            block = header.rblock;
            slot = 0;
        } else if (slotMap[slot] == SLOT_UNOCCUPIED) {
            // increment slot
            slot++;
        } else {
            // (the next occupied slot / record has been found)
            break;
        }
    }

    if (block == -1) {
        // (a record was not found. all records exhausted)
        return E_NOTFOUND;
    }

    // declare nextRecId to store the RecId of the record found
    RecId nextRecId = {block, slot};

    // set the search index to nextRecId 
    RelCacheTable::setSearchIndex(relId, &nextRecId);

    /* Copy the record with record id (nextRecId) to the record buffer (record) */
    RecBuffer targetBuffer(block);
    targetBuffer.getRecord(record, slot);

    return SUCCESS;
}