#include <math.h>
#include "network.h"

// Network thread starts here
UINT NetworkThread(LPVOID pParam)
{
	NetworkParameters *np = (NetworkParameters*)pParam;
	Network n(np);

	n.Begin();

	return 0;
}

Network::Network (NetworkParameters *np)
{
	this->m = np->m; 
	// set up dest address of sock_out
	destination.sin_family = AF_INET;
	destination.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");   
	destination.sin_port = htons (np->dest_port);
	// init the random number generator
	srand((unsigned) time(NULL));

	// open in/out sockets; bind to requested ports
	Winsock r (m);
	sock_in = r.OpenSocket (np->in_port);	// listen on this port
	sock_out = r.OpenSocket (0);			// send packets out on this port

	// release the calling thread since all ports are now open
	SetEvent (np->wait_init);

	rtt = 30;					// 30 ms
	max_q_delay = 1;			// 1 ms
	link_capacity = (int) 1e8;	// 100 Mbps

	switch(np->protocol)
	{
	case RDT3:
		p_reorder = 0;		// cannot reorder with rdt3.0
		p_loss = 0.1;		// 1%
		break;

	case GBN:
		p_reorder = 0.01;	// 1%
		p_loss = 0.01;		// 1%
		break;

	case STCP:
		p_reorder = 0.01;	// 1%
		p_loss = 0.01;		// 1%
		break;
	}

}

void Network::Begin (void) 
{
	// an alternative to select() is to use Windows events; first create a regular auto-reset event
	HANDLE eventSocketReady =  CreateEvent (NULL, false, false, NULL);
	// second, register sock_in with the event using FD_READ notifications only (i.e., available for READ)
	WSAEventSelect(sock_in, eventSocketReady, FD_READ);	// associate event with socket

	char buf [MAX_PKT_SIZE];
	DWORD timeout = INFINITE;  //infinite timeout

	while (true)
	{
		int ret;
		
		if (timeout != 0)
			ret = WaitForSingleObject (eventSocketReady, timeout);// either a timeout occurs or something is pending inside sock_in
		
		double cur_time = m->GetTime();
			
		// packets are ready to be extracted?
		if (timeout == 0 || ret == WAIT_TIMEOUT)
		{
			// examine the front packet
			Packet head = Q.front();

			// can be tranmitted right now?
			while (head.d <= cur_time)
			{
				// remove from the queue
				Q.pop_front();

				// generate random number to decide reordering
				double u = (double)rand()/RAND_MAX; 	// u in [0,1]

				// reorder with probability P_REORDER, but only if the queue is non-empty
				// note: this code can reorder the same packet multiple times, but each time by one position
				// to disable multiple reorderings of the same packet, uncomment the statement below
				if (u < p_reorder && Q.size() > 0)// && head.has_been_reordered == false)	
				{
					m->printf ("[%f] Network: reordered pkt of size %d, message %d\n", m->ElapsedTime(), head.pkt_size, *(int*)head.pkt_data);
					head.has_been_reordered = true;

					// swap this packet with the next one
					Packet tmp;
					// save the current packet
					memcpy (&tmp, &head, sizeof (Packet));
					// take the next one
					head = Q.front();
					Q.pop_front();
					// store the old one in its place
					Q.push_front (tmp);
				}

				// printf takes 1.9 ms to execute, this will normally delay transmission of packets that follow if the sending rate is high;
				// disable all printfs for high-speed transfers, or they will become the bottleneck
				m->printf ("[%f] Network: sent message %d, scheduled %f\n", m->ElapsedTime(), *(int*)head.pkt_data, m->RelativeTime(head.d));

				// send to destination
				sendto (sock_out, head.pkt_data, head.pkt_size, 0, 
					(struct sockaddr*) &destination, sizeof(struct sockaddr_in));

				delete head.pkt_data;
				
				// if there is still stuff in the queue, see if it needs to be transmitted
				if (Q.size() > 0)
					head = Q.front();		// peek at the next packet
				else
					break;
			}
		}
		else	// data in socket
		{
			int bytes = recv (sock_in, buf, MAX_PKT_SIZE, 0);

			if (bytes == SOCKET_ERROR)
			{
					m->printf ("[%f] Network: recvfrom error %d\n", m->ElapsedTime(), WSAGetLastError());
					exit (-1);
			}
			else
			{
				// lose this pkt with probability p_loss
				double u = (double)rand()/RAND_MAX; 	// u in [0,1]
				
				if (u >= p_loss)			
				{
					// keep the packet
					Packet new_pkt;

					// create a new pkt entry
					new_pkt.pkt_data = new char [bytes];
					memcpy (new_pkt.pkt_data, buf, bytes);
					new_pkt.pkt_size = bytes;
					new_pkt.has_been_reordered = false;

					// add it to the queue
					PushInNetworkQueue (new_pkt);
				}
				else
					m->printf ("[%f] Network: dropped pkt of size %d, message %d\n", m->ElapsedTime(), bytes, *(int*)buf);
			}
		}

		// recompute the sleep delay
		if (Q.size() > 0)	
		{
			// if the queue is not empty, figure out how much to sleep
			double cur_time = m->GetTime();
			if (Q.front().d > cur_time)
				timeout = (DWORD)floor( (Q.front().d - cur_time) * 1000.0 + 0.5);		// round to the nearest integer ms
			else		// do not sleep
				timeout = 0;
		}
		else			// queue is empty, sleep forever
			timeout = INFINITE;
	}
}

void Network::PushInNetworkQueue (Packet pkt)
{
	// we first add the propagation delay to the current time
	// departure time (seconds): current time plus half the RTT (emulates one-way delay)
	double departure = m->GetTime() + (rtt/2.0)/1000.0;		

	// transmission delay is simply packet size divided by link capacity
	double transmission_delay = (double) pkt.pkt_size * 8.0 / link_capacity;
	
	// generate a random queuing delay in [0, MAX_Q_DELAY]
	double q_delay = (double)rand() / RAND_MAX * max_q_delay / 1000.0;			// in seconds

	// finally make sure FIFO ordering is preserved
	if (Q.size() > 0) 
	{
		// get the last element in Q without removing it from Q
		Packet old = Q.back();		
		
		// new departure time preserves FIFO ordering; add transmission & queuing delays
		departure = max(departure + transmission_delay + q_delay, old.d + transmission_delay + q_delay);	
	}
	else
	{
		// if the queue is empty, directly add both delays
		departure += transmission_delay + q_delay;
	}

	pkt.d = departure;			// add queuing delay = departure time for new pkt
	
	//printf ("departure %f\n", p->RelativeTime(pkt.d));

	// finally push the packet into Q
	Q.push_back (pkt);
}
