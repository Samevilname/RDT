/* 
winsock.cpp
*/

#include "winsock.h"

Winsock::Winsock(Messenger *m)
{
	this->m = m;	// save the pointer to Messenger class
}

SOCKET Winsock::OpenSocket (int port)
{
	// open a UDP socket
	SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);  //SOCK_DGRAM for UDP sockets
	if(sock == INVALID_SOCKET)
	{
		m->printf ("socket() generated error %d\n", WSAGetLastError ());
		WSACleanup ();	
		exit (-1);	// not graceful, but sufficient for our purposes
	}

	struct sockaddr_in bind_addr;

	// bind to the requested port
	bind_addr.sin_family = AF_INET;
	bind_addr.sin_addr.S_un.S_addr = htonl(INADDR_ANY); //INADDR_ANY: choose the local address
	bind_addr.sin_port = htons (port);	//assign the IP port number in network byte order

	if (bind (sock, (struct sockaddr*) &bind_addr, sizeof(struct sockaddr_in)) == SOCKET_ERROR)
	{
		m->printf ("Bind failed with %d\n", WSAGetLastError());
		exit (-1);
	}

	// set up some options on the socket
	// 1) increase the send buffer size in the kernel; otherwise pkts may be dropped
	int bufsize = 10000000;		// 10 MB
	if(setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (char *)&bufsize, sizeof (int)) == SOCKET_ERROR) 
	{
		m->printf ("setsockopt/SO_SNDBUF error %d\n", WSAGetLastError()) ;
		exit(-1);
	}
	
	// 2) increase the receive buffer size in the kernel; otherwise pkts may be dropped
	if(setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char *)&bufsize, sizeof (int)) == SOCKET_ERROR) 
	{
		m->printf ("setsockopt/SO_RCVBUF error %d\n", WSAGetLastError()) ;
		exit(-1);
	}
	
	return sock;
}
