/*
 * VNetDiscoveryThread.c
 *
 *  Created on: Jul 10, 2021
 *      Author: rony
 */


#include <unistd.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <dos/dostags.h>
#include <workbench/workbench.h>
#include <proto/exec.h>
#include <exec/lists.h>
#include <sys/types.h>
#include <clib/icon_protos.h>
#include <clib/dos_protos.h>
#define __BSDSOCKET_NOLIBBASE__
#include <proto/bsdsocket.h>

#define DBGOUT 1

#include "protocol.h"
#include "VNetUtil.h"

extern char g_KeepServerRunning;

static void discoveryThread();

static void getOSVersion( char *version, LONG len )
{
	struct Library *ExecLibrary = NULL;

	//Did we get the DOSBase
	dbglog( "[getOSVersion] Opening Exec Library.\n" );
	ExecLibrary = OpenLibrary( "exec.library", 0 );
	if( ExecLibrary != NULL )
	{
		dbglog( "[getOSVersion] Exec Library opened.  Examining version.\n" );
		snprintf( version, len, "%d.%d", ExecLibrary->lib_Version, ExecLibrary->lib_Revision );
		dbglog( "[getOSVersion] OS Version is %s.\n", version );
	}
	dbglog( "[getOSVersion] Done.\n" );
}

void startDiscoveryThread()
{
	if ( DOSBase )
	{

		/*
		BPTR consoleHandle = Open( "CONSOLE:", MODE_OLDFILE );
		if( consoleHandle )
		{
			dbglog( "[startDiscoveryThread] Console handle opened.\n" );
		}
		else
		{
			dbglog( "[startDiscoveryThread] Console handle NOT opened.\n" );
		}
		*/

		//Start a client thread
		struct TagItem tags[] = {
				{ NP_StackSize,		8192 },
				{ NP_Name,			(ULONG)"VNetDiscoveryThread" },
				{ NP_Entry,			(ULONG)discoveryThread },
				//{ NP_Output,		(ULONG)consoleHandle },
				{ NP_Synchronous, 	FALSE },
				{ TAG_DONE, 0UL }
		};
		dbglog( "[startDiscoveryThread] Starting discovery thread.\n" );
		struct Process *clientProcess = CreateNewProc(tags);
		if( clientProcess == NULL )
		{
			dbglog( "[startDiscoveryThread] Failed to create client thread.\n" );

			return;
		}
		dbglog( "[startDiscoveryThread] Thread started.\n" );
	}
}

static void printInterfaceDebug()
{
	dbglog( "[discoveryThread] Searching for network interfaces.\n" );
	struct List *interfaceList;
	struct Node *node;
	UBYTE interfaceCount = 0;

	//Get the list
	interfaceList = ObtainInterfaceList();
	if( interfaceList == NULL )
	{
		dbglog( "[discoveryThread] unable to get a list of network interfaces.\n" );
		return;
	}
	node = interfaceList->lh_Head;

	while( node )
	{
		if( node->ln_Name == 0 )
			break;
		dbglog( "[discoveryThread] Found interface: %s\n", node->ln_Name );
		node = node->ln_Succ;
		interfaceCount++;
	}
	dbglog( "[discoveryThread] %d network interfaces discovered.\n", interfaceCount );
	dbglog( "[discoveryThread] Network interface discovery completed.\n" );
	ReleaseInterfaceList( interfaceList );
}

