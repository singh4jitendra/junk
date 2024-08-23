#ifndef __UPDATEH__
#define __UPDATEH__

/*
 ******************************************************************************
 * Change History:
 * 09/29/06 mjb 06142 - Added new version for bonded inventory.
 ******************************************************************************
*/

#include "common.h"

typedef struct {
    char branch[BRANCHLENGTH];
    char sku[SKULENGTH];
    char filename[FILENAMELENGTH];
    char cust[CUSTOMERLENGTH];
} CDINVR11DETAIL;

typedef struct {
    char sku[SKULENGTH];
    char quantity[QUANTITYLENGTH];
    char bo[BOLENGTH];
} CCINVC11DETAIL;

typedef struct {
    char branch[BRANCHLENGTH];
    char sku[SKULENGTH];
    char mode[MODELENGTH];
    char filename[FILENAMELENGTH];
} CCACKR11DETAIL;

typedef struct {
    char transaction[TRANSACTIONSIZE];
    char username[USERNAMELENGTH];
    char branch[BRANCHLENGTH];
    char sku[SKULENGTH];
} CDINVR11BUFFER;

typedef struct {
    char transaction[TRANSACTIONSIZE];
    char username[USERNAMELENGTH];
    char sku[SKULENGTH];
    char quantity[QUANTITYLENGTH];
    char bo[BOLENGTH];
    char rsvtype[2];
    char biqty[7];
    char cust[CUSTOMERLENGTH];
    char branch[BRANCHLENGTH];
} CCINVC11BUFFER;

typedef struct {
    char transaction[TRANSACTIONSIZE];
    char username[USERNAMELENGTH];
    char partition[PARTITIONLENGTH];
    char branch[BRANCHLENGTH];
    char sku[SKULENGTH];
    char mode[MODELENGTH];
} CCACKR11BUFFER;

#endif
