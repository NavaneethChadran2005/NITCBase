#include "OpenRelTable.h"
#include <cstdlib> 
#include <cstring> 

OpenRelTableMetaInfo OpenRelTable::tableMetaInfo[MAX_OPEN];

OpenRelTable::OpenRelTable() {

  // initialize relCache and attrCache with nullptr and tableMetaInfo to free
  for (int i = 0; i < MAX_OPEN; ++i) {
    RelCacheTable::relCache[i] = nullptr;
    AttrCacheTable::attrCache[i] = nullptr;
    tableMetaInfo[i].free = true;
  }

  /************ Setting up Relation Cache entries ************/
  
  /**** setting up Relation Catalog relation in the Relation Cache Table****/
  RecBuffer relCatBlock(RELCAT_BLOCK);
  Attribute relCatRecord[RELCAT_NO_ATTRS];
  
  relCatBlock.getRecord(relCatRecord, RELCAT_SLOTNUM_FOR_RELCAT);

  struct RelCacheEntry relCacheEntry;
  RelCacheTable::recordToRelCatEntry(relCatRecord, &relCacheEntry.relCatEntry);
  relCacheEntry.recId.block = RELCAT_BLOCK;
  relCacheEntry.recId.slot = RELCAT_SLOTNUM_FOR_RELCAT;

  RelCacheTable::relCache[RELCAT_RELID] = (struct RelCacheEntry*)malloc(sizeof(RelCacheEntry));
  *(RelCacheTable::relCache[RELCAT_RELID]) = relCacheEntry;

  /**** setting up Attribute Catalog relation in the Relation Cache Table ****/
  relCatBlock.getRecord(relCatRecord, RELCAT_SLOTNUM_FOR_ATTRCAT);

  RelCacheTable::recordToRelCatEntry(relCatRecord, &relCacheEntry.relCatEntry);
  relCacheEntry.recId.block = RELCAT_BLOCK;
  relCacheEntry.recId.slot = RELCAT_SLOTNUM_FOR_ATTRCAT;

  RelCacheTable::relCache[ATTRCAT_RELID] = (struct RelCacheEntry*)malloc(sizeof(RelCacheEntry));
  *(RelCacheTable::relCache[ATTRCAT_RELID]) = relCacheEntry;

  /************ Setting up Attribute cache entries ************/
  
  /**** setting up Relation Catalog relation in the Attribute Cache Table ****/
  RecBuffer attrCatBlock(ATTRCAT_BLOCK);
  Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
  
  AttrCacheEntry* head = nullptr;
  AttrCacheEntry* current = nullptr;

  for (int i = 0; i <= 5; i++) {
    attrCatBlock.getRecord(attrCatRecord, i);

    AttrCacheEntry* entry = (AttrCacheEntry*)malloc(sizeof(AttrCacheEntry));
    AttrCacheTable::recordToAttrCatEntry(attrCatRecord, &entry->attrCatEntry);
    entry->recId.block = ATTRCAT_BLOCK;
    entry->recId.slot = i;
    entry->next = nullptr;

    if (head == nullptr) {
        head = entry;
        current = entry;
    } else {
        current->next = entry;
        current = entry;
    }
  }
  AttrCacheTable::attrCache[RELCAT_RELID] = head;

  /**** setting up Attribute Catalog relation in the Attribute Cache Table ****/
  head = nullptr;
  current = nullptr;

  for (int i = 6; i <= 11; i++) {
    attrCatBlock.getRecord(attrCatRecord, i);

    AttrCacheEntry* entry = (AttrCacheEntry*)malloc(sizeof(AttrCacheEntry));
    AttrCacheTable::recordToAttrCatEntry(attrCatRecord, &entry->attrCatEntry);
    entry->recId.block = ATTRCAT_BLOCK;
    entry->recId.slot = i;
    entry->next = nullptr;

    if (head == nullptr) {
        head = entry;
        current = entry;
    } else {
        current->next = entry;
        current = entry;
    }
  }
  AttrCacheTable::attrCache[ATTRCAT_RELID] = head;


  /************ Setting up tableMetaInfo entries ************/
  tableMetaInfo[RELCAT_RELID].free = false;
  strcpy(tableMetaInfo[RELCAT_RELID].relName, RELCAT_RELNAME);

  tableMetaInfo[ATTRCAT_RELID].free = false;
  strcpy(tableMetaInfo[ATTRCAT_RELID].relName, ATTRCAT_RELNAME);

}

