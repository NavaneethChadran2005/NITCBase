#include "Algebra.h"

#include <cstring>
#include <cstdio>
#include <cstdlib>

bool isNumber(char *str);

/*  used to select all the records that satisfy a condition.
    the arguments of the function are
    - srcRel - the source relation we want to select from
    - targetRel - the relation we want to select into. (ignore for now)
    - attr - the attribute that the condition is checking
    - op - the operator of the condition
    - strVal - the value that we want to compare against (represented as a string)
*/
int Algebra::select(char srcRel[ATTR_SIZE], char targetRel[ATTR_SIZE], char attr[ATTR_SIZE], int op, char strVal[ATTR_SIZE]){
    int srcRelId = OpenRelTable::getRelId(srcRel);
    if(srcRelId == E_RELNOTOPEN){
        return E_RELNOTOPEN;
    }

    AttrCatEntry attrCatEntry;
    // get the attribute catalog entry for attr, using AttrCacheTable::getAttrcatEntry()
    //    return E_ATTRNOTEXIST if it returns the error
    int ret = AttrCacheTable::getAttrCatEntry(srcRelId, attr, &attrCatEntry);
	if (ret == E_ATTRNOTEXIST) {
		return E_ATTRNOTEXIST;
	}
    
    /*** Convert input strVal (string) to an attribute of data type NUMBER or STRING ***/
    int type = attrCatEntry.attrType;
    Attribute attrValue;
    if(type == NUMBER){
        if(isNumber(strVal)){
            attrValue.nVal = atof(strVal);
        }
        else{
            return E_ATTRTYPEMISMATCH;
        }
    }
    else if( type == STRING){
        strcpy(attrValue.sVal, strVal);
    }

    /*** Creating and opening the target relation ***/
    // prepare arguments for createRel() in the following way:
    // get RelcatEntry of srcRel using
    RelCatEntry relCatEntry;
    RelCacheTable::getRelCatEntry(srcRelId, &relCatEntry);
    int src_nAttrs = relCatEntry.numAttrs;

    // store all the attributes names of secRel in 2D array, and attribute type in 1d array.
    char attr_names[src_nAttrs][ATTR_SIZE];
    int attr_type[src_nAttrs];

    for(int i = 0; i < src_nAttrs; i++){
        AttrCatEntry srcAttrCatEntry;
        AttrCacheTable::getAttrCatEntry(srcRelId, i, &srcAttrCatEntry);
        strcpy(attr_names[i],srcAttrCatEntry.attrName);
        attr_type[i] = srcAttrCatEntry.attrType;
    }

    // Create the relation for target relation by calling Schema::createRel()
    int retVal = Schema::createRel(targetRel, src_nAttrs, attr_names, attr_type);

    // little bit of error handling
    if(retVal != SUCCESS){
        Schema::deleteRel(targetRel);
        return retVal;
    }

    int targetRelId = OpenRelTable::openRel(targetRel);
    if(targetRelId < 0) {
        // Schema::deleteRel(targetRel);
        return targetRelId;
    }

    /*** Selecting and inserting records into the target relation ***/
    // Reset searchIndex for attrCacheTable [not required for this stage]

    Attribute record[src_nAttrs];
    RelCacheTable::resetSearchIndex(srcRelId);
    // AttrCacheTable::resetSearchIndex(srcRelId); (in upcoming stages)

    while(BlockAccess::search(srcRelId, record, attr, attrValue, op) == SUCCESS){
        int ret = BlockAccess::insert(targetRelId, record);
        if(ret != SUCCESS){
            Schema::closeRel(targetRel);
            Schema::deleteRel(targetRel);
            return ret;
        }
    }
    Schema::closeRel(targetRel);
    // Schema::deleteRel(targetRel);
    return SUCCESS;
}

/*
    Insert()
    Method to insert the given record into the specific Relation.
*/
int Algebra::insert(char relName[ATTR_SIZE], int nAttrs, char record[][ATTR_SIZE]){
    if(strcmp(relName,RELCAT_RELNAME) == 0 || strcmp(relName,ATTRCAT_RELNAME) == 0){
        return E_NOTPERMITTED;
    }

    // get the relation's relId
    int relId = OpenRelTable::getRelId(relName);

    // if relation is not open, return erro
    if(relId == E_RELNOTOPEN){
        return E_RELNOTOPEN;
    }

    // get the relation catalog entry from relation cache using getRelCatEntry()
    RelCatEntry relCatEntry;
    RelCacheTable::getRelCatEntry(relId, &relCatEntry);

    // if relCatEntry.numAttrs != num of attributes in relation, return error
    if(relCatEntry.numAttrs != nAttrs){
        return E_NATTRMISMATCH;
    }

    Attribute recordValues[nAttrs];
    // converting 2D char array of record to Attribute array recordValues
    for(int i = 0; i < nAttrs; i++){
        // get the attrCatEntry for i-th attribute form the attrCacheTable
        AttrCatEntry attrCatEntry;
        AttrCacheTable::getAttrCatEntry(relId, i, &attrCatEntry);

        int type = attrCatEntry.attrType;
        if(type == NUMBER){
            // if the char array record[i] can be converted into number
            if(isNumber(record[i]) == true){
                // convert it into number and store it into recordValues[i].nVal
                recordValues[i].nVal = atof(record[i]);
            }
            else{
                return E_ATTRTYPEMISMATCH;
            }
        }
        else if(type == STRING){
            strcpy(recordValues[i].sVal, record[i]);
        }
    }

    // insert the record by calling BlockAccess::insert()
    int retVal = BlockAccess::insert(relId, recordValues);
    return retVal;
}

