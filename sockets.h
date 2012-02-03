#include <sys/types.h> // size_t
#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>

typedef struct mySocketTag {
	int sockRef;
	struct sockaddr_in address;
} clientSocket;
typedef clientSocket serverSocket;

typedef struct ipSpecTag {
	char addr[256]; //I guess this could be pretty long...
	char port[7]; //ports shouldn't be longer than 5 chars...
} ipSpec;

typedef ipSpec destSpec;
typedef ipSpec srcSpec;

/*
typedef struct srcSpecTag {
	uint16_t port;
} srcSpec;*/

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

#define OPT_FRESPONSEOPT OPT_LS
#define OPT_LRESPONSEOPT OPT_MKDIR
#define OPT_FFILENAMEREQ OPT_CD
#define OPT_LFILENAMEREQ OPT_MKDIR

typedef union intToCharUnionTag{
	uint32_t intSpot;
	unsigned char bytes[4];
} intToCharUnion;

int clientConnect( clientSocket *cSocket, const destSpec *dest );
int clientSendOpt( const clientSocket *cSocket, const userOpts option, const unsigned char *data, const size_t dataLen );
int clientGetResp( const clientSocket *cSocket, const userOpts option, char *response, size_t responseLen, const size_t maxResponseLen );
int clientDisconnect( const clientSocket *cSocket );

int serverListen( serverSocket *sSocket, const srcSpec *src	);
int serverAccept( const serverSocket *sSocket, serverSocket *accepted ); //Currently blocking
int serverRecvRequest( const serverSocket *accepted, userOpts *option, unsigned char *data, const size_t maxDataSize, size_t *dataSize );
int serverRespRequest( const serverSocket *accepted, const userOpts option, const unsigned char *data, const size_t dataSize, const char *highestDir );
int serverCloseAccepted( const serverSocket *accepted );
int serverStopListen( const serverSocket *sSocket );

int fmtMessage( const userOpts userOpt, const unsigned char *data, const size_t dataLen, unsigned char *message, const size_t msgBufSize, size_t *finalMsgSize );

int queryUser( userOpts *option, unsigned char *data, const size_t maxDataLen, size_t *dataLen );
int checkCRC( unsigned char *buffer, const size_t buffersize );
int copyInFilename( char *filename, const size_t maxFNameSize, size_t *fnamelen, const unsigned char *data, const size_t dataSize );
int prepError( int errval, unsigned char *response, const size_t maxResponseSize, size_t *actualResponseSize );
int parseCMD( const int argc, char * const argv[], ipSpec *src, char *rootDir, const size_t rootDirSize );

//Used in validating the "don't go above the root" rules
int validateFilename( char *filename, const size_t filenameLen, const char *highestDir );
int countPeriodPairs( char *filename, const size_t filenameLen, unsigned int *periodPairs );
int verifyBstartswithA( const char *A, const char *B );

/*Protocol Being Considered
Basic message:
1 byte command (command can be ls, mv, whatever, also can be "send file", or "acknowledge" - whatever's needed)
4 bytes integer total message length (network endian order - big endian, I think)
x bytes total message checksum (checksum of total message, with the checksum bytes set to NULL) - probably a crc32, which is 4 bytes
remainder bytes data (endianness?)

Max data block size: 1024 bytes.  Max message size is then 1029+x bytes (1033 with crc32)

Command - RECV File - data section contains filename
*/
#define COMMANDOFF 0
#define COMMANDLEN 1
#define LENGTHOFF 1
#define LENGTHLEN 4
#define CHCKSUMOFF 5
#define CHCKSUMLEN 4
#define DATASEGOFF 9
#define HEADERSIZE DATASEGOFF //The header ends at the data segment start
#define MAXDATASIZE 1024
#define MESSAGEBUFSIZE 1040 //This seems like a good number for now

#define MAXFILENAMESIZE 256 //Good for now

#define TRUE (1==1)
#define FALSE (1!=1)
