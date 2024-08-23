#ifndef __RESERVEH__
#define __RESERVEH__

/*
 ******************************************************************************
 * Change History:
 * 09/29/06 mjb 06142 - Added new version for bonded inventory.
 ******************************************************************************
*/
#ifdef WESCOM_CLIENT

#define NUMVERSIONSUPPORTED    3

void WDOCRESERVE(FUNCPARMS*);
void TWDOCRESERVE(FUNCPARMS*);
void Client060500(int, FUNCPARMS*, int);
void Client060400(int, FUNCPARMS*, int);

CLIENTVERSIONCONTROL versionTable[NUMVERSIONSUPPORTED] = {
    { "10.03.00", Client060500, 512 },
    { "06.05.00", Client060500, 512 },
    { "06.04.00", Client060400, 512 }
};

#endif

#endif