/*
    project(srcRel[ATTR_SIZE], targetRel[ATTR_SIZE]);
*/
int Algebra::project(char srcRel[ATTR_SIZE], char targetRel[ATTR_SIZE]){
    int srcRelId = OpenRelTable::getRelId(srcRel);
    if(srcRelId < 0){
        return srcRelId;
    }

    RelCatEntry srcRelCatEntry;
    RelCacheTable::getRelCatEntry(srcRelId, &srcRelCatEntry);

    int numAttrs = srcRelCatEntry.numAttrs;
    
    char attrNames[numAttrs][ATTR_SIZE];
    int attrTypes[numAttrs];

    // iterate through every attribute of the source relation and fill the arrays attrNames, attrTypes
    for(int i = 0; i < numAttrs; i++){
        AttrCatEntry attrCatEntry;
        AttrCacheTable::getAttrCatEntry(srcRelId, i, &attrCatEntry);
        strcpy(attrNames[i], attrCatEntry.attrName);
        attrTypes[i] = attrCatEntry.attrType;
    }

    /*** Creating and opening the target relation ***/
    // create target relation
    int ret = Schema::createRel(targetRel,numAttrs,attrNames, attrTypes);
    if(ret != SUCCESS){
        return ret;
    }

    // open target relation
    int targetRelId = OpenRelTable::openRel(targetRel);
    if(targetRelId < 0){
        Schema::deleteRel(targetRel);
        return targetRelId;
    }

    /*** Inserting projected records into the target relation */
    RelCacheTable::resetSearchIndex(srcRelId);
    Attribute record[numAttrs];

    while(BlockAccess::project(srcRelId, record) == SUCCESS){
        ret = BlockAccess::insert(targetRelId, record);
        if(ret != SUCCESS){
            Schema::closeRel(targetRel);
            Schema::deleteRel(targetRel);
            return ret;
        }
    }

    Schema::closeRel(targetRel);
    
    return SUCCESS;
}

/*
    project()
*/
int Algebra::project(char srcRel[ATTR_SIZE], char targetRel[ATTR_SIZE],int tar_nAttrs, char tar_Attrs[][ATTR_SIZE]){
    int srcRelId = OpenRelTable::getRelId(srcRel);
    if(srcRelId < 0){
        return srcRelId;
    }

    RelCatEntry srcRelCatEntry;
    RelCacheTable::getRelCatEntry(srcRelId, &srcRelCatEntry);
    int numAttrs = srcRelCatEntry.numAttrs;

    // create attr_offset[tar_nAttrs] of type int
    // create attr_types[tar_nAttrs] of type int

    int attr_offset[tar_nAttrs];
    int attr_types[tar_nAttrs];

    /*** Checking if attributes of target are peresent in the source relation and stores its offset and types ***/
    for(int i = 0; i < tar_nAttrs; i++){
        AttrCatEntry attrCatEntry;
        int ret = AttrCacheTable::getAttrCatEntry(srcRelId, tar_Attrs[i], &attrCatEntry);
        if(ret != SUCCESS){
            return ret;
        }

        attr_offset[i] = attrCatEntry.offset;
        attr_types[i] = attrCatEntry.attrType;
    }

    /*** Creating and opening the target relation ***/
    // create
    int ret = Schema::createRel(targetRel, tar_nAttrs, tar_Attrs, attr_types);
    if(ret != SUCCESS){
        return ret;
    }

    // open
    int targetRelId = OpenRelTable::openRel(targetRel);
    if(targetRelId < 0){
        Schema::deleteRel(targetRel);
        return targetRelId;
    }

    /*** Inserting projected records into the target relation ***/
    RelCacheTable::resetSearchIndex(srcRelId);
    Attribute record[numAttrs];
    
    while(BlockAccess::project(srcRelId, record) == SUCCESS){
        Attribute proj_record[tar_nAttrs];

        for(int i = 0; i < tar_nAttrs; i++){
            proj_record[i] = record[attr_offset[i]];
        }

        // insert the record
        ret = BlockAccess::insert(targetRelId, proj_record);
        if(ret != SUCCESS){
            Schema::closeRel(targetRel);
            Schema::deleteRel(targetRel);
            return ret;
        }
    }

    // close target relation
    Schema::closeRel(targetRel);

    return SUCCESS;

}


// will return if a string can be parsed as a floating point number
bool isNumber(char *str){
    int len;
    float ignore;
    /*
    sscanf returns the number of elements read, so if there is no float matching
    the first %f, ret will be 0, else it'll be 1

    %n gets the number of characters read. this scanf sequence will read the
    first float ignoring all the whitespace before and after. and the number of
    characters read that far will be stored in len. if len == strlen(str), then
    the string only contains a float with/without whitespace. else, there's other
    characters.
    */
    int ret = sscanf(str, "%f %n", &ignore, &len);
    return ret == 1 && len == strlen(str);
}
