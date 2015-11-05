#include "transport.h"
#include "network.h"
#include <fstream>
#include <string>
#include <iostream>

// this function is where the sender thread starts
UINT GBNSenderThread(LPVOID pParam)
{
	// copy parameters and start the sender
	TransportParameters *sp = ((TransportParameters*)pParam);
	Sender s(sp);
	cout << "Fixed woindow size(pkts)[100]:";
	cin >> sp->window;
	cout << "Fixed timeout(ms) [2000]:";
	cin >> sp->timeout;
	cout << "Initial sequence number [0]";
	cin >> sp->seqn;
	s.GBNBegin();
	sp->m->printf("------- terminating SenderThread --------\n");
	cout << "Using GBN with window size " << sp->window << "packets" << endl;
	cout << "Packet size " << MAX_PKT_SIZE << "bytes" << endl;
	cout << "Retransmission timeout " << sp->timeout << "ms" << endl;
	cout << "Number of timeouts: " << sp->num_of_timeout << endl;
	cout << "Total retransmitted packets: " << sp->num_of_repacket << endl;
	cout << "Duplicate packets: " << sp->num_of_dupacket << endl;
	SetEvent(sp->isFinished);
	return 0;
}

// this function is where the receiver thread starts
UINT GBNReceiverThread(LPVOID pParam)
{
	// copy parameters and start the receiver
	TransportParameters *rp = ((TransportParameters*)pParam);
	Receiver r(rp);

	r.GBNBegin();

	rp->m->printf("------- terminating ReceiverThread --------\n");
	SetEvent(rp->isFinished);

	return 0;
}

// ------------------------------------------------------------------------
void Sender::GBNBegin(void)
{
	// start sender-side network thread
	NetworkParameters np;
	np.in_port = sp->base_port + 1;
	np.dest_port = sp->base_port + 2;
	np.m = sp->m;
	np.wait_init = CreateEvent(NULL, false, false, NULL);
	np.protocol = sp->protocol;

	CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)NetworkThread, &np, 0, NULL);
	// block here until the Network is ready; otherwise, some packets may be dropped as the network socket isn't open yet
	WaitForSingleObject(np.wait_init, INFINITE);
	sp->m->printf("Sender: started the network thread\n");
	CloseHandle(np.wait_init);

	// wait for the receiver and its network thread to initialize
	WaitForSingleObject(sp->receiver_ready, INFINITE);
	sp->m->printf("Sender: ready to transmit\n");


	HANDLE dataReady = CreateEvent(NULL, false, false, NULL);
	WSAEventSelect(sock, dataReady, FD_READ);
	DWORD timeout = sp->timeout;

	int maxsize = MAX_RDTdata_SIZE - 32;
	int window = sp->window;
	sp->m->printf("Sending %s\n", sp->filename);
	sp->m->printf("the window size %d under GBN protocol\n", sp->window);
	sp->m->printf("Packet size %d bytes\n", maxsize);
	sp->m->printf("Retransmission timeout %d ms\n", timeout);
	ifstream ffile(sp->filename);
	if (!ffile.is_open())////
	{
		cout << "Open error, unable to open this  file!" << endl;
		return;
	}


	int pktsize = sizeof(RdtHeader) + 1;
	string sendstring = "";
	char *send_buf = new char[pktsize];
	char c;
	string payload;
	int bytes = 0;
	int seqq = sp->seqn;
	int base = seqq;
	int next = base;

	RdtHeader *rdt_header = (RdtHeader *)send_buf;
	rdt_header->ack = 0;
	rdt_header->fin = 0;
	rdt_header->ok = 0;
	rdt_header->protocol = sp->protocol;
	rdt_header->seq = next;
	rdt_header->syn = 1;
	rdt_header->win = sp->window;
	send_buf[pktsize - 1] = 0;
	do{
		bytes = 0;
		sendto(sock, send_buf, pktsize, 0, (struct sockaddr*)&send_addr, sizeof(struct sockaddr_in));
		sp->num_of_timeout++;
		sp->num_of_repacket++;
		if (WaitForSingleObject(dataReady, timeout) == WAIT_OBJECT_0)
		{
			if ((bytes = recv(sock, send_buf, sizeof(RdtHeader), 0))>0)
			{
				sp->m->printf("[%f] Sender got ACK %d\n", sp->m->ElapsedTime(), rdt_header->seq);
			}
			else
				printf("Error on recv %d\n", WSAGetLastError());
		}
	} while (bytes <= 0 || rdt_header->ok == 0);
	sp->num_of_timeout--;
	sp->num_of_repacket--;
	base++;
	next++;
	int state = 0;
	while (!ffile.eof()){
		ffile.get(c);
		sendstring = sendstring + c;
		sp->filesize++;
		if (sendstring.size() >= maxsize){
			if (state == 0)
			{
				payload = sendstring.substr(0, maxsize);
				sendstring = sendstring.substr(maxsize);
				pktsize = payload.size() + sizeof(RdtHeader) + 1;
				send_buf = new char[pktsize];

				rdt_header = (RdtHeader *)send_buf;
				rdt_header->ack = 0;
				rdt_header->fin = 0;
				rdt_header->ok = 0;
				rdt_header->protocol = sp->protocol;
				rdt_header->seq = next;
				rdt_header->win = window;
				rdt_header->syn = 0;

				memcpy(send_buf + sizeof(RdtHeader), payload.c_str(), payload.size());
				send_buf[pktsize - 1] = 0;
				sendto(sock, send_buf, pktsize, 0, (struct sockaddr*)&send_addr, sizeof(struct sockaddr_in));
				sp->m->printf("[%f]Sender is sending packet %d\n", sp->m->ElapsedTime(), rdt_header->seq);
				next++;
				if (window < next - base + 1){
					sp->m->printf("use up sending window!\n");
					state = 1;
				}
			}
			else if (state == 1)
			{
				bytes = 0;
				sp->m->printf("waiting for receiver......\n");
				if (WaitForSingleObject(dataReady, timeout) == WAIT_OBJECT_0){
					if ((bytes = recv(sock, send_buf, sizeof(RdtHeader), 0))>0)
					{
						sp->m->printf("[%f]Sender got ACK %d\n", sp->m->ElapsedTime(), rdt_header->ack);
						if (rdt_header->ack == base)
						{
							state = 0;
							base++;
						}
						else
						{
							sp->num_of_repacket += next - base;
							next = base;
							state = 0;
						}
					}
					else
						printf("Error on recv %d\n", WSAGetLastError());
				}
				else
				{
					sp->m->printf("Timeout!\n");
					sp->num_of_repacket += next - base;
					next = base;
					sp->num_of_timeout++;
					state = 0;
				}
			}
		}

	}
	ffile.close();

	payload = sendstring;
	pktsize = payload.size() + sizeof(RdtHeader) + 1;
	send_buf = new char[pktsize];

	rdt_header = (RdtHeader *)send_buf;
	rdt_header->ack = 0;
	rdt_header->fin = 1;
	rdt_header->ok = 0;
	rdt_header->seq = next;
	rdt_header->syn = 0;
	rdt_header->win = window;
	sp->num_of_timeout--;
	sp->num_of_repacket--;
	memcpy(send_buf + sizeof(RdtHeader), payload.c_str(), payload.size());
	send_buf[pktsize - 1] = 0;

	do{
		bytes = 0;
		sp->m->printf("last packet\n");
		sendto(sock, send_buf, pktsize, 0, (struct sockaddr*)&send_addr, sizeof(struct sockaddr_in));
		sp->num_of_timeout++;
		sp->num_of_repacket++;
		if (WaitForSingleObject(dataReady, timeout) == WAIT_OBJECT_0){
			if ((bytes = recv(sock, send_buf, sizeof(RdtHeader), 0))>0){
				sp->m->printf("[%f] Sender got ACK %d\n", sp->m->ElapsedTime(), rdt_header->ack);
			}
			else
				printf("Error on recv %d\n", WSAGetLastError());
		}
	} while (bytes <= 0);

	delete send_buf;


}

