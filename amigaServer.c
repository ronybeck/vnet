#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <libraries/bsdsocket.h>
#include <proto/bsdsocket.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <proto/dos.h>
#include <dos/dostags.h>
#include <dos/dos.h>
#include <dos/exall.h>
#include <proto/exec.h>

#include "Amiga/protocol.h"

struct Library * SocketBase = NULL;

/*****************************************************************************/

#define PORTNUMBER 20202

/*****************************************************************************/



//File receive parameters
#define RECEIVE_BUFFER_SIZE (1024*5)
#define TEXT_BUFFER_SIZE 2048
char *g_FileReceiveBuffer = NULL;
char *g_FileNameBuffer = NULL;
char *g_DirNameBuffer = NULL;
char *g_CommandBuffer = NULL;
ProtocolMessage_t *g_ProtocolMessage = NULL;
ProtocolMessage_DirEntry_t *g_ProtcolMessage_DirEntry = NULL;

/*****************************************************************************/

#include <proto/exec.h>
#define SocketBase FindTask(NULL)->tc_UserData
#define __BSDSOCKET_NOLIBBASE__
#include <proto/bsdsocket.h>



int recvData( SOCKET socketHandle, char *data, int size)
{
	int totalBytesReceived = 0;
	int retries = 10;

	//Keep pulling bytes until we are done or there is an error
	while( size > 0 && retries )
 	{
		int res;
		res = recv( socketHandle, data + totalBytesReceived, size, 0);
		if(res == -1)	break;
		if( res == 0 )
		{
			retries--;
			Delay( 10 );
			printf( "timeout on receiving.  %d timeouts remaining.\n", retries );
		}
		totalBytesReceived += res;
		size -= res;
	}
	return totalBytesReceived;
}


int recvLargeFile(SOCKET socketHandle, BPTR fileHandle, ULONG fileSize )
{
	ULONG totalBytesReceived = 0;
	ULONG bytesToGet = 0;
	int bytesReceivedFromSocket = 0;

	//Pull the file
	while( totalBytesReceived < fileSize )
	{
		//Get the next chunk of data
		if( fileSize > 4096 )
			printf( "\rDownloading %d/%d Kilobytes", (int)totalBytesReceived/1024, (int)fileSize/1024 );
		bytesToGet = ( fileSize - totalBytesReceived ) > RECEIVE_BUFFER_SIZE ? RECEIVE_BUFFER_SIZE : ( fileSize - totalBytesReceived );
		//printf( "Getting %d bytes\n", bytesToGet );
		bytesReceivedFromSocket = recvData( socketHandle, g_FileReceiveBuffer, bytesToGet );
		//printf( "Got %s bytes\n", bytesReceivedFromSocket );
		if( bytesReceivedFromSocket < bytesToGet || bytesReceivedFromSocket == 0 )
		{
			//Failed to get all bytes.  This should normally only happen if there is a network problem
			break;
		}

		//Write the data to the file
		Write( fileHandle, g_FileReceiveBuffer, bytesReceivedFromSocket );
		totalBytesReceived += bytesReceivedFromSocket;
	}

	//We are done here
	printf("\n");
	return totalBytesReceived;
}

int recvSmallFile(SOCKET socketHandle, BPTR fileHandle, ULONG fileSize )
{
	ULONG bytesReceived = 0;

	//Download the file
	bytesReceived = recvData( socketHandle, g_FileReceiveBuffer, fileSize );
	if( bytesReceived > 0 )
	{
		Write( fileHandle, g_FileReceiveBuffer, bytesReceived );
	}

	return bytesReceived;
}


