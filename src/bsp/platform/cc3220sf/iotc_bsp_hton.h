
#ifndef __XI_BSP_HTON__
#include <simplelink.h>
#include <sys/types.h>

#ifndef htons
#define htons     sl_Htons
#endif

#ifndef htonl
#define htonl     sl_Htonl
#endif

#ifndef ntohs
#define ntohs     sl_Ntohs
#endif

#ifndef ntohl
#define ntohl     sl_Ntohl
#endif

#endif
