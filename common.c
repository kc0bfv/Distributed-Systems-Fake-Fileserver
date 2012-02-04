#include <netdb.h> //getaddrinfo
#include <string.h> //memcpy
#include <unistd.h> //close, getcwd
#include <stdlib.h> //malloc
#include <dirent.h> //opendir, readdir, closedir
#include <sys/stat.h> //stat, mkdir
#include <time.h> //ctime_r
#include <fcntl.h>
#include <errno.h>

#include <assert.h>

#include <stdio.h>

#include "common.h"
#include "crc.h"

//Build an array which resolves userOpts to human strings
char *resolveOpt[] = {"No Selection","Quit","ls","pwd","hostname","cp","cd","stat","mkdir","error"};

int fmtMessage( const userOpts userOpt, const unsigned char *data, const size_t dataLen, unsigned char *message, const size_t msgBufSize, size_t *finalMsgSize ) {
	*finalMsgSize = HEADERSIZE+dataLen; //The final size must be 1(cmd byte)+msglen)+4(crc32)+dataLen
	if( (msgBufSize < *finalMsgSize) || (dataLen > MAXDATASIZE) || (dataLen < 0) ) { //Check for invalid buffer sizes
		return -1; //Error
	}

	bzero( message, msgBufSize ); //Zero out the output buffer - checksum area must be 0 to calculate checksum
	if( dataLen > 0 ) { //If dataLen == 0, there's no data to copy
		memcpy( &(message[HEADERSIZE]), data, dataLen );
	}
	message[0] = userOpt; //Truncate the userOption down to one byte and put it in there

	{ //Do a conversion from little endian to big, and drop the size into the message
		intToCharUnion conv;
		conv.intSpot = *finalMsgSize;

		//Reverse the order and store it in the message
		int i = 0;
		for( i = 0; i < 4; i++ ) {
			message[4-i] = conv.bytes[i];
		}
	}

	{
		intToCharUnion conv;
		crc( message, *finalMsgSize, &conv.intSpot );

		memcpy( &(message[5]), conv.bytes, 4 ); //Copy the 4 bytes of a crc32
	}

	return 0;
}

int checkCRC( unsigned char *buffer, const size_t buffersize ) { //Check the CRC
	intToCharUnion conv;
	uint32_t crcRes;

	memcpy( conv.bytes, &(buffer[CHCKSUMOFF]), CHCKSUMLEN ); //copy the crc32 bytes
	bzero( &(buffer[CHCKSUMOFF]), CHCKSUMLEN ); //Zero out the checksum in the buffer

	crc( buffer, buffersize, &crcRes );
	if( crcRes != conv.intSpot ) {
		//errno=0; //Probably make this more useful TODO
		errno=2;
		return -1; //CRC Error
	}

	return 0;
}

int prepError( int errval, unsigned char *response, const size_t maxResponseSize, size_t *actualResponseSize ) {
	char *errorSpot, failMsg[]="Failure! ";

	//Write out failure, then store in errorSpot the location we want to write the error to
	errorSpot = stpncpy( (char *) response, failMsg, maxResponseSize );
	strerror_r( errval, errorSpot, maxResponseSize - sizeof(failMsg) );

	response[maxResponseSize - 1] = '\0'; //Make sure...
	*actualResponseSize = strnlen( (char *) response, maxResponseSize );

	return 0;
}

int parseCMD( const int argc, char * const argv[], ipSpec *src, char *rootDir, const size_t rootDirSize ) {
	int retval;
	size_t len;
	char temp[MAXFILENAMESIZE];

	//Accept -h, -p port, -s serveraddress, -r rootdirectory
	while( (retval = getopt( argc, argv, "hp:s:r:" )) != EOF ) { 
		switch( retval ) {
			case 'h':
				printf( "Usage: %s [-h] [-p portnum] [-s serveraddress] [-r rootdirectory]\n", argv[0] );
				printf( "-s has no effect on the server right now.\n-r has no effect on the client right now.\n" );
				return -1;
				break;
			case 'p':
				len = strnlen( optarg, sizeof(src->port) );
				if( len < sizeof(src->port) ) {
					strncpy( src->port, optarg, sizeof(src->port) );
					src->port[sizeof(src->port)] = '\0';
				}
				break;
			case 's':
				len = strnlen( optarg, sizeof(src->addr) );
				if( len < sizeof(src->addr) ) {
					strncpy( src->addr, optarg, sizeof(src->addr) );
					src->addr[sizeof(src->addr)] = '\0';
				}
				break;
			case 'r':
				len = strnlen( optarg, rootDirSize );
				if( len < rootDirSize ) { //if the argument length is a reasonable size
					if( chdir( optarg ) == 0 ) { //and if we can actually change to the requested directory
						if( getcwd( temp, sizeof(temp) ) != NULL ) { //and if we can get the requested directory's name
							strncpy( rootDir, temp, rootDirSize ); //then copy the full path of the requested direc in
							rootDir[rootDirSize] = '\0'; //make sure it's null terminated
						}
					}
				}
				break;
			default: break;
		}
	}
	return 0;
}
