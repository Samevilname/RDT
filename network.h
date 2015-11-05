/*
network.h
*/

#pragma once 
#include <deque>
#include <time.h>
#include "transport.h"
using namespace std;

//This function is where the network thread starts
UINT NetworkThread(LPVOID pParam);

class NetworkParameters {
public:
	int			in_port, dest_port;	// this threads receives packets from in_port, sends to dest_port
	Messenger	*m;				// Messenger class
	HANDLE		wait_init;		// signals whoever called us that the init is over
	int			protocol;		// user-selected protocol
};

class Packet{
public:
	char		*pkt_data; 		// pointer to where pkt data is
	int 		pkt_size;		// number of bytes in pkt
	double		d;				// departure time in seconds; could be fractional
	bool		has_been_reordered;		// true if the packet has been reordered once before
};

class Network {
public:
	Messenger	*m;							// printf class
	SOCKET		sock_in, sock_out;			// in/out sockets of the network
	struct		sockaddr_in destination;	// destination address
	double		rtt;						// fixed propagation RTT (ms)
	double		max_q_delay;				// maximum queuing delay (ms)
	double		p_loss;						// loss rate
	double		p_reorder;					// reordering rate
	int			link_capacity;				// in bps

	deque<Packet> Q;						// double sided for access to last elem
				
	Network (NetworkParameters *np);
	void		Begin (void);				// main loop
	void		PushInNetworkQueue (Packet pkt); // adds one packet to the end of the queue, configures its departure time
};
