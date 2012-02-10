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

#include "clientFuncs.h"

int clientConnect( clientSocket *cSocket, const destSpec *dest ) {
	struct addrinfo *retAddrs, hints;

	bzero( &hints, sizeof( hints ) );
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	//Return at least one address for whatever hostname and port the user specified
	//This function call allocates RAM which must be deallocated with freeaddrinfo
	if( getaddrinfo( dest->addr, dest->port, &hints, &retAddrs ) != 0 ) {
		return -1; //Error
	}

	//Copy the first address returned into our persistent address storage
	//A better thing to do would be to test each returned address in turn, but whatever
	memcpy( &(cSocket->address), retAddrs->ai_addr, sizeof( cSocket->address ) );

	//Complements getaddrinfo - frees the RAM it allocated
	freeaddrinfo( retAddrs );
	
	//Open the socket
	cSocket->sockRef = socket( AF_INET, SOCK_STREAM, 0 );
	if( cSocket->sockRef == -1 ) {
		return -1; //Error
	}

	//Connect the socket to the remote host
	if( connect( cSocket->sockRef, (struct sockaddr *) &(cSocket->address), sizeof(cSocket->address) ) == -1 )	{
		return -1; //Error
	}

	return 0; //success
}

int clientSendOpt( const clientSocket *cSocket, const userOpts option, const unsigned char *data, const size_t dataLen ) {
	unsigned char output[MESSAGEBUFSIZE];
	size_t outputAmt=0;

	fmtMessage( option, data, dataLen, output, sizeof( output ), &outputAmt );

	if( write( cSocket->sockRef, &output, outputAmt ) != outputAmt ) {
		return -1; //Error
	}

	return 0;
}

int clientGetResp( const clientSocket *cSocket, const userOpts option, char *response, size_t responseLen, const size_t maxResponseLen ) {
	unsigned char message[MESSAGEBUFSIZE];

	ssize_t bytesRead = 0;

	bzero( response, sizeof(response) ); //Fill response with Null chars

	bytesRead = read(cSocket->sockRef, (char *) message, sizeof(message));

	if( checkCRC( message, bytesRead ) != 0 ) {
		printf( "Error CRC checking %i\n", (int) bytesRead );
		return -1; //Error verifying crc - TODO: better errno
	}

	responseLen = bytesRead - HEADERSIZE; //TODO: make sure this = header specified size

	if( responseLen > maxResponseLen ) {
		printf( "response is too large\n" ); //TODO:Do an errno for this...
		return -1;
	}

	memcpy( response, &(message[DATASEGOFF]), responseLen );
	response[responseLen] = '\0'; //Let's make sure it's null terminated
	response[maxResponseLen-1] = '\0'; //Let's make damn sure it's terminated

	return 0;
}

//Disconnect from the remote host
int clientDisconnect( const clientSocket *cSocket ) {
	if( close( cSocket->sockRef ) != 0 ) {
		return -1; //Error - What does that mean on a close?  Probably, program should die
	}

	return 0;
}

int readUserInput( char *buffer, const size_t bufSize ) {
	char formatStr[10];
#ifdef __APPLE__ //Dynamically setup a format string for scanf so we can't mess up string input
	snprintf( formatStr, sizeof(formatStr), "%%%lus", bufSize-1 );
#elif defined __linux__
	snprintf( formatStr, sizeof(formatStr), "%%%us", bufSize-1 );
#endif
	if( scanf( formatStr, buffer ) != 1 ) {
		return -1;
	}
	buffer[bufSize-1] = '\0'; //Just make sure

	return 0;
}

int queryUser( userOpts *option, unsigned char *data, const size_t maxDataLen, size_t *dataLen ) {
	unsigned int sel = 0;
	*option = OPT_NOSELECTION; //Setup a default
	data[0] = '\0'; //Defaults
	*dataLen = 0;


	printf( "\n\nDo what?\n" );
	printf( "1) List files \t\t2) Change directories \t3) Print current directory\n" );
	printf( "4) Print the hostname \t5) Copy a file \t\t6) Make a directory\n" );
	printf( "7) Print a file stat \t8) Quit\n" );
	printf( "? " );
	{
		char buffer[100];
		readUserInput( buffer, sizeof(buffer) );
		if( sscanf( buffer, "%u", &sel ) != 1 ) {
			sel = 0;
		}
	}

	switch( sel ) {
		case 1: *option = OPT_LS; break;
		case 2: *option = OPT_CD; break;
		case 3: *option = OPT_PWD; break;
		case 4: *option = OPT_HN; break;
		case 5: *option = OPT_CP; break;
		case 6: *option = OPT_MKDIR; break;
		case 7: *option = OPT_STAT; break;
		case 8: *option = OPT_QUIT; break;
		default: *option = OPT_NOSELECTION;
	}

	if( (*option >= OPT_FFILENAMEREQ) && (*option <= OPT_LFILENAMEREQ) ) {
		char buffer[MAXFILENAMESIZE];
		size_t len;
		printf( "Enter name: " );
		readUserInput( buffer, sizeof(buffer) );
		len = strnlen( buffer, sizeof(buffer) ) + 1; //+1 makes the length include the null character
		if( len > sizeof(buffer) || len > MAXDATASIZE ) { //If the user entered malformed data, or if the data is too large to fit in the data segment
			return -1; //Invalid user data. - Error TODO: set errno
		}

		strncpy( (char *) data, buffer, maxDataLen ); //Use strncpy instead of memcpy to fill remainder of array with null
		*dataLen = len;
	} else if( *option == OPT_CP ) {
		char buffer1[256], buffer2[256];
		size_t len1, len2;

		printf( "Enter original name: " );
		readUserInput( buffer1, sizeof(buffer1) );
		len1 = strnlen( buffer1, sizeof(buffer1) ) + 1; //+1 makes the length include the null character
		if( len1 > sizeof(buffer1) || len1 > MAXDATASIZE ) { //If the user entered malformed data, or if the data is too large to fit in the data segment
			return -1; //Invalid user data. - Error TODO: set errno
		}

		printf( "Enter destination name: " );
		readUserInput( buffer2, sizeof(buffer2) );
		len2 = strnlen( buffer2, sizeof(buffer2) ) + 1; //+1 makes the length include the null character
		if( len2 > sizeof(buffer2) || len2+len1 > MAXDATASIZE ) { //If the user entered malformed data, or if the data is too large to fit in the data segment
			return -1; //Invalid user data. - Error TODO: set errno
		}

		strncpy( (char *) data, buffer1, maxDataLen ); //Copy initial filename in, null, dest filename, null
		strncpy( (char *) &(data[len1]), buffer2, maxDataLen-len1 ); //Use strncpy instead of memcpy to fill remainder of array with null.  Copy the filename in so it starts after initial filename's terminating null
		*dataLen = len1+len2; //len1 and len2 include the filenames' nulls.  dataLen is sum of both filename lengths including their terminating nulls
	}

	printf( "\n\n\n" );

	return 0;
}
