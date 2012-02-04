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

#include "serverFuncs.h"

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

//Add a process to our linked list of processes
int addProc( procParams **listHeadPtr, procParams *newItem ) {
	if( *listHeadPtr == NULL ) {
		*listHeadPtr = newItem;
	} else {
		procParams *cur;
		for( cur = *listHeadPtr; cur->next != NULL; cur=cur->next ) { //Peruse to the last item in the list
		}
		cur->next = newItem;
		newItem->next = NULL; //make sure
	}

	return 0;
}

//Remove a thread from our linked list.  Also, deallocate any memory it has, and cleanup the PID
int remProc( procParams **listHeadPtr, procParams *remItem ) {
	if( *listHeadPtr == NULL ) {
		return -1;  //no items are in the list - that's bad
	}

	procParams *cur = *listHeadPtr, *prev = NULL;

	//Peruse to the correct list item
	for( cur = *listHeadPtr; cur != NULL && cur != remItem; prev=cur, cur=cur->next ) {
	}

	if( cur == NULL ) {
		return -1; //we didn't find remItem, so we can't remove it
	}

	//Cut out the item
	if( cur == *listHeadPtr ) { //we're removing the head of the list
		*listHeadPtr = cur->next;
	} else {
		prev->next = cur->next;
	}

	free( cur->acceptedSock );
	free( cur );

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