OpenRelTable::~OpenRelTable() {

    for (int i = 2; i < MAX_OPEN; ++i) {
        if (!OpenRelTable::tableMetaInfo[i].free) {
            // close the relation using openRelTable::closeRel().
            OpenRelTable::closeRel(i);
        }
    }

    /**** Closing the catalog relations in the relation cache ****/

    // releasing the relation cache entry of the attribute catalog
    if (RelCacheTable::relCache[ATTRCAT_RELID]->dirty) {

        /* Get the Relation Catalog entry from RelCacheTable::relCache
        Then convert it to a record using RelCacheTable::relCatEntryToRecord(). */
        union Attribute record[RELCAT_NO_ATTRS];
        RelCacheTable::relCatEntryToRecord(&(RelCacheTable::relCache[ATTRCAT_RELID]->relCatEntry), record);

        RecId recId = RelCacheTable::relCache[ATTRCAT_RELID]->recId;

        // declaring an object of RecBuffer class to write back to the buffer
        RecBuffer relCatBlock(recId.block);

        // Write back to the buffer using relCatBlock.setRecord() with recId.slot
        relCatBlock.setRecord(record, recId.slot);
    }
    // free the memory dynamically allocated to this RelCacheEntry
    free(RelCacheTable::relCache[ATTRCAT_RELID]);


    // releasing the relation cache entry of the relation catalog
    if (RelCacheTable::relCache[RELCAT_RELID]->dirty) {

        /* Get the Relation Catalog entry from RelCacheTable::relCache
        Then convert it to a record using RelCacheTable::relCatEntryToRecord(). */
        union Attribute record[RELCAT_NO_ATTRS];
        RelCacheTable::relCatEntryToRecord(&(RelCacheTable::relCache[RELCAT_RELID]->relCatEntry), record);

        RecId recId = RelCacheTable::relCache[RELCAT_RELID]->recId;

        // declaring an object of RecBuffer class to write back to the buffer
        RecBuffer relCatBlock(recId.block);

        // Write back to the buffer using relCatBlock.setRecord() with recId.slot
        relCatBlock.setRecord(record, recId.slot);
    }
    // free the memory dynamically allocated for this RelCacheEntry
    free(RelCacheTable::relCache[RELCAT_RELID]);


    // free the memory allocated for the attribute cache entries of the
    // relation catalog and the attribute catalog
    for (int relId = RELCAT_RELID; relId <= ATTRCAT_RELID; relId++) {
        AttrCacheEntry *current = AttrCacheTable::attrCache[relId];
        AttrCacheEntry *next;
        
        while (current != nullptr) {
            next = current->next;
            free(current);
            current = next;
        }
        AttrCacheTable::attrCache[relId] = nullptr;
    }
}

int OpenRelTable::getFreeOpenRelTableEntry() {
  for (int i = 0; i < MAX_OPEN; ++i) {
    if (tableMetaInfo[i].free) {
      return i;
    }
  }
  return E_CACHEFULL;
}

int OpenRelTable::getRelId(char relName[ATTR_SIZE]) {
  for (int i = 0; i < MAX_OPEN; ++i) {
    // If the slot is occupied and the names match, return its index
    if (!tableMetaInfo[i].free && strcmp(tableMetaInfo[i].relName, relName) == 0) {
      return i;
    }
  }
  return E_RELNOTOPEN;
}

