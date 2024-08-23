#ifndef _MAINTC_H_
#define _MAINTC_H_

#if !defined(_SYS_TIME_H_)
#include <sys/time.h>
#endif
#include <sys/types.h>

#if !defined(MAXRECORDS)
#define MAXRECORDS    100
#endif

/* redefinition of the DETAILS section of transaction area */
typedef struct {
    char transaction[TRANSACTIONSIZE];        /* transaction number    */
    char branch[BRANCHLENGTH];                /* branch number         */
    char sku[SKULENGTH];                      /* SKU number            */
    char istsflg[ISTSFLGLENGTH];              /* flag of some sort     */
    char discon_flag;
    char supplier[9];
    char sug_ord_qty[9];
    char opn_ord_qty[9];
    char rem_ord_qty[9];
    char excess_qty[9];
    char qty_on_hand[QTYONHANDLENGTH];        /* quantity on hand      */
    char qty_on_rsv[QTYONRESERVELENGTH];      /* quantity on reserve   */
    char qty_on_bkordr[QTYONBACKORDERLENGTH]; /* quantity on backorder */
    char qty_on_order[QTYONORDERLENGTH];      /* quantity on order     */
    char reord_pt[6];
    char seoq[6];
    char last_cost[8];
    char avg_cost[AVERAGECOSTLENGTH];         /* average cost          */
    char repl_cost[8];
    char mtd_sales[10];
    char ytd_sales[10];
    char uom[UOMLENGTH];                      /* unit of measurement   */
    char velocity_code;
    char buyer_code[2];
    char item_sharable;
    char d_username[USERNAMELENGTH];          /* username              */
    char sods_desc[35];
    char sods_sku[11];
    char dummy[35];                           /* tim bauer             */
} LINEITEM;

/* redefinition of the DETAILS section of transaction area */
typedef struct {
    char transaction[TRANSACTIONSIZE];        /* transaction number    */
    char branch[BRANCHLENGTH];                /* branch number         */
    char sku[SKULENGTH];                      /* SKU number            */
    char qty_on_hand[QTYONHANDLENGTH];        /* quantity on hand      */
    char qty_on_rsv[QTYONRESERVELENGTH];      /* quantity on reserve   */
    char qty_on_bkordr[QTYONBACKORDERLENGTH]; /* quantity on backorder */
    char qty_on_order[QTYONORDERLENGTH];      /* quantity on order     */
    char avg_cost[AVERAGECOSTLENGTH];         /* average cost          */
    char uom[UOMLENGTH];                      /* unit of measurement   */
    char d_username[USERNAMELENGTH];          /* username              */
    char istsflg[ISTSFLGLENGTH];              /* flag of some sort     */
} OLDLINEITEM;

/* maintainence transaction message */
typedef struct {
    char status[STATUSLENGTH];                /* return status         */
    ERRORSTR err;                             /* error information     */
    OLDLINEITEM lineitem[MAXRECORDS];            /* updated information   */
} OLDMNTMESSAGE;

typedef struct {
    char process_type;                         /* P - packet, R - record */
    char status[STATUSLENGTH];
    ERRORSTR err;
    LINEITEM lineitem[50];
} MNTMESSAGE;

/* use unsigned longs for time value */
typedef struct {
   u_int32_t seconds;
   u_int32_t hundreds;
} UTIME;

/* used to create a queue file name */
typedef struct {
    char filename[FILENAMELENGTH];            /* name of file          */
    UTIME filetime;                        /* time file was created */
} USEFILE;

#endif
