/* 
transport.cpp
*/

#include "transport.h"
#include "network.h"
#include <fstream>
#include <string>
#include <iostream>

// this function is where the sender thread starts
UINT SenderThread(LPVOID pParam)
{
	// copy parameters and start the sender
	TransportParameters *sp = ((TransportParameters*)pParam);
	Sender s (sp);
	cout << "Fixed timeout(ms) [2000]:";
	cin >> s.timeout;
	s.Begin();

	sp->m->printf( "------- terminating SenderThread --------\n"); 
	cout << "Using GBN with window size " << s.windowsize << "packets" << endl;
	cout << "Packet size " << MAX_PKT_SIZE << "bytes" << endl;
	cout << "Retransmission timeout " << s.timeout << "ms" << endl;
	cout << "Number of timeouts: " << s.timeoutnum << endl;
	cout << "Total retransmitted packets: " << s.totalretranpkt << endl;
	cout << "Duplicate packets: " << s.duppkt << endl;
	SetEvent(sp->isFinished); 
	return 0;
}

// this function is where the receiver thread starts
UINT ReceiverThread(LPVOID pParam)
{
	// copy parameters and start the receiver
	TransportParameters *rp = ( (TransportParameters*)pParam );
	Receiver r (rp);

	r.Begin();
	
	rp->m->printf( "------- terminating ReceiverThread --------\n"); 
	SetEvent(rp->isFinished); 

	return 0;
}

// ------------------------------------------------------------------------

Sender::Sender (TransportParameters *sp)
{
	this->sp = sp;

	// open sending port
	Winsock rdt_sock (sp->m);
	sock = rdt_sock.OpenSocket (sp->base_port + 4);	// bound to port "base + 4, listening for incoming ACKs

	// set up the address of where we're sending data
	send_addr.sin_family = AF_INET;
	send_addr.sin_addr.S_un.S_addr = inet_addr ("127.0.0.1");	// localhost 
	send_addr.sin_port = htons (sp->base_port + 1);	
}

Sender::~Sender ()
{
	CloseHandle( sp->isFinished ); 
	CloseHandle( sp->receiver_ready ); 
}

void Sender::Begin (void)
{
	// start sender-side network thread
	NetworkParameters np;
	np.in_port = sp->base_port + 1;
	np.dest_port = sp->base_port + 2;
	np.m = sp->m;
	np.wait_init = CreateEvent (NULL, false, false, NULL);
	np.protocol = sp->protocol;

	CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)NetworkThread, &np, 0, NULL);
	// block here until the Network is ready; otherwise, some packets may be dropped as the network socket isn't open yet
	WaitForSingleObject (np.wait_init, INFINITE);
	sp->m->printf ("Sender: started the network thread\n");
	CloseHandle (np.wait_init);

	// wait for the receiver and its network thread to initialize
	WaitForSingleObject (sp->receiver_ready, INFINITE);
	sp->m->printf ("Sender: ready to transmit\n");
	

	// ------- now ready to transmit packets -------------------

	// an alternative to select() is to use Windows events; first create a regular auto-reset event
	HANDLE dataReady =  CreateEvent (NULL, false, false, NULL);
	// second, register sock_in with the event using FD_READ notifications only (i.e., available for READ)
	WSAEventSelect(sock, dataReady, FD_READ);	// associate event with socket