int ReceiveFile(SOCKET socketHandle )
{
	int nameLen = 0;
	int fileLen = 0;
	BPTR fileHandle = (BPTR)NULL;
	int returnCode = 0;

	//Get file name length
	recvData( socketHandle, (char*)&nameLen, sizeof(nameLen));
	nameLen = ntohl(nameLen);

	//Crete a buffer for the filename and store this
	memset( g_FileNameBuffer, 0, TEXT_BUFFER_SIZE );
	recvData( socketHandle, g_FileNameBuffer, nameLen);
	//printf("file name : %s\n", fileName );

	//Get the length of the file
	recvData( socketHandle, (char*)&fileLen, sizeof(fileLen) );
	fileLen = ntohl(fileLen);
	//printf("File length : %d\n", fileLen);

	//Create the file
	fileHandle = Open( g_FileNameBuffer, MODE_NEWFILE );
	if( fileHandle == (BPTR)NULL )
	{
		printf( "Unable to open file '%s' for writing.\n", g_FileNameBuffer );
		PrintFault( IoErr(), "Error" );
		return 1;
	}

	//Download it
	if( fileLen > RECEIVE_BUFFER_SIZE )
	{
		printf( "Receiving large file %s\n", g_FileNameBuffer );
		int bytesReceived = recvLargeFile( socketHandle, fileHandle, fileLen );
		if( fileLen != bytesReceived )
		{
			printf( "Failed to receive file '%s'.  Only %d/%d kilobytes received.", g_FileNameBuffer, bytesReceived/1024, fileLen/1024);
			returnCode = 1;
		}
	}
	else
	{
		printf( "Receiving small file %s\n", g_FileNameBuffer );
		int bytesReceived = recvSmallFile( socketHandle, fileHandle, fileLen );
		if( fileLen != bytesReceived )
		{
			printf( "Failed to receive file '%s'.  Only %d/%d kilobytes received.", g_FileNameBuffer, bytesReceived/1024, fileLen/1024);
			returnCode = 1;
		}
	}


	//printf( "Closing file handle.\n" );
	Close( fileHandle );

	return returnCode;
}

/*****************************************************************************/

void ExecuteCLICommand(char *command)
{
	struct TagItem tags[3];

	tags[0].ti_Tag = SYS_Input;
	tags[0].ti_Data = Input();
	tags[1].ti_Tag = SYS_Output;
	tags[1].ti_Data = Output();
	tags[2].ti_Tag = TAG_END;

	System(command, tags);
}

int RunNetCommand(SOCKET s)
{
	int commandLen;
	char *command = g_CommandBuffer;
	memset( g_CommandBuffer, 0, TEXT_BUFFER_SIZE );
	recvData(s, (char*)&commandLen, sizeof(commandLen));
	commandLen = ntohl(commandLen);
	printf("command length %d\n", commandLen);
	recvData(s, command, commandLen);
	command[commandLen] = 0;
	printf("%s\n", command);

	ExecuteCLICommand(command);


	return 0;
}

/*****************************************************************************/


int mkdirCommand( SOCKET socketHandle )
{
	int nameLen = 0;

	//Get file name length
	memset( g_DirNameBuffer, 0, TEXT_BUFFER_SIZE );
	recvData( socketHandle, (char*)&nameLen, sizeof(nameLen));
	nameLen = ntohl(nameLen);

	//Crete a buffer for the filename and store this
	recvData( socketHandle, g_DirNameBuffer, nameLen );
	printf( "Directory name: %s\n", g_DirNameBuffer );

	//Let's see if we don't already have this directory
	BPTR existingDir = Lock( g_DirNameBuffer, ACCESS_READ );
	if( existingDir )
	{
		//Looks like this dir exists already.  Nothing to do
		UnLock( existingDir );
		return 0;
	}

	//Create the directory
	BPTR dirLock = CreateDir( g_DirNameBuffer );
	if( dirLock == (int)NULL )
	{
		printf( "Unable to create a directory lock for '%s'\n", g_DirNameBuffer );
		return 1;
	}

	//Unlock the lock
	UnLock( dirLock );

	//We are done
	return 0;
}

int sendDirStructure( SOCKET socketHandle, struct ExAllData fileData )
{

	return 0;
}

