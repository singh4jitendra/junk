#ifndef __RESERVEH__
#define __RESERVEH__

#ifdef WESCOM_CLIENT

#define NUMVERSIONSUPPORTED    2

void WDOCRESERVE(FUNCPARMS*);
void TWDOCRESERVE(FUNCPARMS*);
void Client060500(int, FUNCPARMS*, int);
void Client060400(int, FUNCPARMS*, int);

CLIENTVERSIONCONTROL versionTable[NUMVERSIONSUPPORTED] = {
    { "06.05.00", Client060500, 512 },
    { "06.04.00", Client060400, 512 }
};

#endif

#endif
