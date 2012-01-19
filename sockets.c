#include <netdb.h> //getaddrinfo
#include <string.h> //memcpy
#include <unistd.h> //close
#include <stdlib.h> //malloc
#include <fcntl.h>
#include <errno.h>

#include <assert.h>

#include <stdio.h>

#include "sockets.h"
#include "crc.h"

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
//	printf( "%d %d %d\n", AF_INET, SOCK_STREAM, 6 );
//	printf( "%d %d %d\n", retAddrs->ai_family, retAddrs->ai_socktype, retAddrs->ai_protocol );
//	printf( "%d %d \n", retAddrs->ai_addr->sin_family, retAddrs->ai_addr->sin_port );
//	printf( "%d %d \n", cSocket->address.sin_family, cSocket->address.sin_port );

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

//	printf( "A\n" );

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

int clientWaitForFile( const clientSocket *cSocket, const unsigned char *filename, const size_t filenameLen ) {
	int writeFile = 0;
	ssize_t bytesRead = 0;
	unsigned int keepGoing=1;
	unsigned char message[MESSAGEBUFSIZE];

	printf( "Starting wait\n" );

	writeFile = open( (char *) filename, O_WRONLY|O_CREAT|O_EXCL ); //TODO:Make sure all exit points close the file
	if( writeFile < 0 ) {
		printf( "Error opening %s\n", (char *) filename );
		return -1; //Error opening the file
	}

	while( keepGoing == 1 ) {
		bytesRead = read(cSocket->sockRef, message, sizeof(message));
		printf( "Got a Read\n" );

		if( checkCRC( message, bytesRead ) != 0 ) {
			printf( "Error CRC checking %i\n", (int) bytesRead );
			close( writeFile );
			return -1; //Error verifying crc - TODO: better errno
		}

		if( message[COMMANDOFF] == OPT_EOF ) { //If we're at the end of the file
			keepGoing = 0;
		} else if( message[COMMANDOFF] == OPT_SENDING ) {
			if( write( writeFile, & (message[DATASEGOFF]), bytesRead-HEADERSIZE ) != bytesRead-HEADERSIZE ) {
				printf( "Error writing\n" );
				close( writeFile );
				return -1; //Error writing to file - TODO: better errno
			}
		} else {
			printf( "Weird COMMAND\n" );
			close( writeFile );
			return -1; //Error receiving file - TODO: set errno
		}
	}

	printf( "Closing writefile\n" );

	close( writeFile );

	return 0;
}

//Disconnect from the remote host
int clientDisconnect( const clientSocket *cSocket ) {
	if( close( cSocket->sockRef ) != 0 ) {
		return -1; //Error - What does that mean on a close?  Probably, program should die
	}

	return 0;
}


int serverListen( serverSocket *sSocket, const srcSpec *src ) {
	bzero( &(sSocket->address), sizeof( sSocket->address ) );
	sSocket->address.sin_family = AF_INET;
	sSocket->address.sin_port = htons( src->port ); //TODO: fix this up later
	sSocket->address.sin_addr.s_addr = htonl( INADDR_LOOPBACK );

	sSocket->sockRef = socket( AF_INET, SOCK_STREAM, 0 );
	if( sSocket->sockRef == -1 ) {
		return -1; //Error
	}

	if( bind( sSocket->sockRef, (struct sockaddr *) &(sSocket->address), sizeof(sSocket->address) ) == -1 )
	{
		return -1; //Error
	}

	if( listen( sSocket->sockRef, 5 ) == -1 ) //Backlog of 5.  Reasonable?
	{
		return -1; //Error
	}

	return 0;
}

int serverAccept( const serverSocket *sSocket, serverSocket *accepted ) {
	socklen_t addrLen = sizeof( accepted->address);

	accepted->sockRef = accept( sSocket->sockRef, (struct sockaddr *) &(accepted->address), &addrLen );

	if( accepted->sockRef == -1 ) {
		return -1; //Error
	}

	return 0;
}

