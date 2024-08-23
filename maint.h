#ifndef __MAINTH__
#define __MAINTH__

#include "common.h"

typedef struct {
    char status[STATUSLENGTH];
    ERRORSTR error;
    char transaction[TRANSACTIONSIZE];
    char branch[BRANCHLENGTH];
    char sku[SKULENGTH];
    char qty_on_hand[QTYONHANDLENGTH];
    char qty_on_rsv[QTYONRESERVELENGTH];
    char qty_on_bkordr[QTYONBACKORDERLENGTH];
    char qty_on_order[QTYONORDERLENGTH];
    char avg_cost[AVERAGECOSTLENGTH];
    char uom[UOMLENGTH];
    char username[USERNAMELENGTH];
    char d_username[USERNAMELENGTH];
    char istsflg[ISTSFLGLENGTH];
} COBOLPARMS;

typedef COBOLPARMS MAINTBUFFER;

typedef struct {
    char branch[BRANCHLENGTH];
    char sku[SKULENGTH];
    char istsflg[ISTSFLGLENGTH];
    char discon_flag;
    char supplier[9];
    char sug_ord_qty[9];
    char opn_ord_qty[9];
    char rem_ord_qty[9];
    char excess_qty[9];
    char qty_on_hand[QTYONHANDLENGTH];
    char qty_on_rsv[QTYONRESERVELENGTH];
    char qty_on_bkordr[QTYONBACKORDERLENGTH];
    char qty_on_order[QTYONORDERLENGTH];
    char reord_pt[6];
    char seoq[6];
    char last_cost[8];
    char avg_cost[AVERAGECOSTLENGTH];
    char repl_cost[8];
    char mtd_sales[10];
    char ytd_sales[10];
    char uom[UOMLENGTH];
    char velocity_code;
    char buyer_code[2];
    char item_sharable;
    char d_username[USERNAMELENGTH];
    char sods_desc[35];
    char sods_sku[11];
    char dummy[35];
} DETAILS;

#ifdef WESCOM_CLIENT

#define NUMVERSIONSUPPORTED    2

VERSIONCONTROL versionTable[NUMVERSIONSUPPORTED] = {
  { "06.05.00", Client060500, 0, 512 },
  { "06.04.00", Client060400, 0, 512 }
};

#endif

#endif
