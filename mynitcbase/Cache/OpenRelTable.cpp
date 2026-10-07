#include "OpenRelTable.h"
#include<iostream>
#include <cstring>

OpenRelTableMetaInfo OpenRelTable::tableMetaInfo[MAX_OPEN];

OpenRelTable:: OpenRelTable() {
    // initialize relCache and attrCache with nullptr
    for(int i = 0; i< MAX_OPEN; i++){
        RelCacheTable::relCache[i] = nullptr;
        AttrCacheTable::attrCache[i] = nullptr;
    }


    /******* Setting up Relation Cache entries *********/
    // we need to populate relation cache with entries for the relation and attribute catalog.

    // Setting up Relation Catalog relation in the Relation Cache Table
    RecBuffer relCatBlock(RELCAT_BLOCK);
    Attribute relCatRecord[RELCAT_NO_ATTRS];

    relCatBlock.getRecord(relCatRecord, RELCAT_SLOTNUM_FOR_RELCAT);
    struct RelCacheEntry relCacheEntry;
    RelCacheTable::recordToRelCatEntry(relCatRecord, &relCacheEntry.relCatEntry);
    relCacheEntry.recId.block = RELCAT_BLOCK;
    relCacheEntry.recId.slot = RELCAT_SLOTNUM_FOR_RELCAT; // 0
    // allocate this on the heap beacuse we want it to persist outside this function
    RelCacheTable::relCache[RELCAT_RELID] = (struct RelCacheEntry*)malloc(sizeof(RelCacheEntry));
    *(RelCacheTable::relCache[RELCAT_RELID]) = relCacheEntry;



    // Setting up Attribute Catalog relation in the Relation Cache Table
    relCatBlock.getRecord(relCatRecord, RELCAT_SLOTNUM_FOR_ATTRCAT);
    // set up the relation cache entry for the attribute catalog similarly
    // from the record at RELCAT_SLOTNUM_FOR_ATTRCAT
    RelCacheTable::recordToRelCatEntry(relCatRecord, &relCacheEntry.relCatEntry);
    relCacheEntry.recId.block = RELCAT_BLOCK; //block number 4 (relational catalog block)
    relCacheEntry.recId.slot = RELCAT_SLOTNUM_FOR_ATTRCAT; // 1
    RelCacheTable::relCache[ATTRCAT_RELID] = (struct RelCacheEntry*)malloc(sizeof(RelCacheEntry));
    *(RelCacheTable::relCache[ATTRCAT_RELID]) = relCacheEntry;


    /******** Setting up Attribute Cache Entries *********/
    // Setting up Relation Catalog relation in the Attribute Cache Table
    RecBuffer attrCatBlock(ATTRCAT_BLOCK);
    Attribute attrCatRecord[ATTRCAT_NO_ATTRS];

    // iterate through all the attributes of the relation catalog and create a linked list
    // NOTE: allocate each entry dynamically using malloc
    struct AttrCacheEntry *head, *last;
    for(int i = 0; i<6; i++){
        attrCatBlock.getRecord(attrCatRecord,i);
        struct AttrCacheEntry* attrCacheEntry = (struct AttrCacheEntry*)malloc(sizeof(AttrCacheEntry));
        AttrCacheTable::recordToAttrCatEntry(attrCatRecord, &attrCacheEntry->attrCatEntry);
        attrCacheEntry->recId.block = ATTRCAT_BLOCK;
        attrCacheEntry->recId.slot = i;
        if(i == 0){
            head = attrCacheEntry;
            last = attrCacheEntry;
        }
        else{
            last->next = attrCacheEntry;
            last = last->next;
        }
    }
    last->next = nullptr;
    AttrCacheTable::attrCache[RELCAT_RELID] = head;


    // Setting up Attribute Catalog relation in the Attribute Cache Table
    for(int i = 6; i<12; i++){
        attrCatBlock.getRecord(attrCatRecord,i);
        struct AttrCacheEntry* attrCacheEntry = (struct AttrCacheEntry*)malloc(sizeof(AttrCacheEntry));
        AttrCacheTable::recordToAttrCatEntry(attrCatRecord, &attrCacheEntry->attrCatEntry);
        attrCacheEntry->recId.block = ATTRCAT_BLOCK;
        attrCacheEntry->recId.slot = i;
        if(i == 6){
            head = attrCacheEntry;
            last = attrCacheEntry;
        }
        else{
            last->next = attrCacheEntry;
            last = attrCacheEntry;
        }
    }
    last->next = nullptr;
    AttrCacheTable::attrCache[ATTRCAT_RELID] = head;

    /*************** Setting up tableMetaInfo entries *******************/
    for(int i = 0; i<MAX_OPEN; i++){
        if(i == RELCAT_RELID){ // i == 0
            tableMetaInfo[i].free = false;
            strcpy(tableMetaInfo[0].relName, RELCAT_RELNAME);
        }
        else if(i == ATTRCAT_RELID){    // i == 1
            tableMetaInfo[i].free = false;
            strcpy(tableMetaInfo[1].relName, ATTRCAT_RELNAME);
        }
        else{
            tableMetaInfo[i].free = true;
        }
    }


}