int serverRecvRequest( const serverSocket *accepted, userOpts *option, unsigned char *data, const size_t maxDataSize, size_t *dataSize ) {
	unsigned char buffer[MESSAGEBUFSIZE];
	int valsRead = 0;

	valsRead = read( accepted->sockRef, buffer, sizeof(buffer) );
	if( valsRead == 0 ) { //End of file
		//TODO: what here?
		return 0;
	} else if( valsRead < HEADERSIZE ) { //Probably have to make this more accurate later?
		errno=1; //TODO: better errno
		return -1; //Error
	}

	if( checkCRC( buffer, valsRead ) != 0 ) {
		return -1; // CRC error, TODO: better errno
	}

	*option = (userOpts) buffer[COMMANDOFF];

	//TODO: This should probably make sure that valsRead is equivalent to the size specified in the header
	*dataSize = valsRead - HEADERSIZE;
	memcpy( data, &(buffer[DATASEGOFF]), valsRead ); //copy the data segment into the data buffer
	
	//TODO: Handle the different options somewhere?
	//TODO: Check message size vs what's professed in message
	//TODO: Check sent option against list of possibles
	return 0;
}

int serverRecvData( const serverSocket *accepted, char *buffer, const size_t bufSize, ssize_t valsRead ) {
	valsRead = read( accepted->sockRef, buffer, bufSize );

	if( valsRead == -1 ) {
		return -1;
	}

	return 0;
}

int serverSendFile( const serverSocket *accepted, const unsigned char *filename, const size_t filenameSize ){
	//TODO: What if the data segment is too big for a filename?

//	size_t filenameSize = bufSize-HEADERSIZE;
//	char *filename = malloc( filenameSize+1 ); //buffer size - header + null character
//	//TODO: make sure filename is freed on every possible return - this is why C sucks

	int readFile = 0;
	unsigned int blockCounter = 0;
	ssize_t bytesRead = 0;
	unsigned char fileBuf[MAXDATASIZE]; //Max data segment size, so we'll send data in 1024 byte blocks

	size_t mesgSize = 0;
	unsigned char message[MESSAGEBUFSIZE];

	printf( "Starting send\n" );

	//memcpy( filename, &(buffer[DATASEGOFF]), filenameSize ); //copy in the filename from the data seg
	//filename[filenameSize] = '\0'; //Null terminate the string

	/*TODO: if( checkFilename( filename, filenameSize ) == -1 ) {
		//Return or something
		}
		*/

	readFile = open( (char *) filename, O_RDONLY );
	if( readFile < 0 ) {
		//TODO: Probably return an error message to the client
		//free( filename );
		return -1; //Error opening the file
	}

	printf( "Opened file\n" );
	
	blockCounter = 0;
	while( ( bytesRead = read(readFile, fileBuf, sizeof(fileBuf)) ) > 0 ) { //Until we hit EOF...
		blockCounter++;
		fmtMessage( OPT_SENDING, fileBuf, bytesRead, message, sizeof(message), &mesgSize );
		if( write( accepted->sockRef, &message, mesgSize ) != mesgSize ) {
			//TODO: handle ERROR
			perror( "Error Writing File Out" );
		}
		//TODO: Wait for client to send OK response for specific block - do I really care to do this, or just trust TCP?
		//Probably need to break out Recv Request part to do this, also, refactor RecvRequest into handle request
		printf( "Sent block\n" );
	}

	//Send an EOF
	fmtMessage( OPT_EOF, NULL, 0, message, sizeof(message), &mesgSize );
	if( write( accepted->sockRef, &message, mesgSize ) != mesgSize ) {
		//TODO: handle ERROR
		perror( "Error sending EOF" );
	}

	printf( "Done sending\n" );

	close( readFile );
	//free( filename );
	return 0; 
}

int serverCloseAccepted( const serverSocket *accepted ) {
	return serverStopListen( accepted ); //They do the same thing right now...
}

