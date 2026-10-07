#include "RelCacheTable.h"

#include <cstring>

RelCacheEntry* RelCacheTable::relCache[MAX_OPEN];

/*
    Get the relation catalog entry for the relation with rel-id 'relId' from the cache
    NOTE: this function expects the caller to allocate memory for '*relCatBuf'
*/
int RelCacheTable::getRelCatEntry(int relId, RelCatEntry* relCatBuf){
    if(relId < 0 || relId >= MAX_OPEN){
        return E_OUTOFBOUND;
    }

    // if there is no entry at the rel-id
    if(relCache[relId] == nullptr){
        return E_RELNOTOPEN;
    }

    // copy the value to the relCatBuf argument
    *relCatBuf = relCache[relId]->relCatEntry;

    return SUCCESS;
}

/*
    Converts a relation catalog record to RelCatEntry struct.
    We get the record as Attribute[] for the BlockBuffer.getRecord().
    This function will convert that to a struct RelCatEntry type.
    NOTE: this function expects the caller to allocate memory for '*relCatEntry'
*/
void RelCacheTable::recordToRelCatEntry(union Attribute record[RELCAT_NO_ATTRS], RelCatEntry* relCatEntry){
    strcpy(relCatEntry->relName,record[RELCAT_REL_NAME_INDEX].sVal);
    relCatEntry->numAttrs = (int)record[RELCAT_NO_ATTRIBUTES_INDEX].nVal;
    relCatEntry->numRecs = (int)record[RELCAT_NO_RECORDS_INDEX].nVal;
    relCatEntry->firstBlk = (int)record[RELCAT_FIRST_BLOCK_INDEX].nVal;
    relCatEntry->lastBlk = (int)record[RELCAT_LAST_BLOCK_INDEX].nVal;
    relCatEntry->numSlotsPerBlk = (int)record[RELCAT_NO_SLOTS_PER_BLOCK_INDEX].nVal;
}

/*
    will return the searchIndex for the relation corresponding to `relId`
    NOTE: This function expects the caller to allocate memory for `*searchIndex`
*/
int RelCacheTable::getSearchIndex(int relId, RecId* searchIndex){
    //check if relId is within the bound
    if(relId <0 || relId >= MAX_OPEN){
        return E_OUTOFBOUND;
    }

    //check if relation at 'relId' is open or not. We can get to know if it has an entry in relation cache or not
    if(relCache[relId] == nullptr){
        return E_RELNOTOPEN;
    }

    // copy the searchIndex from Relation Cache entry to the searchIndex variable passed into parameter.
    *searchIndex = relCache[relId]->searchIndex;
    return SUCCESS;
}

// setSearchIndex
/*
    During a liner search operation if the record is found, we update searchIndex
    with the rec-id ({block, slot}) uing this function.
*/
int RelCacheTable::setSearchIndex(int relId, RecId* searchIndex){
    //check if relId is within the bound
    if(relId < 0 || relId >= MAX_OPEN){
        return E_OUTOFBOUND;
    }

    // check if relation at 'relId' is open or not
    if(relCache[relId] == nullptr){
        return E_RELNOTOPEN;
    }

    // update the searchIndex value in relCache for the relId to the searchIndex argument
    relCache[relId]->searchIndex = *searchIndex;
    return SUCCESS;
}

//resetSearchIndex
int RelCacheTable::resetSearchIndex(int relId){
    // check if relId is within the bound
    if(relId < 0 || relId >= MAX_OPEN){
        return E_OUTOFBOUND;
    }

    // check if relation at 'relId' is open or not
    if(relCache[relId] == nullptr){
        return E_RELNOTOPEN;
    }

    // reset
    relCache[relId]->searchIndex.block = -1;
    relCache[relId]->searchIndex.slot = -1;
    return SUCCESS;
}

/*
setRelCatEntry() => sets the Relation Catalog entry corresponding to the specific relation
in the Relation Cache table.
*/
int RelCacheTable::setRelCatEntry(int relId, RelCatEntry *relCatBuf){
    if(relId < 0 || relId >= 12){
        return E_OUTOFBOUND;
    }

    if(relCache[relId] == nullptr){
        return E_RELNOTOPEN;
    }

    /*
        Copy the relCatBuf to the corresponding Relation Catalog entry in
        the Relation Cache table.
    */
    relCache[relId]->relCatEntry = *relCatBuf;

    /*
        set the dirty flag of the corresponding Relation Cache entry in
        the Relation Cache table.
    */
    relCache[relId]->dirty = true;

    return SUCCESS;
}

/*
    relCatEntryToRecord()
    converts RelCatEntry structure to a record.
*/
void RelCacheTable::relCatEntryToRecord(RelCatEntry *relCatEntry, union Attribute record[RELCAT_NO_ATTRS]){
    strcpy(record[RELCAT_REL_NAME_INDEX].sVal, relCatEntry->relName);
    record[RELCAT_NO_ATTRIBUTES_INDEX].nVal = relCatEntry->numAttrs;
    record[RELCAT_NO_RECORDS_INDEX].nVal = relCatEntry->numRecs;
    record[RELCAT_FIRST_BLOCK_INDEX].nVal = relCatEntry->firstBlk;
    record[RELCAT_LAST_BLOCK_INDEX].nVal = relCatEntry->lastBlk;
    record[RELCAT_NO_SLOTS_PER_BLOCK_INDEX].nVal = relCatEntry->numSlotsPerBlk;
}
