#include <sys/types.h> // size_t
#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "common.h"

typedef mySocket serverSocket;

typedef ipSpec srcSpec;

typedef enum procStatusTag {
	STAT_NOTSTARTED,
	STAT_RUNNING,
	STAT_FINISHED
} procStatus;

typedef struct procParamsDef {
	serverSocket *acceptedSock;
	int index;
	char *rootDir;
	procStatus status;
	pid_t pid;
	struct procParamsDef *next; //For linked list operation
} procParams;

//Server socket functions
int serverListen( serverSocket *sSocket, const srcSpec *src	);

int serverAccept( const serverSocket *sSocket, serverSocket *accepted ); //Currently blocking

int serverRecvRequest( const serverSocket *accepted, userOpts *option, unsigned char *data, const size_t maxDataSize, size_t *dataSize );

int serverRespRequest( const serverSocket *accepted, const userOpts option, const unsigned char *data, const size_t dataSize, const char *highestDir );

int serverCloseAccepted( const serverSocket *accepted );

int serverStopListen( const serverSocket *sSocket );


//Utilities
int copyInFilename( char *filename, const size_t maxFNameSize, size_t *fnamelen, const unsigned char *data, const size_t dataSize );

int addProc( procParams **listHeadPtr, procParams *newItem );

int remProc( procParams **listHeadPtr, procParams *newItem );


//Used in validating the "don't go above the root" rules
int validateFilename( char *filename, const size_t filenameLen, const char *highestDir );

int countPeriodPairs( char *filename, const size_t filenameLen, unsigned int *periodPairs );

int verifyBstartswithA( const char *A, const char *B );

