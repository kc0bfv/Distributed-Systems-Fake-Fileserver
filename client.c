#include <stdio.h>

#include "sockets.h"

int main( int argc, char *argv[] ) {
	clientSocket cSocket;
	destSpec dest;
	userOpts option = OPT_NOSELECTION;

	unsigned char data[MAXDATASIZE];
	size_t dataLen;

	//processInputParams( argc, argv, dest );

	dest.addr = "127.0.0.1";
	dest.port = "3000";

//	printf( "Here\n" );

	if( clientConnect( &cSocket, &dest ) == -1 ) {
		perror( "clientConnect" );
		return 1;
	}

//	printf( "Here2\n" );

	while( option != OPT_QUIT ) {
		//Do stuff
		if( option != OPT_NOSELECTION ) {
			if( clientSendOpt( &cSocket, option, data, dataLen ) == -1 ) {
				perror( "clientSendOpt" );
				return 1;
			}
		}

		if( option == OPT_RECV ) {
			if( clientWaitForFile( &cSocket, data, dataLen ) == -1 ) { //Wait for the server's response, store it in the filename still specified in data
				perror( "ClientWaitForFile Error" );
			}
		}

		if( queryUser( &option, data, MAXDATASIZE, &dataLen ) == -1 ) {
				perror( "queryUser" );
				return 1;
		}
	}

//	printf( "Here3\n" );

	clientSendOpt( &cSocket, option, NULL, 0 ); //Let the server know
	clientDisconnect( &cSocket );

	return 0;
}
