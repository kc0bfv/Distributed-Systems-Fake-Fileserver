#include <sys/types.h> // size_t
#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "common.h"

typedef mySocket clientSocket;

typedef ipSpec destSpec;

//Client Socket Functions
int clientConnect( clientSocket *cSocket, const destSpec *dest );

int clientSendOpt( const clientSocket *cSocket, const userOpts option, const unsigned char *data, const size_t dataLen );

int clientGetResp( const clientSocket *cSocket, const userOpts option, char *response, size_t responseLen, const size_t maxResponseLen );

int clientDisconnect( const clientSocket *cSocket );


//Utilities
int queryUser( userOpts *option, unsigned char *data, const size_t maxDataLen, size_t *dataLen );
