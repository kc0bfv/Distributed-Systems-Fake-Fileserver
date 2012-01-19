#include <stdio.h>

#include "sockets.h"

int main( int argc, char *argv[] ) {
	serverSocket sSocket, acceptedSock;
	srcSpec src;
	userOpts option;
	int clientWantsConnection=1;

	src.port = 3000;

	if( serverListen( &sSocket, &src ) == -1 ) {
		perror( "ServerListen" );
		return 1;
	}
	
	int goRound = 1;
	while( goRound == 1 ) {
		if( serverAccept( &sSocket, &acceptedSock ) == -1 ) {
			perror( "serverAccept" );
			return 1; //Just die for now, maybe try again, later TODO
		}
		//TODO: thread off at this point
		clientWantsConnection = 1;
		unsigned char data[MAXDATASIZE];
		size_t dataLen;
		while( clientWantsConnection==1 ) {
			if( serverRecvRequest( &acceptedSock, &option, data, sizeof(data), &dataLen ) == -1 ) {
				perror( "serverRecvRequest" );
				return 1; //Probably do something other than die later TODO
			}
			printf( "%i\n", option );

			//Handle the options the user specified
			switch( option ) {
				case OPT_LS: break;
				case OPT_CD: break;
				case OPT_PWD: break;
				case OPT_SEND: break;
				case OPT_RECV: serverSendFile( &acceptedSock, data, dataLen ); break;
				case OPT_QUIT: clientWantsConnection = 0; break;
				default: ;
			}
		}
		if( serverCloseAccepted( &acceptedSock ) == -1 ) {
			perror( "serverCloseAccepted" );
			return 1; //Just die - there was an error while closing...
		}
		//goRound = 0; //Just do one connection for now, then die 
	}

	if( serverStopListen( &sSocket ) == -1 ) {
		perror( "serverStopListen" );
		return 1;
	}

	return 0;
}