/*	DWORD timeout = INFINITE;	*/				//infinite timeout

	ifstream packet(sp->filename);
	string line, outline = "";
	while (getline(packet, line)){
		outline += line;
	}
	const char *oldchar = outline.c_str();
	int maximum_size = outline.length() + 1;
	int payload_size = MAX_PKT_SIZE - sizeof(RdtHeader);
	int i = maximum_size / payload_size + 1;
	char **payload = new char*[i];
	for (int j = 0; j < i; j++){
		payload[j] = new char[payload_size];
	}
	for (int j = 0; j < i; j++){
		for (int k = 0; k < payload_size; k++){
			payload[j][k] = oldchar[j*payload_size + k];
		}
	}
	int pktsize = MAX_PKT_SIZE;
	char *send_buf = new char[MAX_PKT_SIZE];
	int last = 0;
	char recv_buf[MAX_PKT_SIZE];
	RdtHeader *rdt_headerrecv = (RdtHeader *)recv_buf;
	RdtHeader *rdt_header = (RdtHeader *)send_buf;

	// set rdt fields:
	rdt_header->protocol = sp->protocol;
	rdt_header->ack = 1;
	rdt_header->seq = 0;
	rdt_header->syn = 0;
	rdt_header->fin = 0;
	// ...
	memcpy(send_buf + sizeof(RdtHeader), payload[0], payload_size);
	// ------ end of creating a packet ------------------
	sp->m->printf("send_buf %s \n", send_buf);
	sendto(sock, send_buf, pktsize, 0, (struct sockaddr*) &send_addr, sizeof(struct sockaddr_in));

	// ------------------- recv ACKs from receiver --------------------------------
	timeout = 1000;
		// wait for each ACK, then print its value; NOTE: when using WSAEventSelect, the socket is automatically non-blocking
	while (true){
		if (WaitForSingleObject(dataReady, timeout) == WAIT_OBJECT_0){
			int bytes;
			// TODO: check for errors, size of received packet, etc.
			if ((bytes = recv(sock, recv_buf, MAX_PKT_SIZE, 0)) > 0){
				int ACK = rdt_headerrecv->ack;
				int SEQ = rdt_headerrecv->seq;
				if (ACK == i && SEQ == 1){
					sp->m->printf("[%f] Sender got ACK %d\n", sp->m->ElapsedTime(), ACK);
					rdt_header->fin = 1;
					sendto(sock, send_buf, pktsize, 0, (struct sockaddr*) &send_addr, sizeof(struct sockaddr_in));
					break;
				}
				else if (ACK == last + 1 && SEQ == 0){
					sp->m->printf("[%f] Sender got ACK %d\n", sp->m->ElapsedTime(), ACK);
					rdt_header->seq = 1;
					rdt_header->ack = last + 1;
					rdt_header->fin = 0;
					sendto(sock, send_buf, pktsize, 0, (struct sockaddr*) &send_addr, sizeof(struct sockaddr_in));
				}	
				else if (ACK == last + 1 && SEQ == 1){
					sp->m->printf("[%f] Sender got ACK %d\n", sp->m->ElapsedTime(), ACK);
					last++;
					rdt_header->ack = last + 1;
					rdt_header->seq = 0;
					rdt_header->fin = 0;
					memcpy(send_buf + sizeof(RdtHeader), payload[last], payload_size);
					sendto(sock, send_buf, pktsize, 0, (struct sockaddr*) &send_addr, sizeof(struct sockaddr_in));
				}
				else if (ACK != last + 1){
					rdt_header->fin = 0;
					sendto(sock, send_buf, pktsize, 0, (struct sockaddr*) &send_addr, sizeof(struct sockaddr_in));
					totalretranpkt++;
				}
				
			}
			else
				printf("Error on recv %d\n", WSAGetLastError());
		}
		else{
			timeoutnum++;
			totalretranpkt++;
			sendto(sock, send_buf, pktsize, 0, (struct sockaddr*) &send_addr, sizeof(struct sockaddr_in));
		}
			
	}
	delete send_buf; 	
	closesocket (sock);	
	sp->m->printf (" end of Sender::Begin() \n");
}

// ------------------------------------------------------------------------

Receiver::Receiver (TransportParameters *rp)
{
	this->rp = rp;
	// open receiving port
	Winsock rdt_sock (rp->m);
	sock = rdt_sock.OpenSocket (rp->base_port + 2);			// bound to port base + 2, listening for incoming data
	
	// set up the address of where we're sending ACKs
	send_addr.sin_family = AF_INET;
	send_addr.sin_addr.S_un.S_addr = inet_addr ("127.0.0.1");
	send_addr.sin_port = htons (rp->base_port + 3);	
}

Receiver::~Receiver ()
{
	CloseHandle( rp->isFinished ); 
	CloseHandle( rp->receiver_ready );
}

