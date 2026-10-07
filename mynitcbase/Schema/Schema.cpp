#include "Schema.h"

#include <cmath>
#include <cstring>

int Schema::openRel(char relName[ATTR_SIZE]) {
  int ret = OpenRelTable::openRel(relName);

  // the OpenRelTable::openRel() function returns the rel-id if successful
  // a valid rel-id will be within the range 0 <= relId < MAX_OPEN and any
  // error codes will be negative
  if(ret >= 0){
    return SUCCESS;
  }

  //otherwise it returns an error message
  return ret;
}

int Schema::closeRel(char relName[ATTR_SIZE]) {
  // if relation is relation catalog or attribute catalog, it cannot be closed
  if (strcmp(relName, RELCAT_RELNAME) == 0 || strcmp(relName, ATTRCAT_RELNAME) == 0) {
    return E_NOTPERMITTED;
  }

  // this function returns the rel-id of a relation if it is open or
  // E_RELNOTOPEN if it is not.
  int relId = OpenRelTable::getRelId(relName);

  // if the relation is not open
  if (relId == E_RELNOTOPEN) {
    return E_RELNOTOPEN;
  }

  return OpenRelTable::closeRel(relId);
}

int Schema::renameRel(char oldRelName[ATTR_SIZE], char newRelName[ATTR_SIZE]) {
  // if the oldRelName or newRelName is either Relation Catalog or Attribute Catalog,
  // return E_NOTPERMITTED
  if (strcmp(oldRelName, RELCAT_RELNAME) == 0 || strcmp(oldRelName, ATTRCAT_RELNAME) == 0 ||
      strcmp(newRelName, RELCAT_RELNAME) == 0 || strcmp(newRelName, ATTRCAT_RELNAME) == 0) {
    return E_NOTPERMITTED;
  }

  // if the relation is open
  //    (check if OpenRelTable::getRelId() returns E_RELNOTOPEN)
  //    return E_RELOPEN
  if (OpenRelTable::getRelId(oldRelName) != E_RELNOTOPEN) {
    return E_RELOPEN;
  }

  // retVal = BlockAccess::renameRelation(oldRelName, newRelName);
  // return retVal
  int retVal = BlockAccess::renameRelation(oldRelName, newRelName);
  return retVal;
}

int Schema::renameAttr(char *relName, char *oldAttrName, char *newAttrName) {
  // if the relName is either Relation Catalog or Attribute Catalog,
  // return E_NOTPERMITTED
  if (strcmp(relName, RELCAT_RELNAME) == 0 || strcmp(relName, ATTRCAT_RELNAME) == 0) {
    return E_NOTPERMITTED;
  }

  // if the relation is open
  //    (check if OpenRelTable::getRelId() returns E_RELNOTOPEN)
  //    return E_RELOPEN
  if (OpenRelTable::getRelId(relName) != E_RELNOTOPEN) {
    return E_RELOPEN;
  }

  // Call BlockAccess::renameAttribute with appropriate arguments.
  int retVal = BlockAccess::renameAttribute(relName, oldAttrName, newAttrName);
  
  // return the value returned by the above renameAttribute() call
  return retVal;
}

int Schema::createRel(char relName[], int nAttrs, char attrs[][ATTR_SIZE], int attrtype[]) {
    // declare variable relNameAsAttribute of type Attribute
    Attribute relNameAsAttribute;
    
    // copy the relName into relNameAsAttribute.sVal
    strcpy(relNameAsAttribute.sVal, relName);

    // Reset the searchIndex using RelCacheTable::resetSearchIndex()
    RelCacheTable::resetSearchIndex(RELCAT_RELID);

    // Search the relation catalog for attribute value "RelName" = relNameAsAttribute
    RecId targetRelId = BlockAccess::linearSearch(RELCAT_RELID, "RelName", relNameAsAttribute, EQ);

    // if a relation with name `relName` already exists
    if (targetRelId.block != -1 && targetRelId.slot != -1) {
        return E_RELEXIST;
    }

    // compare every pair of attributes of attrNames[] array
    for (int i = 0; i < nAttrs; i++) {
        for (int j = i + 1; j < nAttrs; j++) {
            if (strcmp(attrs[i], attrs[j]) == 0) {
                return E_DUPLICATEATTR;
            }
        }
    }

    /* declare relCatRecord of type Attribute which will be used to store the
       record corresponding to the new relation which will be inserted
       into relation catalog */
    Attribute relCatRecord[RELCAT_NO_ATTRS];

    // fill relCatRecord fields
    strcpy(relCatRecord[RELCAT_REL_NAME_INDEX].sVal, relName);
    relCatRecord[RELCAT_NO_ATTRIBUTES_INDEX].nVal = nAttrs;
    relCatRecord[RELCAT_NO_RECORDS_INDEX].nVal = 0;
    relCatRecord[RELCAT_FIRST_BLOCK_INDEX].nVal = -1;
    relCatRecord[RELCAT_LAST_BLOCK_INDEX].nVal = -1;
    
    // (number of slots is calculated as specified in the physical layer docs)
    // Integer division automatically handles the floor() requirement
    relCatRecord[RELCAT_NO_SLOTS_PER_BLOCK_INDEX].nVal = (2016 / (16 * nAttrs + 1));

    int retVal = BlockAccess::insert(RELCAT_RELID, relCatRecord);
    // if BlockAccess::insert fails return retVal
    if (retVal != SUCCESS) {
        return retVal;
    }

    // iterate through 0 to numOfAttributes - 1 :
    for (int i = 0; i < nAttrs; i++) {
        /* declare Attribute attrCatRecord to store the attribute catalog
           record corresponding to i'th attribute of the argument passed*/
        Attribute attrCatRecord[ATTRCAT_NO_ATTRS];

        // fill attrCatRecord fields
        strcpy(attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal, relName);
        strcpy(attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, attrs[i]);
        attrCatRecord[ATTRCAT_ATTR_TYPE_INDEX].nVal = attrtype[i];
        attrCatRecord[ATTRCAT_PRIMARY_FLAG_INDEX].nVal = -1;
        attrCatRecord[ATTRCAT_ROOT_BLOCK_INDEX].nVal = -1;
        attrCatRecord[ATTRCAT_OFFSET_INDEX].nVal = i;

        int attrRetVal = BlockAccess::insert(ATTRCAT_RELID, attrCatRecord);
        
        /* if attribute catalog insert fails:
             delete the relation by calling deleteRel(targetrel) of schema layer
             return E_DISKFULL
        */
        if (attrRetVal != SUCCESS) {
            Schema::deleteRel(relName); 
            return E_DISKFULL; 
        }
    }

    return SUCCESS;
}

int Schema::deleteRel(char *relName) {
    // if the relation to delete is either Relation Catalog or Attribute Catalog,
    //     return E_NOTPERMITTED
    if (strcmp(relName, RELCAT_RELNAME) == 0 || strcmp(relName, ATTRCAT_RELNAME) == 0) {
        return E_NOTPERMITTED;
    }

    // get the rel-id using appropriate method of OpenRelTable class by
    // passing relation name as argument
    int relId = OpenRelTable::getRelId(relName);

    // if relation is opened in open relation table, return E_RELOPEN
    if (relId != E_RELNOTOPEN) {
        return E_RELOPEN;
    }

    // Call BlockAccess::deleteRelation() with appropriate argument.
    int retVal = BlockAccess::deleteRelation(relName);

    // return the value returned by the above deleteRelation() call
    return retVal;
}