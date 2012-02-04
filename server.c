#include <stdio.h>
#include <unistd.h> //getcwd
#include <stdlib.h> //malloc
#include <string.h>

#include "serverFuncs.h"

int forkedFunc( procParams *params ) {
	serverSocket *acceptedSock = params->acceptedSock;
	userOpts option;
	int clientWantsConnection=1;
	unsigned char data[MAXDATASIZE];
	size_t dataLen;


	while( clientWantsConnection==1 ) {
		if( serverRecvRequest( acceptedSock, &option, data, sizeof(data), &dataLen ) == -1 ) {
			perror( "Error receiving client request" );
			return 1; //Probably do something other than die later TODO
		}
		printf( " Operation: Client %i - Option %s\n", params->index, resolveOpt[option] );

		//Handle the options the user specified
		if( option == OPT_QUIT ) {
			clientWantsConnection = 0;
		} else {
			serverRespRequest( acceptedSock, option, data, dataLen, params->rootDir );
		}
	}
	if( serverCloseAccepted( acceptedSock ) == -1 ) {
		perror( "Error closing client socket" );
		return 1; //Just die - there was an error while closing...
	}	

	return 0;
}

int main( int argc, char **argv ) {
	serverSocket sSocket;
	srcSpec src;

	char rootDir[MAXFILENAMESIZE];
	if( getcwd( rootDir, sizeof(rootDir) ) == NULL ) {
		perror( "Error determining current directory" );
		return -1;
	}
	rootDir[MAXFILENAMESIZE-1] = '\0'; //Always make sure

	strncpy( src.port, "3000", sizeof(src.port) );
	src.port[5] = '\0';

	//Take the default source port and root directory
	if( parseCMD( argc, argv, &src, rootDir, sizeof(rootDir) ) != 0 ) {
		return 1; //This should happen when the user specifies -h
	}


	if( serverListen( &sSocket, &src ) == -1 ) {
		perror( "Error while beginning to listen" );
		return 1;
	}
	
	int goRound = 0;
	procParams *procs = NULL;
	while( 1 == 1 ) {
		//Allocate memory for all the thread parameters
		procParams *params = (procParams *) malloc( sizeof(procParams) );
		params->acceptedSock = (serverSocket *) malloc( sizeof(serverSocket) );
		params->next = NULL;
		params->index = ++goRound;
		params->rootDir = rootDir;

		//Accept a connection
		if( serverAccept( &sSocket, params->acceptedSock ) == -1 ) {
			perror( "Error accepting client connection" );
			return 1; //Just die for now, maybe try again, later TODO
		}

		pid_t pid = 0;
		//Farm the connection off to a new process
		if( (pid = fork()) == 0 ) { //if we're the child
			forkedFunc( params );
			return 0;
		} else {
			params->pid = pid;
			printf( "Connection: PID %i - Client index %i\n", params->pid, params->index );
		}

		//Add the process info to a linked list
		addProc( &procs, params );

		//Peruse the list to clean up finished clients
		procParams *cur = procs;
		for( cur = procs; cur != NULL; cur = cur->next ) {
			if( waitpid( cur->pid, NULL, WNOHANG ) != 0 ) { //If waitpid found a stopped process
				printf( "Disconnect: Client %i\n", cur->index );
				remProc( &procs, cur );
			}
		}
	}

	if( serverStopListen( &sSocket ) == -1 ) {
		perror( "Error finishing listening" );
		return 1;
	}

	return 0;
}
