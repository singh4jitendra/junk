#ifndef __WDACKH__
#define __WDACKH__

#ifdef WESCOM_CLIENT

#define NUMVERSIONSUPPORTED    2

void WDOCGETACKN(FUNCPARMS*);
void TWDOCGETACKN(FUNCPARMS*);
void AckClient060400(int, FUNCPARMS*, int);
void AckClient060503(int, FUNCPARMS*, int);

CLIENTVERSIONCONTROL ackVersionTable[NUMVERSIONSUPPORTED] = {
    { "06.05.03", AckClient060503, 512 },
    { "06.04.00", AckClient060400, 512 }
};

#endif

#endif
