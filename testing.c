#include <stdio.h>
#include "packets/packet_types.h"

#ifdef BATTLESHIPS_VERSION
char* APP_VERSION = BATTLESHIPS_VERSION;
#else
char* APP_VERSION = "UNDEFINED!";
#endif

int main()
{
    printf("App Version: %s\n", APP_VERSION);
    printf("Generic packet size: %ld\n", sizeof(struct GenericPacket));
    printf("Char* size: %ld\n", sizeof(char*));
    printf("Char size: %ld\n", sizeof(char));
}