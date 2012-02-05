#ifndef __COMMON_H
#define __COMMON_H

#include <sys/types.h> // size_t
#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>

//This data structure makes it easier to pass around sockets
typedef struct mySocketTag {
	int sockRef;
	struct sockaddr_in address;
} mySocket;

//This data structure specifies an IP address and port number in character string
typedef struct ipSpecTag {
	char addr[256]; //I guess this could be pretty long...
	char port[7]; //ports shouldn't be longer than 5 chars...
} ipSpec;

//These are the different options users can choose from
typedef enum userOptsTag {
	//Client Requests - no response
	OPT_NOSELECTION, //=0
	OPT_QUIT, //Quit

	//Client Requests - response required
		//These first 3 require no data parameters
	OPT_LS, //List the current directory on the server
	OPT_PWD, //Print the current directory's location on the server
	OPT_HN, //Get the server's hostname
		//This one requires 2 filenames
	OPT_CP, //Copy a file on the server
		//These 3 require a filename
	OPT_CD, //Change the current directory on the server
	OPT_STAT,
	OPT_MKDIR, //Make a directory on the server

	//Other
	OPT_ERROR,
} userOpts;

//This is instantiated in common.c.  It maps userOpts to human-friendly descriptions.  It's used in logging
char *resolveOpt[OPT_ERROR+1];

//Define the first and last options that require server responses
#define OPT_FRESPONSEOPT OPT_LS
#define OPT_LRESPONSEOPT OPT_MKDIR
//Define the first and last options that require EXACTLY ONE filename parameter
#define OPT_FFILENAMEREQ OPT_CD
#define OPT_LFILENAMEREQ OPT_MKDIR

//This helps convert big/little endian byte ordering
typedef union intToCharUnionTag{
	uint32_t intSpot;
	unsigned char bytes[4];
} intToCharUnion;

//------------------ Utilities
//Take in some parameters stored in data and an option to pass to the server, and return a fully formed message.  The message is prepped for sending
int fmtMessage( const userOpts userOpt, const unsigned char *data, const size_t dataLen, unsigned char *message, const size_t msgBufSize, size_t *finalMsgSize );
//Check a received CRC value for validity
int checkCRC( unsigned char *buffer, const size_t buffersize );
//Prepare an error to be the parameter of a response message
int prepError( int errval, unsigned char *response, const size_t maxResponseSize, size_t *actualResponseSize );
//Parse the command line for standard options
int parseCMD( const int argc, char * const argv[], ipSpec *src, char *rootDir, const size_t rootDirSize );

/*Protocol Description:
Basic message:
1 byte command (command can be ls, mv, whatever, also can be "send file", or "acknowledge" - whatever's needed)
4 bytes integer total message length (network endian order - big endian)
x bytes total message checksum (checksum of total message with the checksum bytes set to NULL) - probably a crc32, which is 4 bytes
remainder bytes data (all messages are character strings right now, endianness isn't a concern yet)

Max data block size: 1024 bytes.  Max message size is then 1029+x bytes (1033 with crc32)
*/
//These defines let the code know the layout of the protocol fields
#define COMMANDOFF 0 //The offset for the command field
#define COMMANDLEN 1 //# of bytes in command field  (the rest are self explanatory)
#define LENGTHOFF 1
#define LENGTHLEN 4
#define CHCKSUMOFF 5
#define CHCKSUMLEN 4
#define DATASEGOFF 9
#define HEADERSIZE DATASEGOFF //The header ends at the data segment start
#define MAXDATASIZE 1024
#define MESSAGEBUFSIZE 1040 //This seems like a good number for now

#define MAXFILENAMESIZE 256 //Good for now

//Old C doesn't have these defined by default, but I like using them sometimes
#define TRUE (1==1)
#define FALSE (1!=1)


#endif