// ------------------------------------------------------------------------
void Receiver::GBNBegin(void)
{
	// start receiver-side network thread
	NetworkParameters np;
	np.in_port = rp->base_port + 3;
	np.dest_port = rp->base_port + 4;
	np.m = rp->m;
	np.wait_init = CreateEvent(NULL, false, false, NULL);
	np.protocol = rp->protocol;
	CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)NetworkThread, &np, 0, NULL);

	// wait for the network thread to finish; otherwise we may start losing ACKs unnecessarily
	WaitForSingleObject(np.wait_init, INFINITE);
	CloseHandle(np.wait_init);

	rp->m->printf("Receiver: started network\n"); 

	SetEvent(rp->receiver_ready);
	ofstream ofile;
	ofile.open(rp->filename);

	char recv_buf[MAX_PKT_SIZE];
	HANDLE dataReady = CreateEvent(NULL, false, false, NULL);

	WSAEventSelect(sock, dataReady, FD_READ);
	DWORD timeout = INFINITE;

	RdtHeader *rdt_header = (RdtHeader*)recv_buf;

	int end = rp->seqn - 1;
	int output;
	bool terminate = false;
	while (!terminate){
		output = WaitForSingleObject(dataReady, timeout);
		if (output == WAIT_TIMEOUT){

		}
		else{
			int bytes = recvfrom(sock, recv_buf, MAX_PKT_SIZE, 0, 0, 0);
			if (bytes > 0)
			{
				ofile << recv_buf + RDT_HDR_SIZE;
				if (rdt_header->seq != end + 1)
				{
					rp->m->printf("[%f] Receiver: got message %d, ------------------------------ out of order\n", rp->m->ElapsedTime(), rdt_header->seq);
					rp->num_of_dupacket++;
					rdt_header->ack = end;
				}
				else
				{
					rp->m->printf("[%f] Receiver: got message %d\n", rp->m->ElapsedTime(), rdt_header->seq);
					end++;
					rdt_header->ack = end;
				}
				if (rdt_header->syn == 1)
					rdt_header->ok = 1;
				if (rdt_header->fin == 1){
					rp->m->printf("receive last packet\n");
					terminate = true;
					rdt_header->ok = 1;
					sendto(sock, recv_buf, sizeof(RdtHeader), 0, (sockaddr*)&send_addr, sizeof(struct sockaddr_in));
					Sleep(10000);
				}
				else{
					sendto(sock, recv_buf, sizeof(RdtHeader), 0, (sockaddr*)&send_addr, sizeof(struct sockaddr_in));
				}
			}
		}

	}
}
