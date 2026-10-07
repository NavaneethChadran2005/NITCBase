#include "Buffer/StaticBuffer.h"
#include "Cache/OpenRelTable.h"
#include "Disk_Class/Disk.h"
#include "FrontendInterface/FrontendInterface.h"
#include <iostream>

int main(int argc, char *argv[]) {
  Disk disk_run;
  StaticBuffer buffer;
  OpenRelTable cache;

  // i = 0 corresponds to RELCAT_RELID
  // i = 1 corresponds to ATTRCAT_RELID
  for (int i = 0; i <= 2; i++) {
    
    RelCatEntry relCatBuf;
    
    // get the relation catalog entry using RelCacheTable::getRelCatEntry()
    RelCacheTable::getRelCatEntry(i, &relCatBuf);
    
    printf("Relation: %s\n", relCatBuf.relName);

    // j loops from 0 to numAttrs of the relation - 1
    for (int j = 0; j < relCatBuf.numAttrs; j++) {
      
      AttrCatEntry attrCatBuf;
      
      // get the attribute catalog entry for (rel-id i, attribute offset j)
      AttrCacheTable::getAttrCatEntry(i, j, &attrCatBuf);

      // determine the string representation of the attribute type
      const char *attrType = attrCatBuf.attrType == NUMBER ? "NUM" : "STR";
      
      printf("  %s: %s\n", attrCatBuf.attrName, attrType);
    }
    
    printf("\n");
  }

  return 0;
}