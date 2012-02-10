#ifndef __CLIENTFUNCS_H
#define __CLIENTFUNCS_H

#include <sys/types.h> // size_t
#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "common.h"

typedef mySocket clientSocket;

typedef ipSpec destSpec;

//---------------- Client Socket Functions
//Connect to a remote host specified in dest.  Return the socket in cSocket
int clientConnect( clientSocket *cSocket, const destSpec *dest );
//Send the server an option and parameters.  data must contain the fully formatted parameters.  dataLen is the total length of data in data that should be sent.
int clientSendOpt( const clientSocket *cSocket, const userOpts option, const unsigned char *data, const size_t dataLen );
//Wait for a response from the server.  This is blocking.  The response goes in response, and response size goes in responseLen
int clientGetResp( const clientSocket *cSocket, const userOpts option, char *response, size_t responseLen, const size_t maxResponseLen );
//Disconnect from the server
int clientDisconnect( const clientSocket *cSocket );


//-----------------Utilities
//Ask the user for the next function to call on the server.  Also ask the user for any necessary parameters like filenames, fully format the parameters, and drop them in data.
int queryUser( userOpts *option, unsigned char *data, const size_t maxDataLen, size_t *dataLen );
//Ask the user for a string input, stored in buffer, with buffer size bufSize
int readUserInput( char *buffer, const size_t bufSize );

#endif