int getDirCommand( SOCKET socketHandle )
{
	int nameLen = 0;
	short numberOfEntries = 0;
	BPTR dirLock = 0;
	struct FileInfoBlock fileInfoBlock;

	//Get file name length
	memset( g_DirNameBuffer, 0, TEXT_BUFFER_SIZE );
	recvData( socketHandle, (char*)&nameLen, sizeof(nameLen));
	nameLen = ntohl(nameLen);

	//Crete a buffer for the filename and store this
	recvData( socketHandle, g_DirNameBuffer, nameLen );
	printf( "Directory name: %s\n", g_DirNameBuffer );

	//Let's see if we don't already have this directory
	dirLock = Lock( g_DirNameBuffer, ACCESS_READ );
	if( !dirLock )
	{
		//This directory doesn't exist.  Send back an empty answer
		printf( "The directory %s doesn't exist?\n", g_DirNameBuffer );
		send( socketHandle, &numberOfEntries, sizeof( numberOfEntries), 0 );
		return 0;
	}

	//Is this a directory?
	if( !Examine( dirLock, &fileInfoBlock ) || fileInfoBlock.fib_DirEntryType < 1 )
	{
		UnLock( dirLock );
		printf( "The path '%s' is not a directory." );
		//This is not a directory.  Send back an empty answer
		send( socketHandle, &numberOfEntries, sizeof( numberOfEntries), 0 );
		return 0;
	}

	//Create an ExAllControl Object
	struct ExAllControl *control = AllocDosObject(DOS_EXALLCONTROL,NULL);
	char *matchString = "#?";
	if( !control )
	{
		UnLock( dirLock );
		printf( "Unable to create a ExAllControl object.\n" );
		return 0;
	}
	//control->eac_MatchString = matchString;
	//control->eac_LastKey = 0;

	//Iterate through the entries untile we are the end
	ULONG bufferLen =  sizeof( struct ExAllData ) * 100;
	printf( "Opening directory %s to scan\n", g_DirNameBuffer );
	struct ExAllData *buffer = AllocVec( bufferLen, MEMF_FAST|MEMF_CLEAR );
	struct ExAllData *currentEntry = NULL;
	int debugLimit=20;
	BOOL more;
	do
	{
		printf( "Checking for (more) directory entries.\n" );
		more = ExAll( dirLock, buffer, bufferLen, ED_OWNER, control );
		if ((!more) && (IoErr() != ERROR_NO_MORE_ENTRIES))
		{
			printf( "No more entries.\n" );
			break;
		}
		if( control->eac_Entries == 0 )	continue;

		//Go through the current batch of entries
		currentEntry = buffer;
		while( currentEntry )
		{
			printf( "Found " );
			switch( currentEntry->ed_Type )
			{
				case ST_FILE:
					printf( "file " );
					break;
				case ST_USERDIR:
					printf( "dir " );
					break;
				case ST_ROOT:
					printf( "root " );
					break;
				case ST_SOFTLINK:
					printf( "softlink " );
					break;
				default:
					printf( "unknown type " );
					break;
			}

			printf( "'%s' which is %d bytes in size.\n", currentEntry->ed_Name, currentEntry->ed_Size );
			currentEntry = currentEntry->ed_Next;
		}
	}while( more );
	FreeDosObject( DOS_EXALLCONTROL, control );
	FreeVec( buffer );

	printf( "Unlocking directory.\n" );
	UnLock( dirLock );
	//We are done
	return 0;
}

void ProcessCommands(SOCKET s)
{
	int res;
	int running = 1;
	while(running)
	{
		int command;
		res = recvData(s, (char*)&command, sizeof(command));
		if(!res)
		{
			running = 0;
			return;
		}
		switch(command)
		{
			case COMMAND_SEND_FILE :
			{
				//printf("COMMAND_SEND_FILE\n");
				if( ReceiveFile(s) )
				{
					//printf( "Error during SENDFILE.\n" );
					running = 0;
				}
			} break;
			case COMMAND_RUN :
			{
				//printf("COMMAND_RUN\n");
				if( RunNetCommand(s) )
				{
					printf("Error during COMMAND_RUN\n");
					running = 0;
				}
			} break;
			case COMMAND_CLOSE :
			{
				running = 0;
				//printf("COMMAND_CLOSE\n");
			} break;
			case COMMAND_MKDIR:
			{
				//printf( "COMMAND_MKDIR\n." );
				if( mkdirCommand( s ) )
				{
					printf( "Error during COMMAND_MKDIR\n." );
					running = 0;
				}
				break;
			}
			case COMMAND_DIR:
			{
				getDirCommand( s );
				break;
			}
			default :
			{
				running = 0;
				printf("Unknown command received\n");
			} break;
		}
	}
}

