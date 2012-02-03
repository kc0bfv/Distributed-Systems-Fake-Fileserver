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
	response[maxResponseLen] = '\0'; //Let's make damn sure it's terminated

	return 0;
}

/*
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
}*/

//Disconnect from the remote host
int clientDisconnect( const clientSocket *cSocket ) {
	if( close( cSocket->sockRef ) != 0 ) {
		return -1; //Error - What does that mean on a close?  Probably, program should die
	}

	return 0;
}


int serverListen( serverSocket *sSocket, const srcSpec *src ) {
	uint16_t srcport;

	if( sscanf( src->port, "%hu", &srcport ) != 1 ) { //If for some reason, we didn't get a srcport
		return -1;
	}

	bzero( &(sSocket->address), sizeof( sSocket->address ) );
	sSocket->address.sin_family = AF_INET;
	sSocket->address.sin_port = htons( srcport ); //TODO: fix this up later
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
		bzero( data, maxDataSize );
		*option = OPT_QUIT;
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

int serverRespRequest( const serverSocket *accepted, const userOpts option, const unsigned char *data, const size_t dataSize, const char *highestDir ) {

	//Handle the options that require a response to the user
	if( (option >= OPT_FRESPONSEOPT) && (option <= OPT_LRESPONSEOPT) ) {
		//Setup some parameters for the user response
		unsigned char response[MAXDATASIZE];
		unsigned char message[MESSAGEBUFSIZE];
		size_t mesgSize, len;

		bzero( response, sizeof(response) ); //Fill response with Null chars

		//Handle requests which have one parameter - build the user response here
		if( (option >= OPT_FFILENAMEREQ) && (option <= OPT_LFILENAMEREQ) ) {
			char filename[MAXFILENAMESIZE];
			size_t fnamelen;
			int error = FALSE;

			//Copy the filename into our filename buffer
			if( copyInFilename( filename, sizeof(filename), &fnamelen, data, dataSize ) != 0 ) {
				error = TRUE;
			}

			//Don't allow any filenames which violate these rules
			if( validateFilename( filename, sizeof(filename), highestDir ) != 0 ) {
				errno = EPERM;
				error = TRUE;
			}

			if( !error ) {
				switch( option ) {
					case OPT_CD:
						if( chdir( filename ) != 0 ) {
							error = TRUE;
						} else {
							strncpy( (char *) response, "Success!", sizeof( response ) );
							len = strnlen( (char *) response, sizeof(response) );
						}
						break;
					case OPT_STAT:
						{
							struct stat statBuf;
							char times[3][40]; //Make buffers to store the access time strings

							if( stat( filename, &statBuf ) != 0 ) {
								error = TRUE;
							} else { 
								ctime_r( &statBuf.st_atime, times[0] ); //Convert the times to strings
								ctime_r( &statBuf.st_mtime, times[1] );
								ctime_r( &statBuf.st_ctime, times[2] );

								//This returns the number of chars it actually wrote
								len = snprintf( (char *) response, sizeof(response),
										"File: '%s'\nSize: %lli\tBlocks: %lli\tIO Block: %i\nDevice: %i\tInode: %lli\tHardlinks: %i\nAccess: %i\tUid: %i\tGid: %i\nAccess: %s Modify: %s Change: %s ",
										filename, statBuf.st_size, statBuf.st_blocks, statBuf.st_blksize,
										statBuf.st_rdev, statBuf.st_ino, statBuf.st_nlink, statBuf.st_mode,
										statBuf.st_uid, statBuf.st_gid, times[0], times[1], times[2] );

								len = ( len > (sizeof(response)-1) ) ? sizeof(response)-1 : len; //Set len to at most sizeof(response)
								response[len] = '\0'; //Just make sure
							}
						} break;
					case OPT_MKDIR:
						if( mkdir( filename, 0750 ) != 0 ) {
							error = TRUE;
						} else {
							strncpy( (char *) response, "Success!", sizeof( response ) );
							len = strnlen( (char *) response, sizeof(response) );
						}
						break;
					default: error=TRUE; break; //bummer - TODO: errors here
				}
			}
			if( error ) {
				prepError( errno, response, sizeof( response ), &len );
			}
			fmtMessage( option, response, len, message, sizeof( message ), &mesgSize );
		} else if( option == OPT_CP ) { //Handle the copy case
			char filenames[2][MAXFILENAMESIZE];
			size_t fnamelen[2];
			int error = FALSE;

			//Copy the filenames into our filename buffers
			{
				int i = 0, pos = 0;
				for( i=0, pos=0; i < 2 && !error; i++ ) {
					if( copyInFilename( filenames[i], sizeof(filenames[i]), &(fnamelen[i]), &(data[pos]), dataSize-pos ) != 0 ) {
						error = TRUE;
					}

					//Don't allow any filenames which violate these rules
					if( validateFilename( filenames[i], sizeof(filenames[i]), highestDir ) != 0 ) {
						errno = EPERM;
						error = TRUE;
					}

					if( (pos += fnamelen[i] + 1) > dataSize ) {
						error = TRUE;
					}
				}
			}

			if( !error ) {
				int writeFile=0, readFile=0;
				size_t bytesRead;
				char fileBuf[1024];

				readFile = open( filenames[0], O_RDONLY );
				writeFile = open( filenames[1], O_WRONLY|O_CREAT|O_EXCL );
				if( readFile < 0 || writeFile < 0 ) {
					error=TRUE; //Error opening reading file
				}

				while( !error && (bytesRead = read( readFile, fileBuf, sizeof(fileBuf) )) != 0 ) { //Read data and detect EOF
					if( bytesRead == -1 ) { //If there was an error reading in the data
						error=TRUE; //Error reading file
					}
					if( !error && write( writeFile, fileBuf, bytesRead ) != bytesRead ) { //Write the data and detect an error
						error=TRUE;
					}
				}

				close( readFile );
				close( writeFile );
			}

			if( !error ) {
				strncpy( (char *) response, "Success!", sizeof( response ) );
				len = strnlen( (char *) response, sizeof(response) );
			} else {
				prepError( errno, response, sizeof( response ), &len );
			}
			fmtMessage( option, response, len, message, sizeof( message ), &mesgSize );
		} else { //Handle options with no data parameters
			int error = FALSE;

			switch( option ) {
				case OPT_LS:
				{
					DIR *dir = opendir( "./" );
					struct dirent *entry;
					size_t pos = 0;
					while( (entry=readdir(dir)) != NULL ) {
						if( pos + entry->d_namlen + 1 < sizeof(response) ) { //If appending this entry and a newline to the response won't bust the response buffer...
							strncat( (char *) &(response[pos]), entry->d_name, entry->d_namlen );
							response[pos+entry->d_namlen] = '\n'; //Put newline at end of the entry
							response[pos+entry->d_namlen+1] = '\0'; //Make sure there's a null after it all
							pos += entry->d_namlen+1; //Update the position we write to
						} else {
							//TODO: Poop, not enough room to add the filename.  Oh well, just throw it out for now
						}
					}
					closedir( dir );

					len = pos;

					break;
				}
				case OPT_PWD:
					if( getcwd( (char *)response, sizeof( response ) ) == NULL ) {
						error = TRUE; //TODO: Handle error
					}
					response[ sizeof( response ) - 1 ] = '\0';
					len = strnlen( (char *) response, sizeof(response) );
					break;
				case OPT_HN:
					if( gethostname( (char *) response, sizeof( response ) ) == -1 ) {
						error = TRUE; //TODO: Handle error
					}
					response[ sizeof( response ) - 1 ] = '\0';
					len = strnlen( (char *) response, sizeof(response) );
					break;
				default: error = TRUE; break; //bummer - TODO: errors here
			} //Switch

			if( error ) {
				prepError( errno, response, sizeof( response ), &len );
			}
			fmtMessage( option, response, len, message, sizeof( message ), &mesgSize );
		} //if

		//Write out the message to the user
		if( write( accepted->sockRef, &message, mesgSize ) != mesgSize ) {
			//TODO: handle ERROR
			perror( "Error Writing Data Out" );
			return -1;
		}
	} else { //Handle the options that require no user response
		switch( option ) {
			case OPT_QUIT: return -1; break;
			default: break;
		}
	}

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
	printf( "1) List files \t\t2) Change directories \t3) Print current directory\n" );
	printf( "4) Print the hostname \t5) Copy a file \t\t6) Make a directory\n" );
	printf( "7) Print a file stat \t8) Quit\n" );
	printf( "? " );
	scanf( "%u", &sel );

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
		{	char formatStr[10]; //Dynamically build the format string, then read in the filename
			snprintf( formatStr, sizeof(formatStr), "%%%lus", sizeof(buffer) );
			scanf( formatStr, buffer ); }
		buffer[sizeof(buffer)-1] = '\0';
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
		{	char formatStr[10]; //Dynamically build the format string, then read in the filename
			snprintf( formatStr, sizeof(formatStr), "%%%lus", sizeof(buffer1) );
			scanf( formatStr, buffer1 ); }
		buffer1[sizeof(buffer1)-1] = '\0';
		len1 = strnlen( buffer1, sizeof(buffer1) ) + 1; //+1 makes the length include the null character
		if( len1 > sizeof(buffer1) || len1 > MAXDATASIZE ) { //If the user entered malformed data, or if the data is too large to fit in the data segment
			return -1; //Invalid user data. - Error TODO: set errno
		}

		printf( "Enter destination name: " );
		{	char formatStr[10]; //Dynamically build the format string, then read in the filename
			snprintf( formatStr, sizeof(formatStr), "%%%lus", sizeof(buffer2) );
			scanf( formatStr, buffer2 ); }
		buffer2[sizeof(buffer2)-1] = '\0';
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

int copyInFilename( char *filename, const size_t maxFNameSize, size_t *fnamelen, const unsigned char *data, const size_t dataSize ) {
			//Determine the length of the filename provided
			*fnamelen = strnlen( (char *) data, dataSize );
			if( *fnamelen <= 0 || *fnamelen >= maxFNameSize || *fnamelen >= dataSize ) { //If fnamelen1 doesn't make sense...
				return -1; //TODO: set some errors
			}

			strncpy( filename, (char *) data, maxFNameSize ); //Copy over the filename
			filename[*fnamelen] = '\0'; //Just make sure
			filename[maxFNameSize-1] = '\0'; //Really make sure

			return 0;
}

int prepError( int errval, unsigned char *response, const size_t maxResponseSize, size_t *actualResponseSize ) {
	char *errorSpot, failMsg[]="Failure! ";

	//Write out failure, then store in errorSpot the location we want to write the error to
	errorSpot = stpncpy( (char *) response, failMsg, maxResponseSize );
	strerror_r( errval, errorSpot, maxResponseSize - sizeof(failMsg) );

	response[maxResponseSize - 1] = '\0'; //Make sure...
	*actualResponseSize = strnlen( (char *) response, maxResponseSize );

	return 0;
}

//This function makes sure that filename doesn't try to access any directory above highestDir
//There are probably ways to get around these checks
//TODO: descriptive errnos?
int validateFilename( char *filename, const size_t filenameLen, const char *highestDir ) {
	int i = 0;

	//Is this a relative filename or an absolute one?
	//Find the first character in the filename that's not whitespace (a tab or a space, right now)
	for( i = 0; i < filenameLen && (filename[i]==' '||filename[i]=='\t'); i++ ) {
	}	

	//That first character must be a '/' for it to be an absolute filename
	if( filename[i] != '/' ) {
		//There cannot be more ".."'s than the difference between current dir depth and highest dir depth
		unsigned int highDirDepth = 0, curDirDepth = 0, periodPairs = 0;
		size_t highDirLen = 0, curDirLen = 0;

		char currentDir[MAXFILENAMESIZE];
		if( getcwd( currentDir, sizeof(currentDir) ) == NULL ) {
			return -1;
		}
		currentDir[MAXFILENAMESIZE-1] = '\0';

		if( !verifyBstartswithA( highestDir, currentDir ) ) { //This should definitely hold
			return -1;
		}

		//Get the length of the strings
		highDirLen = strnlen( highestDir, MAXFILENAMESIZE ); //Not the best maxlen choice, but it'll work here
		curDirLen = strnlen( currentDir, sizeof(currentDir) );

		//Get the depth of the directories in the FS - /root is depth 1, /root/home is depth 2
		//I had to subtract 1 from the filename length so that / is depth 0, and /root/ is still depth 1
		//That's a hack, but it works for now
		for( i = 0; i < highDirLen-1; i++ ) {
			if( highestDir[i] == '/' ) {
				highDirDepth++;
			}
		}
		for( i = 0; i < curDirLen-1; i++ ) {
			if( currentDir[i] == '/' ) {
				curDirDepth++;
			}
		}
	
		countPeriodPairs( filename, filenameLen, &periodPairs );

		//periodPairs must be < highDirDepth - curDirDepth
		//This conditional takes also considers the possiblity that we're already above the highest dir (bad)
		if( curDirDepth - periodPairs < highDirDepth ) {
			return -1;
		}
	} else { //It's absolute
		unsigned int periodPairs = 0;

		//Don't allow any ".."s
		countPeriodPairs( filename, filenameLen, &periodPairs );
		if( periodPairs > 0 ) {
			return -1;
		}
		//The characters of the filename must match the higestDir through the end of highestDir
		if( !verifyBstartswithA( highestDir, filename ) ) {
			return -1;
		}
	}
		

	return 0;
}

int countPeriodPairs( char *filename, const size_t filenameLen, unsigned int *periodPairs ) {
	int i = 0;

	//Check every character in filename from beginning through second to last character
	for( i = 0, *periodPairs=0; i < filenameLen-1 && filename[i+1] != '\0'; i++ ) {
		if( filename[i] == '.' && filename[i+1] == '.' ) {
			(*periodPairs)++;
		}
	}

	return 0;
}

int verifyBstartswithA( const char *A, const char *B ) {
	int i = 0;

	//Go through every A and make sure it corresponds to B
	//Don't go past the end of B, though
	for( i = 0; A[i]!='\0' && B[i]!='\0'; i++ ) {
		if( A[i] != B[i] ) {
			return FALSE;
		}
	}

	//If we're not at the end of A, then B must have ended before A
	if( A[i] != '\0' ) {
		return FALSE;
	}

	//We passed all the tests
	return TRUE;
}

int parseCMD( const int argc, char * const argv[], ipSpec *src, char *rootDir, const size_t rootDirSize ) {
	int retval;
	size_t len;

	//Accept -h, -p port, -s serveraddress, -r rootdirectory
	while( (retval = getopt( argc, argv, "hp:s:r:" )) != EOF ) { 
		switch( retval ) {
			case 'h':
				printf( "Usage: %s [-h] [-p portnum] [-s serveraddress] [-r rootdirectory]\n", argv[0] );
				printf( "-s has no effect on the server right now.\n-r has no effect on the client right now.\n" );
				return -1;
				break;
			case 'p':
				len = strnlen( optarg, sizeof(src->port) );
				if( len < sizeof(src->port) ) {
					strncpy( src->port, optarg, sizeof(src->port) );
					src->port[sizeof(src->port)] = '\0';
				}
				break;
			case 's':
				len = strnlen( optarg, sizeof(src->addr) );
				if( len < sizeof(src->addr) ) {
					strncpy( src->addr, optarg, sizeof(src->addr) );
					src->addr[sizeof(src->addr)] = '\0';
				}
				break;
			case 'r':
				len = strnlen( optarg, rootDirSize );
				if( len < rootDirSize ) {
					strncpy( rootDir, optarg, rootDirSize );
					rootDir[rootDirSize] = '\0';
				}
				break;
			default: break;
		}
	}
	return 0;
}