void Receiver::Begin (void)
{
	// start receiver-side network thread
	NetworkParameters np;
	np.in_port = rp->base_port + 3;
	np.dest_port = rp->base_port + 4;
	np.m = rp->m;
	np.wait_init = CreateEvent (NULL, false, false, NULL);
	np.protocol = rp->protocol;
	CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)NetworkThread, &np, 0, NULL);

	// wait for the network thread to finish; otherwise we may start losing ACKs unnecessarily
	WaitForSingleObject (np.wait_init, INFINITE);
	CloseHandle (np.wait_init);

	rp->m->printf ("Receiver: started network\n");

	// allow sender to start transmitting; all sockets are in place
	SetEvent (rp->receiver_ready);

	// ---------------------- ready to receive data from sender ----------------------------
	// an alternative to select() is to use Windows events; first create a regular auto-reset event
	HANDLE dataReady =  CreateEvent (NULL, false, false, NULL);
	// second, register sock_in with the event using FD_READ notifications only (i.e., available for READ)
	WSAEventSelect(sock, dataReady, FD_READ);	// associate event with socket
	DWORD timeout = INFINITE;			// infinite timeout
	//DWORD timeout = 1000;
	ofstream packet(rp->filename);
	char recv_buf [MAX_PKT_SIZE];
	char send_buf[MAX_PKT_SIZE];
	RdtHeader *rdt_headersend = (RdtHeader *)send_buf;
	RdtHeader *rdt_header = (RdtHeader *) recv_buf; 
	//int result = WaitForSingleObject (dataReady, timeout);// either a timeout occurs or something is pending inside sock_in
	while (true){
		if (WaitForSingleObject(dataReady, timeout) == WAIT_OBJECT_0){
			int bytes = recvfrom(sock, recv_buf, MAX_PKT_SIZE, 0, 0, 0);
			if (bytes > 0) {
				rp->m->printf("recv data from sender: \n %s\n", recv_buf);
				rp->m->printf("recv protocol: %d\n", rdt_header->protocol);
			}
			// send ACKs
			int ACK = rdt_header->ack;
			int SEQ = rdt_header->seq;
			int SYN = rdt_header->syn;
			int FIN = rdt_header->fin;
			rdt_headersend->ack = ACK;
			rdt_headersend->seq = SEQ;
			sendto(sock, send_buf, MAX_PKT_SIZE, 0, (sockaddr*)&send_addr, sizeof(struct sockaddr_in));
			if (SEQ == 1){
				int i = sizeof(RdtHeader);
				while (recv_buf[i] != '\0'){
					packet << recv_buf[i];
					i++;
				}
			}
			if (FIN == 1){
				Sleep(1000);
				break;
			}
		}
		else 
			break;
	}
	//if (WaitForSingleObject(dataReady, timeout) == WAIT_OBJECT_0)
	//{
	//	int bytes = recv(sock, recv_buf, sizeof(int), 0);
	//	if (bytes > 0)
	//	{
	//		// respond with an ACK
	//		ACK = message;
	//		sendto(sock, (char*)&ACK, sizeof(int), 0, (sockaddr*)&send_addr, sizeof(struct sockaddr_in));

	//		last = message;
	//	}
	//}
	//else
	//	break;

///*
	// debug section: receive test packets
	//int last = 0;
	//timeout = 1000; 
	//while (true)	
	//{
	//	int message;
	//	int ACK;

	//	// TODO: check for errors, verify the size of received packet, etc.
	//	if ( WaitForSingleObject (dataReady, timeout) == WAIT_OBJECT_0 )
	//	{
	//		int bytes = recv (sock, (char*) &message, sizeof(int), 0);
	//		if (bytes > 0)
	//		{
	//			if (message != last + 1)
	//				rp->m->printf ("[%f] Receiver: got message %d, ********* out of order\n", rp->m->ElapsedTime(), message);
	//			else
	//				rp->m->printf ("[%f] Receiver: got message %d\n", rp->m->ElapsedTime(), message);

	//			// respond with an ACK
	//			ACK = message;
	//			sendto (sock, (char*) &ACK, sizeof(int), 0, (sockaddr*) &send_addr, sizeof(struct sockaddr_in));

	//			last = message;
	//		}
	//	}
	//	else
	//		break;
	//}
	// debug section: end
//*/ 

	
	closesocket (sock);
	rp->m->printf (" end of Receiver::Begin() \n" );



}



