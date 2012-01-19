#include <sys/types.h> // size_t
#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>

typedef struct mySocketTag {
	int sockRef;
	struct sockaddr_in address;
} clientSocket;
typedef clientSocket serverSocket;

typedef struct destSpecTag {
	char *addr;
	char *port;
} destSpec;

typedef struct srcSpecTag {
	uint16_t port;
} srcSpec;

typedef enum userOptsTag {
	//Client Options
	OPT_NOSELECTION, //=0
	OPT_LS, //List the current directory on the server
	OPT_CD, //Change the current directory on the server
	OPT_PWD, //Print the current directory's location on the server
	OPT_SEND, //Send the server a file
	OPT_RECV, //Get a file from the server
	OPT_QUIT, //Quit
	OPT_ERROR,

	//Client Messages
	OPT_OK, //Generic ok message

	//Server Messages
	OPT_SENDING, //Message contains a file block
	OPT_EOF //File is done
} userOpts;

typedef union intToCharUnionTag{
	uint32_t intSpot;
	unsigned char bytes[4];
} intToCharUnion;

int clientConnect( clientSocket *cSocket, const destSpec *dest );
int clientSendOpt( const clientSocket *cSocket, const userOpts option, const unsigned char *data, const size_t dataLen );
int clientWaitForFile( const clientSocket *cSocket, const unsigned char *filename, const size_t filenameLen );
int clientDisconnect( const clientSocket *cSocket );

int serverListen( serverSocket *sSocket, const srcSpec *src	);
int serverAccept( const serverSocket *sSocket, serverSocket *accepted ); //Currently blocking
int serverRecvRequest( const serverSocket *accepted, userOpts *option, unsigned char *data, const size_t maxDataSize, size_t *dataSize );
int serverRecvData( const serverSocket *accepted, char *buffer, const size_t bufSize, ssize_t valsRead );
int serverSendFile( const serverSocket *accepted, const unsigned char *filename, const size_t filenameSize );
int serverCloseAccepted( const serverSocket *accepted );
int serverStopListen( const serverSocket *sSocket );

int fmtMessage( const userOpts userOpt, const unsigned char *data, const size_t dataLen, unsigned char *message, const size_t msgBufSize, size_t *finalMsgSize );

int queryUser( userOpts *option, unsigned char *data, const size_t maxDataLen, size_t *dataLen );
int checkCRC( unsigned char *buffer, const size_t buffersize );

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
