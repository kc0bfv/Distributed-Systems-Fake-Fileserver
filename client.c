#include <stdio.h>

#include "sockets.h"

int main( int argc, char *argv[] ) {
	clientSocket cSocket;
	destSpec dest;
	userOpts option = OPT_NOSELECTION;

	unsigned char data[MAXDATASIZE];
	size_t dataLen;

	char response[MAXDATASIZE+1];
	size_t responseLen;

	//processInputParams( argc, argv, dest );

	dest.addr = "127.0.0.1";
	dest.port = "3000";

	if( clientConnect( &cSocket, &dest ) == -1 ) {
		perror( "clientConnect" );
		return 1;
	}

	while( option != OPT_QUIT ) {
		//Do stuff
		if( option != OPT_NOSELECTION ) {
			if( clientSendOpt( &cSocket, option, data, dataLen ) == -1 ) {
				perror( "clientSendOpt" );
				return 1;
			}
		}

		if( option >= OPT_FRESPONSEOPT && option <= OPT_LRESPONSEOPT ) {
			if( clientGetResp( &cSocket, option, response, responseLen, sizeof(response) ) == -1 ){
				perror( "clientGetResp" );
				return 1;
			}

			printf( "%s\n", response );
		}

		if( queryUser( &option, data, sizeof(data), &dataLen ) == -1 ) {
				perror( "queryUser" );
				return 1;
		}
	}

	clientSendOpt( &cSocket, option, NULL, 0 ); //Let the server know that we're quitting

	clientDisconnect( &cSocket );

	return 0;
}
