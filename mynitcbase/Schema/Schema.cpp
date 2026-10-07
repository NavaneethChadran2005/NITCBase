#include "Schema.h"

#include <cmath>
#include <cstring>

int Schema::openRel(char relName[ATTR_SIZE]){
    int ret = OpenRelTable::openRel(relName);

    // OpenRelTable::openRel() will send rel-id [0,12].
    //If relation is not open, any error code will return negative integer.
    if(ret >= 0){
        return SUCCESS;
    }

    return ret;
}

int Schema::closeRel(char relName[ATTR_SIZE]){
    // if relation in relation catalog or attribute catalog, the not permited to close
    if( strcmp(relName,RELCAT_RELNAME) == 0 || strcmp(relName,ATTRCAT_RELNAME) == 0 ){
        return E_NOTPERMITTED;
    }

    int relId = OpenRelTable::getRelId(relName);
    if( relId == E_RELNOTOPEN){
        return E_RELNOTOPEN;
    }

    return OpenRelTable::closeRel(relId);
}

// renameRel(): method to change the relation name of specified relation to a new specified name.
int Schema::renameRel(char oldRelName[ATTR_SIZE], char newRelName[ATTR_SIZE]){
    // oldRelName and newRelName should not be same as relation catalog or attribute catalog name.
    if(strcmp(oldRelName,RELCAT_RELNAME) == 0 || strcmp(newRelName,RELCAT_RELNAME) == 0 || strcmp(oldRelName,ATTRCAT_RELNAME) == 0 || strcmp(newRelName,ATTRCAT_RELNAME) == 0){
        return E_NOTPERMITTED;
    }

    // check if relation is closed or not
    // NOTE: Relation must be close to perform this function
    int relId = OpenRelTable:: getRelId(oldRelName);
    if(relId >= 0){
        return E_RELOPEN;
    }

    int retVal = BlockAccess:: renameRelation(oldRelName, newRelName);
    return retVal;
}

// renameAttr(): method used to change attrname of a given relation
// NOTE: Relation must be close to perfrom this operation.
int Schema::renameAttr(char relName[ATTR_SIZE], char oldAttrName[ATTR_SIZE], char newAttrName[ATTR_SIZE]){
    // check if relName is not same as RELCAT_NAME and ATTRCAT_NAME
    if(strcmp(relName,RELCAT_RELNAME) == 0 || strcmp(relName,ATTRCAT_RELNAME) == 0){
        return E_NOTPERMITTED;
    }

    // check if relation is closed or not
    int relId = OpenRelTable::getRelId(relName);
    if(relId >=0){
        return E_RELOPEN;
    }

    int retVal = BlockAccess::renameAttribute(relName, oldAttrName, newAttrName);
    return retVal;
}

// createRel(): method to create a new relation with the given name, attributes.
int Schema::createRel(char relName[], int nAttrs, char attrs[][ATTR_SIZE], int attrtype[]){

    /*---- Firstly we will check if the relName already exist or not ----*/
    Attribute relNameAsAttribute;
    strcpy(relNameAsAttribute.sVal, relName);

    RecId targetRelId;
    
    // perform linear search on relation catalog
    RelCacheTable::resetSearchIndex(RELCAT_RELID);
    char  relCatAttrRelname[ATTR_SIZE] = RELCAT_ATTR_RELNAME;
    // char *relCatAttrRelname;
    // strcpy(relCatAttrRelname,RELCAT_ATTR_RELNAME);
    targetRelId = BlockAccess::linearSearch(RELCAT_RELID,relCatAttrRelname,relNameAsAttribute,EQ);
    if(targetRelId.block != -1 && targetRelId.slot != -1){
        return E_RELEXIST;
    }

    // compare every pair of attributes of attrsNames[] to check if there is any dublicate.
    for(int i = 0; i < nAttrs; i++){
        for(int j = i + 1; j < nAttrs; j++){
            if(strcmp(attrs[i], attrs[j]) == 0){
                return E_DUPLICATEATTR;
            }
        }
    }

    /*--- Now a record into relational catalog ---*/
    Attribute relCatRecord[RELCAT_NO_ATTRS];
    strcpy(relCatRecord[RELCAT_REL_NAME_INDEX].sVal, relName);
    relCatRecord[RELCAT_NO_ATTRIBUTES_INDEX].nVal = nAttrs;
    relCatRecord[RELCAT_NO_RECORDS_INDEX].nVal = 0;
    relCatRecord[RELCAT_FIRST_BLOCK_INDEX].nVal = -1;
    relCatRecord[RELCAT_LAST_BLOCK_INDEX].nVal = -1;
    relCatRecord[RELCAT_NO_SLOTS_PER_BLOCK_INDEX].nVal = floor((2016 / (16 * nAttrs + 1)));

    // use insert() to insert the above record in relcation catalog
    int ret = BlockAccess::insert(RELCAT_RELID, relCatRecord);
    if(ret != SUCCESS){
        return ret;
    }

    /*--- Now inserting attributes into attribute catalog ---*/
    // iterate through all the attributes, put it in a attrCatRecord and insert into attribute catalog
    for(int i = 0; i < nAttrs; i++){
        Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
        strcpy(attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal, relName);
        strcpy(attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, attrs[i]);
        attrCatRecord[ATTRCAT_ATTR_TYPE_INDEX].nVal = attrtype[i];
        attrCatRecord[ATTRCAT_PRIMARY_FLAG_INDEX].nVal = -1;
        attrCatRecord[ATTRCAT_ROOT_BLOCK_INDEX].nVal = -1;
        attrCatRecord[ATTRCAT_OFFSET_INDEX].nVal = i;
        int retVal = BlockAccess::insert(ATTRCAT_RELID, attrCatRecord);
        if(retVal != SUCCESS){
            //delete the relation using delRel(relId);
            Schema::deleteRel(relName);
            return E_DISKFULL;
        }
    }

    return SUCCESS;
    
}

// deleteRel()
int Schema::deleteRel(char *relName){
    /*--- if relName == RELCAT or ATTRCAT, then not permitted ---*/
    if(strcmp(relName, RELCAT_RELNAME) == 0 || strcmp(relName, ATTRCAT_RELNAME) == 0){
        return E_NOTPERMITTED;
    }

    // Check if relation is open or not. Deletion can be performed only if relation is closed
    int relId = OpenRelTable::getRelId(relName);
    if( relId >=0 && relId < MAX_OPEN){
        return E_RELOPEN;
    }

    int ret = BlockAccess::deleteRelation(relName);
    return ret;
}