/*****************************************************************************/

int main(int argc, char *argv[])
{
	struct sockaddr_in addr;
	SOCKET s = 0;
	SOCKET server = 0;
	int res = 0;

	//We want to do at least a few attempts to start this
	//Maybe the TCP/Stack is starting in the back ground
	//and we need to give it a few seonconds to start
	int numberOfRestartAttempts = 15;

	while( numberOfRestartAttempts-- > 0 )
	{
		printf( "Opening bsdsocket.library.\n" );
		if( (SocketBase = OpenLibrary("bsdsocket.library", 4 )))
			break;
		printf("No TCP/IP Stack running!\n");
		printf( "Will retry in 1 second.\n" );
		Delay( 100 );
	}

	//Did we open the bsdsocket library successfully?
	if( !SocketBase )
	{
		printf( "Failed to open bsdsocket.library.  Is the TCP Stack running?.\n" );
		return 1;
	}


	//Create a buffer for the file write
	g_FileReceiveBuffer = (char*)AllocVec( RECEIVE_BUFFER_SIZE, MEMF_FAST );
	if( !g_FileReceiveBuffer )
	{
		printf( "Failed to reserve a receive buffer.\n");
		return 1;
	}

	//Create a buffer for filepaths
	g_FileNameBuffer = (char*)AllocVec( TEXT_BUFFER_SIZE, MEMF_FAST );
	if( !g_FileNameBuffer )
	{
		printf( "Failed to reserve a filepath buffer.\n" );
		FreeVec( g_FileReceiveBuffer );
		return 1;
	}

	//Create a buffer for directory paths
	g_DirNameBuffer = (char*)AllocVec( TEXT_BUFFER_SIZE, MEMF_FAST );
	if( !g_DirNameBuffer )
	{
		printf( "Failed to reserve a filepath buffer.\n" );
		FreeVec( g_FileReceiveBuffer );
		FreeVec( g_FileNameBuffer );
		return 1;
	}

	//Create a buffer for commands
	g_CommandBuffer = (char*)AllocVec( TEXT_BUFFER_SIZE, MEMF_FAST );
	if( !g_CommandBuffer )
	{
		printf( "Failed to reserve a command buffer.\n" );
		FreeVec( g_FileReceiveBuffer );
		FreeVec( g_FileNameBuffer );
		FreeVec( g_DirNameBuffer );
		return 1;
	}

	//reserve some memory for the protocol
	g_ProtocolMessage = AllocVec( 1024, MEMF_FAST );
	g_ProtcolMessage_DirEntry = AllocVec( 512, MEMF_FAST );


	printf( "Opening socket.\n" );
	server = socket(AF_INET, SOCK_STREAM, 0);
	if(server == SOCKET_ERROR)
	{
		printf("socket error\n");
		return 1;
	}

	printf( "Setting socket options.\n" );
	int yes = 1;
	res = setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
	if(res == SOCKET_ERROR)
	{
		printf("setsockopt error\n");
		return 1;
	}

	printf( "Binding to port %d\n", PORTNUMBER );
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(PORTNUMBER);
	res = bind(server, (struct sockaddr *)&addr, sizeof(addr));
	if(res == SOCKET_ERROR)
	{
		printf("bind error\n");
		return 1;
	}

	printf( "Listening on port.\n" );
	res = listen(server, 10);
	if(res == SOCKET_ERROR)
	{
		printf("listen error\n");
		return 1;
	}
	while(1)
	{
		printf("Awaiting new connection\n");
		socklen_t addrLen = sizeof(addr);
		SOCKET s = accept(server, (struct sockaddr *)&addr, &addrLen);
		if( s < 0 )
		{
			printf("accept error %d \"%s\"\n", errno, strerror(errno));
			break;
		}
		ProcessCommands(s);
		printf("Closing connection\n");
		CloseSocket(s);
	}
	CloseSocket(server);

	FreeVec( g_FileReceiveBuffer );
	FreeVec( g_FileNameBuffer );
	FreeVec( g_DirNameBuffer );
	FreeVec( g_CommandBuffer );
	FreeVec( g_ProtocolMessage );
	FreeVec( g_ProtcolMessage_DirEntry );

	return 0;
}