int OpenRelTable::openRel(char relName[ATTR_SIZE]) {
  int relId = getRelId(relName);
  if (relId >= 0) {
    return relId;
  }

  relId = getFreeOpenRelTableEntry();
  if (relId == E_CACHEFULL) {
    return E_CACHEFULL;
  }

  // Set up the target value for linearSearch
  Attribute attrVal;
  strcpy(attrVal.sVal, relName);

  /****** Setting up Relation Cache entry ******/
  RelCacheTable::resetSearchIndex(RELCAT_RELID);
  
  // Search the Relation Catalog for this table name
  RecId relcatRecId = BlockAccess::linearSearch(RELCAT_RELID, (char*)"RelName", attrVal, EQ);

  if (relcatRecId.block == -1 && relcatRecId.slot == -1) {
    return E_RELNOTEXIST; 
  }

  RecBuffer relBuffer(relcatRecId.block);
  Attribute relRecord[RELCAT_NO_ATTRS];
  relBuffer.getRecord(relRecord, relcatRecId.slot);

  RelCacheEntry* relCacheEntry = (RelCacheEntry*)malloc(sizeof(RelCacheEntry));
  RelCacheTable::recordToRelCatEntry(relRecord, &(relCacheEntry->relCatEntry));
  relCacheEntry->recId = relcatRecId;
  
  RelCacheTable::relCache[relId] = relCacheEntry;

  /****** Setting up Attribute Cache entry ******/
  AttrCacheEntry* listHead = nullptr;
  AttrCacheEntry* current = nullptr;

  RelCacheTable::resetSearchIndex(ATTRCAT_RELID);

  while (true) {
    // linearSearch acts as our iterator - it will automatically use its internal 
    // bookmark to find the next column belonging to this table on each loop
    RecId attrcatRecId = BlockAccess::linearSearch(ATTRCAT_RELID, (char*)"RelName", attrVal, EQ);
    
    if (attrcatRecId.block == -1 && attrcatRecId.slot == -1) {
      break; // No more columns found for this table
    }

    RecBuffer attrBuffer(attrcatRecId.block);
    Attribute attrRecord[ATTRCAT_NO_ATTRS];
    attrBuffer.getRecord(attrRecord, attrcatRecId.slot);

    AttrCacheEntry* attrCacheEntry = (AttrCacheEntry*)malloc(sizeof(AttrCacheEntry));
    AttrCacheTable::recordToAttrCatEntry(attrRecord, &(attrCacheEntry->attrCatEntry));
    attrCacheEntry->recId = attrcatRecId;
    attrCacheEntry->next = nullptr;

    if (listHead == nullptr) {
      listHead = attrCacheEntry;
      current = attrCacheEntry;
    } else {
      current->next = attrCacheEntry;
      current = attrCacheEntry;
    }
  }

  AttrCacheTable::attrCache[relId] = listHead;

  /****** Setting up metadata ******/
  tableMetaInfo[relId].free = false;
  strcpy(tableMetaInfo[relId].relName, relName);

  return relId;
}

int OpenRelTable::closeRel(int relId) {
  // confirm that rel-id fits the following conditions
  if (relId < 2 || relId >= MAX_OPEN) {
    return E_OUTOFBOUND;
  }

  // does not correspond to a free slot
  if (OpenRelTable::tableMetaInfo[relId].free) {
    return E_RELNOTOPEN;
  }

  /****** Releasing the Relation Cache entry of the relation ******/

  if (RelCacheTable::relCache[relId]->dirty) {
    /* Get the Relation Catalog entry from RelCacheTable::relCache */
    union Attribute record[RELCAT_NO_ATTRS];
    
    /* Then convert it to a record using RelCacheTable::relCatEntryToRecord(). */
    RelCacheTable::relCatEntryToRecord(&(RelCacheTable::relCache[relId]->relCatEntry), record);

    RecId recId = RelCacheTable::relCache[relId]->recId;

    // declaring an object of RecBuffer class to write back to the buffer
    RecBuffer relCatBlock(recId.block);

    // Write back to the buffer using relCatBlock.setRecord() with recId.slot
    relCatBlock.setRecord(record, recId.slot);
  }

  /****** Releasing the Attribute Cache entry of the relation ******/

  // free the memory allocated in the attribute caches which was
  // allocated in the OpenRelTable::openRel() function
  AttrCacheEntry *current = AttrCacheTable::attrCache[relId];
  AttrCacheEntry *next;
  
  while (current != nullptr) {
    next = current->next;
    free(current);
    current = next;
  }
  
  // also free the relation cache memory
  free(RelCacheTable::relCache[relId]);
  
  AttrCacheTable::attrCache[relId] = nullptr;
  RelCacheTable::relCache[relId] = nullptr;

  /****** Set the Open Relation Table entry of the relation as free ******/

  // update `metainfo` to set `relId` as a free slot
  OpenRelTable::tableMetaInfo[relId].free = true;

  return SUCCESS;
}