int serverStopListen( const serverSocket *sSocket ) {
	if( close( sSocket->sockRef ) != 0 ) {
		return -1; //Error - once again, what does an error mean on close?  Program should probably die
	}

	return 0;
}

int fmtMessage( const userOpts userOpt, const unsigned char *data, const size_t dataLen, unsigned char *message, const size_t msgBufSize, size_t *finalMsgSize ) {
	*finalMsgSize = HEADERSIZE+dataLen; //The final size must be 1(cmd byte)+msglen)+4(crc32)+dataLen
	if( (msgBufSize < *finalMsgSize) || (dataLen > MAXDATASIZE) || (dataLen < 0) ) { //Check for invalid buffer sizes
		return -1; //Error
	}

	bzero( message, msgBufSize ); //Zero out the output buffer - checksum area must be 0 to calculate checksum
	if( dataLen > 0 ) { //If dataLen == 0, there's no data to copy
		memcpy( &(message[HEADERSIZE]), data, dataLen );
	}
	message[0] = userOpt; //Truncate the userOption down to one byte and put it in there

	{ //Do a conversion from little endian to big, and drop the size into the message
		intToCharUnion conv;
		conv.intSpot = *finalMsgSize;

		//Reverse the order and store it in the message
		int i = 0;
		for( i = 0; i < 4; i++ ) {
			message[4-i] = conv.bytes[i];
		}
	}

	{
		intToCharUnion conv;
		crc( message, *finalMsgSize, &conv.intSpot );

		memcpy( &(message[5]), conv.bytes, 4 ); //Copy the 4 bytes of a crc32
	}

	return 0;
}


int queryUser( userOpts *option, unsigned char *data, const size_t maxDataLen, size_t *dataLen ) {
	unsigned int sel = 0;
	*option = OPT_NOSELECTION; //Setup a default
	data[0] = '\0'; //Defaults
	*dataLen = 0;


	printf( "\n\nDo what?\n" );
	printf( "1) List files\n" );
	printf( "2) Change directories\n" );
	printf( "3) Print current directory\n" );
	printf( "4) Receive a file\n" );
	printf( "5) Send a file\n" );
	printf( "6) Quit\n" );
	printf( "7) Default\n" );
	printf( "? " );
	scanf( "%u", &sel );

	switch( sel ) {
		case 1: *option = OPT_LS; break;
		case 2: *option = OPT_CD; break;
		case 3: *option = OPT_PWD; break;
		case 4: *option = OPT_RECV; break;
		case 5: *option = OPT_SEND; break;
		case 6: *option = OPT_QUIT; break;
		default: *option = OPT_NOSELECTION;
	}

	if( *option == OPT_RECV ) {
		char buffer[1024];
		size_t len;
		printf( "Enter filename: " );
		scanf( "%1023s", buffer );
		buffer[1023] = '\0';
		len = strnlen( buffer, 1024 ) + 1; //+1 makes the length include the null character
		if( len > 1024 && len < MAXDATASIZE ) { //If the user entered malformed data, or if the data is too large to fit in the data segment
			return -1; //Invalid user data. - Error TODO: set errno
		}

		strncpy( (char *) data, buffer, maxDataLen ); //Use strncpy instead of memcpy to fill remainder of array with null
		*dataLen = len;
	}

	return 0;
}

int checkCRC( unsigned char *buffer, const size_t buffersize ) { //Check the CRC
	intToCharUnion conv;
	uint32_t crcRes;

	memcpy( conv.bytes, &(buffer[CHCKSUMOFF]), CHCKSUMLEN ); //copy the crc32 bytes
	bzero( &(buffer[CHCKSUMOFF]), CHCKSUMLEN ); //Zero out the checksum in the buffer

	crc( buffer, buffersize, &crcRes );
	if( crcRes != conv.intSpot ) {
		//errno=0; //Probably make this more useful TODO
		errno=2;
		return -1; //CRC Error
	}

	return 0;
}
