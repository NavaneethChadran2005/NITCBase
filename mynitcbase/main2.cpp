#include "Buffer/StaticBuffer.h"
#include "Cache/OpenRelTable.h"
#include "Disk_Class/Disk.h"
#include "FrontendInterface/FrontendInterface.h"
#include <iostream>
#include <cstring>

int main(int argc, char *argv[]) {
  Disk disk_run;
  //UPDATE THE SCHEMA (Change 'Class' to 'Batch')
  
  int currentUpdateBlock = ATTRCAT_BLOCK;

  // Traverse all Attribute Catalog blocks
  while (currentUpdateBlock != -1) {
    RecBuffer attrCatBuffer(currentUpdateBlock);
    HeadInfo attrCatHeader;
    attrCatBuffer.getHeader(&attrCatHeader);

    for (int j = 0; j < attrCatHeader.numEntries; j++) {
      Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
      attrCatBuffer.getRecord(attrCatRecord, j);

      // Check if this is the "Students" table AND the "Class" column
      if (strcmp(attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal, "Students") == 0 &&
          strcmp(attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, "Batch") == 0) {
        
        // Overwrite the string "Class" with "Batch"
        strcpy(attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, "Class");
        
        // Write the modified array back to the block buffer and disk
        attrCatBuffer.setRecord(attrCatRecord, j);
        
        printf("Successfully updated 'Class' to 'Batch'.\n\n");
        break;
      }
    }
    // Move to the next block in the Attribute Catalog chain
    currentUpdateBlock = attrCatHeader.rblock;
  }
  //PRINT THE SCHEMA 

  // Initialize the Relation Catalog buffer and header
  RecBuffer relCatBuffer(RELCAT_BLOCK);
  HeadInfo relCatHeader;
  relCatBuffer.getHeader(&relCatHeader);

  // Outer Loop: Iterate through all relations in the Relation Catalog
  for (int i = 0; i < relCatHeader.numEntries; i++) {
    
    Attribute relCatRecord[RELCAT_NO_ATTRS];
    relCatBuffer.getRecord(relCatRecord, i);
    printf("Relation: %s\n", relCatRecord[RELCAT_REL_NAME_INDEX].sVal);

    // Start searching at the first block of the Attribute Catalog
    int currentSearchBlock = ATTRCAT_BLOCK;

    // Continue as long as the current block is valid (not -1)
    while (currentSearchBlock != -1) {
      
      // Load the current block in the linked list
      RecBuffer attrCatBuffer(currentSearchBlock);
      HeadInfo attrCatHeader;
      attrCatBuffer.getHeader(&attrCatHeader);

      // Inner Loop: Check all entries in the CURRENT block
      for (int j = 0; j < attrCatHeader.numEntries; j++) {
        
        Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
        attrCatBuffer.getRecord(attrCatRecord, j);

        // If the attribute belongs to the current relation, print it
        if (strcmp(attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal, relCatRecord[RELCAT_REL_NAME_INDEX].sVal) == 0) {
          const char *attrType = attrCatRecord[ATTRCAT_ATTR_TYPE_INDEX].nVal == NUMBER ? "NUM" : "STR";
          printf("  %s: %s\n", attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, attrType);
        }
      }
      
      // Move to the next block in the chain
      currentSearchBlock = attrCatHeader.rblock;
    }
    
    printf("\n");
  }

  return 0;
}