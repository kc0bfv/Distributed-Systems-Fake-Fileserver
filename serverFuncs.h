#ifndef __SERVERFUNCS_H
#define __SERVERFUNCS_H

#include <sys/types.h> // size_t
#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "common.h"

#ifndef NAME_MAX //Linux defines this value, OS X does not
	#define NAME_MAX 256 
#endif

typedef mySocket serverSocket;

typedef ipSpec srcSpec;

//This data structure maintains information for spawned client processes
typedef struct procParamsDef {
	serverSocket *acceptedSock;
	int index;
	char *rootDir;
	pid_t pid;
	struct procParamsDef *next; //For linked list operation
} procParams;

//----------------- Server socket functions
//Listen for an incoming connection on the port specified in src.  Return the connection in sSocket
int serverListen( serverSocket *sSocket, const srcSpec *src	);
//Accept an incoming connection.  Return the accepted connection in accepted.  This is blocking
int serverAccept( const serverSocket *sSocket, serverSocket *accepted );
//Wait for a request from a connected client.  Verify the request CRC.  Return the option in option, and the parameters (unparsed) in data
int serverRecvRequest( const serverSocket *accepted, userOpts *option, unsigned char *data, const size_t maxDataSize, size_t *dataSize );
//Take the action requested by the user, and respond if necessary.
int serverRespRequest( const serverSocket *accepted, const userOpts option, const unsigned char *data, const size_t dataSize, const char *highestDir );
//Close a client connection
int serverCloseAccepted( const serverSocket *accepted );
//Stop listening for new connections
int serverStopListen( const serverSocket *sSocket );


//------------------ Utilities
//Copy one filename out of the parameters from a user request.  Store the filename in filename
int copyInFilename( char *filename, const size_t maxFNameSize, size_t *fnamelen, const unsigned char *data, const size_t dataSize );
//Add a process to our linked list of all current client processes
int addProc( procParams **listHeadPtr, procParams *newItem );
//Remove a process from that linked list.  This function also frees the associated memory and clears the zombie process
int remProc( procParams **listHeadPtr, procParams *newItem );


//------------------ Used in validating the "don't go above the root" rules
//Make sure any filename we receive doesn't ask us to violate the "don't go above root" rules
int validateFilename( char *filename, const size_t filenameLen, const char *highestDir );
//Count the number of pairs of periods in a filename.
int countPeriodPairs( char *filename, const size_t filenameLen, unsigned int *periodPairs );
//Verify that string B begins with all the characters (in order) of string A.  For example: "foobar" starts with "f", "fo", ... "foobar".  "foobar" does not start with "asdfjkl", " foobar", "foobara", etc.
int verifyBstartswithA( const char *A, const char *B );


#endif
