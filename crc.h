//Perform a CRC-32 checksum
#include <sys/types.h>
#include <stdint.h>

int crc( const unsigned char *buf, const size_t bufSize, uint32_t *cval );
