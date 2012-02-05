#ifndef __CRC_H
#define __CRC_H

//Perform a CRC-32 checksum
#include <sys/types.h>
#include <stdint.h>

//I did not write the crc function found in crc.c.  I've followed the requirements of the copyright notice, and my use is clearly permitted by the holder.  I had to make a few mods to it for use in my code, but they're just formatting.
int crc( const unsigned char *buf, const size_t bufSize, uint32_t *cval );

#endif