#define ENABLE_DISCOVERY_REPLY 0
static void discoveryThread()
{
	dbglog( "[discoveryThread] Client thread started.\n" );

	struct Library *DOSBase = NULL;
	struct Library *SocketBase = NULL;
	int returnCode = 0;
	int yes = 1;
	char *osVersion[32];

	//Did we get the DOSBase
	DOSBase = OpenLibrary( "dos.library", 0 );
	if( DOSBase == NULL )
	{
		dbglog( "[discoveryThread] Failed to get DOSBase.\n" );
		return;
	}

	//Open the BSD Socket library
	dbglog( "[discoveryThread] Opening bsdsocket.library.\n" );

	//Did we open the bsdsocket library successfully?
	SocketBase = OpenLibrary("bsdsocket.library", 4 );
	if( !SocketBase )
	{
		dbglog( "[discoveryThread] Failed to open the bsdsocket.library.  Exiting.\n" );
		return;
	}

	printInterfaceDebug();

#if ENABLE_DISCOVERY_REPLY
	//Open a new server port for this client
	dbglog( "[discoveryThread]  Opening client socket.\n" );
	SOCKET discoverySocket = socket(AF_INET, SOCK_DGRAM, 0 );
	if( discoverySocket < 0 )
	{
		dbglog( "[discoveryThread] Error creating UDP Socket.\n" );
		return;
	}

	dbglog( "[discoveryThread] Setting reuse socket options.\n" );
	returnCode = setsockopt( discoverySocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
	if(returnCode == SOCKET_ERROR)
	{
		dbglog( "[discoveryThread] Error setting reuse socket options. Exiting.\n" );
		CloseSocket( discoverySocket );
		return;
	}




	//setting the bind port
	dbglog( "[discoverySocket] Preparing to bind to port %d\n", BROADCAST_PORTNUMBER );
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	dbglog( "[discoverySocket] Setting local address\n" );
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons( BROADCAST_PORTNUMBER );
	dbglog( "[discoverySocket] Binding to port %d\n", BROADCAST_PORTNUMBER );
	returnCode = bind( discoverySocket, (struct sockaddr *)&addr, sizeof(addr));
	if(returnCode != 0 )
	{
		dbglog( "[discoverySocket] Unable to bind to port %d. Error Code: %d\n", BROADCAST_PORTNUMBER, returnCode );
		switch( returnCode )
		{
			case EBADF:
				dbglog( "[discoverySocket] Not a valid descriptor.\n" );
				break;
			case ENOTSOCK:
				dbglog( "[discoverySocket] Not a socket.\n" );
				break;
			case EADDRNOTAVAIL:
				dbglog( "[discoverySocket] Address not available on machine.\n" );
				break;
			case EADDRINUSE:
				dbglog( "[discoverySocket] Address in use.\n" );
				break;
			case EINVAL:
				dbglog( "[discoverySocket] Socket already nound to this address.\n" );
				break;
			case EACCES:
				dbglog( "[discoverySocket] Permission denied.\n" );
				break;
			case EFAULT:
				dbglog( "[discoverySocket] The name parameter is not in a valid part of the user address.\n" );
				break;
			default:
				dbglog( "[discoverySocket] Some other error occurred (%d).\n", returnCode );
				break;
		}
		CloseSocket( discoverySocket );
		return;
	}
	dbglog( "[discoverySocket] Bound to port %d\n", BROADCAST_PORTNUMBER );
#endif

	//Create a socket for broadcasting
	struct sockaddr_in broadcastAddr;
	memset( &broadcastAddr, 0, sizeof( broadcastAddr ) );
	broadcastAddr.sin_family = AF_INET;
	broadcastAddr.sin_addr.s_addr = INADDR_BROADCAST;
	broadcastAddr.sin_port = htons( BROADCAST_PORTNUMBER );

	//Create a reply socket for sending the broadcast
	dbglog( "[discoveryThread]  Opening broadcast socket.\n" );
	SOCKET broadcastSocket = socket(AF_INET, SOCK_DGRAM, 0 );
	if( broadcastSocket < 0 )
	{
		dbglog( "[discoveryThread] Error creating broadcast UDP Socket.\n" );

		//Shutdown the discovery socket
#if ENABLE_DISCOVERY_REPLY
		CloseSocket( discoverySocket );
#endif
		return;
	}

	dbglog( "[discoveryThread] Setting reuse socket options.\n" );
	returnCode = setsockopt( broadcastSocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
	if(returnCode == SOCKET_ERROR)
	{
		dbglog( "[discoveryThread] Error setting reuse socket options. Exiting.\n" );
#if ENABLE_DISCOVERY_REPLY
		CloseSocket( discoverySocket );
#endif
		CloseSocket( broadcastSocket );
		return;
	}

	dbglog( "[discoveryThread] Setting broadcast socket options.\n" );
	returnCode = setsockopt( broadcastSocket, SOL_SOCKET, SO_BROADCAST, &yes, sizeof(yes));
	if(returnCode == SOCKET_ERROR)
	{
		dbglog( "[discoveryThread] Error setting broadcast socket options. Exiting.\n" );
#if ENABLE_DISCOVERY_REPLY
		CloseSocket( discoverySocket );
#endif
		CloseSocket( broadcastSocket );
		return;
	}



	//Prepare our messages
	ProtocolMessage_DeviceDiscovery_t *discoveryMessage = AllocVec( sizeof( ProtocolMessage_DeviceDiscovery_t ), MEMF_FAST|MEMF_CLEAR );
	ProtocolMessage_DeviceAnnouncement_t *announceMessage = AllocVec( sizeof( ProtocolMessage_DeviceAnnouncement_t ), MEMF_FAST|MEMF_CLEAR );
	announceMessage->header.token = MAGIC_TOKEN;
	announceMessage->header.type = PMT_DEVICE_ANNOUNCE;
	announceMessage->header.length = sizeof( ProtocolMessage_DeviceAnnouncement_t );

	//Add default Values
	strncpy( announceMessage->name, "Unnamed", sizeof( announceMessage->name ) );
	strncpy( announceMessage->osName, "ApolloOS?", sizeof( announceMessage->osName ) );
	strncpy( announceMessage->osVersion, "-", sizeof( announceMessage->osVersion ) );
	strncpy( announceMessage->hardware, "Unknown", sizeof( announceMessage->hardware ) );

	//Let's see what is in the tool types
	dbglog( "[discoverySocket] Opening disk object\n" );
	struct DiskObject *diskObject = GetDiskObject( "VNetServerAmiga" );
	if( diskObject )
	{
		dbglog( "[discoverySocket] Opened disk object\n" );

		//get the variables we want
		STRPTR name = FindToolType( diskObject->do_ToolTypes, (STRPTR)"name" );
		if( name )
		{
			dbglog( "[discoverySocket] ToolTypes Name: %s\n", name );
			strncpy( announceMessage->name, name, sizeof( announceMessage->name ) );
		}
		STRPTR osname = FindToolType( diskObject->do_ToolTypes, (STRPTR)"osname" );
		if( osname )
		{
			dbglog( "[discoverySocket] ToolTypes osname: %s\n", osname );
			strncpy( announceMessage->osName, osname, sizeof( announceMessage->name ) );
		}
		//getOSVersion( osVersion, sizeof( osVersion ) );	//WIP
		if( strlen( osVersion ) == 0 )
		{
			STRPTR osversion = FindToolType( diskObject->do_ToolTypes, "osversion" );
			if( osname )
			{
				dbglog( "[discoverySocket] ToolTypes osversion: %s\n", osversion );
				strncpy( announceMessage->osVersion, osversion, sizeof( announceMessage->osVersion ) );
			}
		}else
		{
			dbglog( "[discoverySocket] ToolTypes osversion: %s\n", osVersion );
			strncpy( announceMessage->osVersion, osVersion, sizeof( announceMessage->osVersion ) );
		}
		STRPTR hardware = FindToolType( diskObject->do_ToolTypes, (STRPTR)"hardware" );
		if( osname )
		{
			dbglog( "[discoverySocket] ToolTypes hardware: %s\n", hardware );
			strncpy( announceMessage->hardware, hardware, sizeof( announceMessage->hardware ) );
		}
		FreeDiskObject( diskObject );
	}

	//Addressing information
	struct sockaddr *requestorAddress = AllocVec( sizeof( *requestorAddress ), MEMF_FAST|MEMF_CLEAR );

	//Start reading all in-bound messages
	int bytesRead = 0;
	int bytesSent = 0;
	char keepThisConnectionRunning = 1;
	while( g_KeepServerRunning && keepThisConnectionRunning )
	{
#if ENABLE_DISCOVERY_REPLY
		socklen_t requestorAddressLen = sizeof( *requestorAddress );
		memset( requestorAddress, 0, requestorAddressLen );
		bytesRead = recvfrom( 	discoverySocket,
								discoveryMessage,
								sizeof( ProtocolMessage_DeviceDiscovery_t ),
								0,
								(struct sockaddr *)requestorAddress,
								&requestorAddressLen );

		//If the datagram isn't the right size, ignore it.
		if( bytesRead != sizeof( ProtocolMessage_DeviceDiscovery_t ) )
		{
			dbglog( "[discoverySocket] Ignoring packet which was only %d bytes in size.\n", bytesRead );
			continue;
		}

		//Ignore invalid tokens
		if( discoveryMessage->header.token != MAGIC_TOKEN )
		{
			dbglog( "[discoverySocket] Ignoring packet with invalid token 0x%08x.\n", discoveryMessage->header.token );
			continue;
		}

		//Ignore invalid types
		if( discoveryMessage->header.type != PMT_DEVICE_DISCOVERY )
		{
			dbglog( "[discoverySocket] Ignoring packet with invalid type 0x%08x.\n", discoveryMessage->header.type );
			continue;
		}

		//If we got this far, then we should send the device info
		//dbglog( "[discoverySocket] Sending reply to device discovery.\n" );
		bytesSent = sendto( discoverySocket, announceMessage, announceMessage->header.length, 0, requestorAddress, requestorAddressLen );

#endif
		//dbglog( "[discoverySocket] Sent %d bytes.\n", bytesSent );
		bytesSent = sendto( broadcastSocket, announceMessage, announceMessage->header.length, 0, (struct sockaddr *)&broadcastAddr, sizeof( broadcastAddr ) );
		dbglog( "[discoverySocket] Sent %d bytes.\n", bytesSent );
		//(void)bytesSent;


		//To prevent overloading the amiga....
		Delay( 100 );
	}

	//exit_discovery:
	//Free our messagebuffer
	FreeVec( discoveryMessage );
	FreeVec( announceMessage );
	FreeVec( requestorAddress );

#if ENABLE_DISCOVERY_REPLY
	//Now close the socket because we are done here
	dbglog( "[discoverySocket] Closing client thread for socket 0x%08x.\n", discoverySocket );
	CloseSocket( discoverySocket );
#endif

	dbglog( "[discoverySocket] Closing client thread for socket 0x%08x.\n", broadcastSocket );
	CloseSocket( broadcastSocket );

	CloseLibrary( DOSBase );
	CloseLibrary( SocketBase );

	return;
}
