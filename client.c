#include <stdio.h>
#include <string.h>

#include "clientFuncs.h"

int main( int argc, char *argv[] ) {
	clientSocket cSocket;
	destSpec dest;
	userOpts option = OPT_NOSELECTION;

	//Setup the default parameters
	strncpy( dest.addr, "127.0.0.1", sizeof(dest.addr) );
	dest.addr[9] = '\0';
	strncpy( dest.port, "3000", sizeof(dest.port) );
	dest.port[5] = '\0';

	//Take the default dest port and address 
	if( parseCMD( argc, argv, &dest, NULL, 0 ) != 0 ) {
		return 1; //This should happen when the user specifies -h
	}

	//Try to connect to the server
	if( clientConnect( &cSocket, &dest ) == -1 ) {
		perror( "Connection error" );
		return 1;
	}

	while( option != OPT_QUIT ) {
		unsigned char data[MAXDATASIZE];
		size_t dataLen;
		char response[MAXDATASIZE+1];
		size_t responseLen;

		//Send the current option, if there is one, and the associated parameters
		if( option != OPT_NOSELECTION ) {
			if( clientSendOpt( &cSocket, option, data, dataLen ) == -1 ) {
				perror( "Error while sending request" );
				return 1;
			}
		}

		//If our request required a response, wait for it
		if( option >= OPT_FRESPONSEOPT && option <= OPT_LRESPONSEOPT ) {
			if( clientGetResp( &cSocket, option, response, responseLen, sizeof(response) ) == -1 ){
				perror( "Error while waiting for response" );
				return 1;
			}

			//Output the response
			printf( "%s\n", response );
		}

		//Get the next action from the user
		if( queryUser( &option, data, sizeof(data), &dataLen ) == -1 ) {
				perror( "Error getting user preference" );
				return 1;
		}
	}

	//Let the server know that we're quitting
	clientSendOpt( &cSocket, option, NULL, 0 );

	//Disconnect
	clientDisconnect( &cSocket ); 

	return 0;
}