OpenRelTable::~OpenRelTable(){

    // close all the open relations from rel-id = 2 onwards
    for(int i = 2; i<MAX_OPEN; i++){
        if(!tableMetaInfo[i].free){
            OpenRelTable::closeRel(i);
        }
    }

    /**** Closing the catalog relations in the relation cache *****/
    
    // releasing the relation cache entry of the attribute catalog
    if (RelCacheTable::relCache[ATTRCAT_RELID]->dirty == true){
         /* 
            Get the Relation Catalog entry from RelCacheTable::relCache
            Then convert it to record
         */
        RelCatEntry relCatEntry = RelCacheTable::relCache[ATTRCAT_RELID]->relCatEntry;
        Attribute relCatRecord[RELCAT_NO_ATTRS];
        RelCacheTable::relCatEntryToRecord(&relCatEntry, relCatRecord);

        // get the record id form the relCache, it stores where the relation is present in Relation Catalog
        RecId recId = RelCacheTable::relCache[ATTRCAT_RELID]->recId;
        RecBuffer relCatBlock(recId.block);

        // write back to the buffer using relCatBlock.setRecord()
        relCatBlock.setRecord(relCatRecord, recId.slot);
    }
    // free the memory which was dynamically allocted to to relCache[ATTRCAT_RELID]
    free(RelCacheTable::relCache[ATTRCAT_RELID]);

    // releasing the relation cache entry of the relation catalog
    if(RelCacheTable::relCache[RELCAT_RELID]->dirty == true){
        /*
            Get the Relation catalog entry from RelCacheTable::relCache
            and convert it to record
         */
         RelCatEntry relCatEntry = RelCacheTable::relCache[RELCAT_RELID]->relCatEntry;
         Attribute relCatRecord[RELCAT_NO_ATTRS];
         RelCacheTable::relCatEntryToRecord(&relCatEntry, relCatRecord);

         RecId recId = RelCacheTable::relCache[RELCAT_RELID]->recId;
         RecBuffer relCatBlock(recId.block);
         relCatBlock.setRecord(relCatRecord, recId.slot);
    }
    free(RelCacheTable::relCache[RELCAT_RELID]);


    // free the memory allocated for the attribute cache entries of the relation catalog and the attribute catalog
    // NOTE: they are never changed because the attributes of relcat and attrcat are never modified.
    for(int i = 0; i < 2; i++){
        struct AttrCacheEntry *entry = AttrCacheTable::attrCache[i];
        while(entry != nullptr){
            struct AttrCacheEntry *temp = entry;
            entry = entry->next;
            free(temp);
        }
    }

}


/* This function will open a relation having name `relName`.
Since we are currently only working with the relation and attribute catalog, we
will just hardcode it. In subsequent stages, we will loop through all the relations
and open the appropriate one.
*/
int OpenRelTable::getRelId(char relName[ATTR_SIZE]){

    // traverse through the tableMetaInfo and check for the relName
    for(int i = 0; i<MAX_OPEN; i++){
        if(tableMetaInfo[i].free == false && strcmp(tableMetaInfo[i].relName, relName)==0 ){
            return i;
        }
    }

    // if not found
    return E_RELNOTOPEN;
}


// openRelTable::getFreeOpenRelTableEntry() => returns the first free entry in open relation cache.
int OpenRelTable::getFreeOpenRelTableEntry(){
    for(int i = 2; i<MAX_OPEN; i++){
        if(tableMetaInfo[i].free){
            return i;
        }
    }

    return E_CACHEFULL;
}

// openRelTable::openRel(relName)
/*
    Creates an entry for the input relation in the Open Relation Table and returns
    the corresponding rel-id
*/
int OpenRelTable::openRel(char relName[ATTR_SIZE]){
    // if relation with relName already has an entry in the Open Relation Table, return the rel-id
    int relId = getRelId(relName);
    if(relId != E_RELNOTOPEN){
        return relId;
    }

    // find a free slot in the Open Relation Table
    relId = getFreeOpenRelTableEntry();
    if(relId < 0){
        return E_CACHEFULL;
    }

    /******* Setting up Relation Cache entry for the free slot *********/
    // search for the entry with the relation name, relName, in the Relation Catalog using linearSearch()
    Attribute relationName;
    strcpy(relationName.sVal, relName);
    RelCacheTable::resetSearchIndex(RELCAT_RELID);

    char relCatAttrRelName[ATTR_SIZE];
    strcpy(relCatAttrRelName, RELCAT_ATTR_RELNAME);

    // relcatRecId stores the rec-id of the relation `relName` in the Relation Catalog.
    RecId relCatRecId = BlockAccess::linearSearch(RELCAT_RELID, relCatAttrRelName, relationName, EQ);

    // if the relation is not found in the Relation Catalog.
    if(relCatRecId.block == -1 && relCatRecId.slot == -1){
        return E_RELNOTEXIST;
    }

    /*
        Read the record entry corresponding to the relcatRecId and create a relCacheEntry
        on it using RecBuffer::getRecord() and RecCacheTable::recordToRelCatEntry().
        Update the recId field of this Relation Cache entry to relcatRecId.
        Use the relation cache entry to set the relId-th entry of the RelCacheTable.

        NOTE: make sure to allocate memory for the RelCacheEntry using malloc()
    */
    RecBuffer relCatBlock(relCatRecId.block); // here instead we can also use RELCAT_RELID
    Attribute relCatRecord[RELCAT_NO_ATTRS];
    relCatBlock.getRecord(relCatRecord, relCatRecId.slot);
    struct RelCacheEntry relCacheEntry;
    RelCacheTable::recordToRelCatEntry(relCatRecord, &relCacheEntry.relCatEntry);
    relCacheEntry.recId = relCatRecId;
    RelCacheTable::relCache[relId] = (struct RelCacheEntry*)malloc(sizeof(RelCacheEntry));
    *(RelCacheTable::relCache[relId]) = relCacheEntry;

    /******** Setting up Attribute Cache entry for the relation *********/

    // let listHead be used to hold the head of the linked list of attrCache entries.
    AttrCacheEntry* listHead, *current;

    /*
        Iterate over all the entries in the Attribute Catalog corresponding to each
        attribute of the relation relName by multiple calls of BlockAccess::linearSearch().
        Care should be take to reset the searchIndex of the relation, ATTRCAT_RELID,
        corresponding to Attribute Catalog before the first call to linearSearch().
    */
    RecId attrCatRecord;
    RelCacheTable::resetSearchIndex(ATTRCAT_RELID);
    for(int i = 0; i<relCacheEntry.relCatEntry.numAttrs; i++){
        /* let attrcatRecId store a valid record id an entry of the relation, relName,
           in the Attribute Catalog.
        */
        RecId attrcatRecId = BlockAccess::linearSearch(ATTRCAT_RELID, relCatAttrRelName, relationName, EQ);
        /*  read the record entry corresponding to attrcatRecId and create an
            Attribute Cache entry on it using RecBuffer::getRecord() and
            AttrCacheTable::recordToAttrCatEntry().
            update the recId field of this Attribute Cache entry to attrcatRecId.
            add the Attribute Cache entry to the linked list of listHead .
        */
      // NOTE: make sure to allocate memory for the AttrCacheEntry using malloc()
      RecBuffer attrCatBlock(attrcatRecId.block);
      Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
      attrCatBlock.getRecord(attrCatRecord, attrcatRecId.slot);
      struct AttrCacheEntry* attrCacheEntry = (struct AttrCacheEntry*)malloc(sizeof(AttrCacheEntry));
      AttrCacheTable::recordToAttrCatEntry(attrCatRecord, &attrCacheEntry->attrCatEntry);
      attrCacheEntry->recId = attrcatRecId;

      if(i==0){
        listHead = attrCacheEntry;
        current = attrCacheEntry;
      }
      else{
        current->next = attrCacheEntry;
        current = attrCacheEntry;
      }
    }

    current->next = nullptr;

    // set the relId-th entry of the AttrCacheTable to listHead
    AttrCacheTable::attrCache[relId] = listHead;


    /********* Setting up metadata in the Open Relation Table for the relation *********/
    // update the relIdth entry of the tableMetaInfo with free as false and relName as the input.
    OpenRelTable::tableMetaInfo[relId].free = false;
    strcpy(OpenRelTable::tableMetaInfo[relId].relName, relName);

    return relId;

}


int OpenRelTable::closeRel(int relId){
    if(relId == RELCAT_RELID || relId == ATTRCAT_RELID){
        return E_NOTPERMITTED;
    }
    if(relId < 0 || relId >= MAX_OPEN){
        return E_OUTOFBOUND;
    }
    if(tableMetaInfo[relId].free == true){
        return E_RELNOTOPEN;
    }

    if(RelCacheTable::relCache[relId]->dirty == true){
        /*
            Get the Relation Catalog entry from RelCacheTable::relCache,
            then convert it to a record using RelCacheTable::relCatToRecord().
        */
        Attribute relCatRecord[RELCAT_NO_ATTRS];
        RelCacheTable::relCatEntryToRecord(&RelCacheTable::relCache[relId]->relCatEntry, relCatRecord);
        RecBuffer relCatBlock(RelCacheTable::relCache[relId]->recId.block);
        relCatBlock.setRecord(relCatRecord,RelCacheTable::relCache[relId]->recId.slot);
    }

    // free the memory allocated in the relation and attribute caches which was allocated in the openRel()
    free(RelCacheTable::relCache[relId]);
    AttrCacheEntry *entry, *temp;
    entry = AttrCacheTable::attrCache[relId];
    while(entry!= nullptr){
        temp = entry;
        entry = entry->next;
        free(temp);
    }

    // update 'tableMetaInfo' to set 'relId' as a free slot;
    tableMetaInfo[relId].free = true;

    // update 'relCache' and 'attrCache' to set the entry at 'relId' to nullptr;
    RelCacheTable::relCache[relId] = nullptr;
    AttrCacheTable::attrCache[relId] = nullptr;

    return SUCCESS;
    
}



