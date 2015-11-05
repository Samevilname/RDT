/* Reliable Data Transfer
main.cpp
*/
#include<iostream>
#include "transport.h"
#include <string>
using namespace std;
int main(int argc, char argv[])
{
	WSADATA wsaData;

	//Initialize WinSock 
	WORD wVersionRequested = MAKEWORD(2,2);
	if (WSAStartup(wVersionRequested, &wsaData) != 0) {
		printf("WSAStartup error %d\n", WSAGetLastError ());
		WSACleanup();	
		return -1;
	}

	DWORD t = timeGetTime();
	// create a mutex for print-outs
	HANDLE printMutex = CreateMutex (NULL, false, NULL);
	// declare the Messenger class  that can print messages on screen from multiple threads
	Messenger m (printMutex);	

	// this is signaled by the receiver when its ports are all set up
	HANDLE receiver_ready = CreateEvent (NULL, false, false, NULL);
	
	// choose protocol
	int protocol;
	//string filename;
	cout << "Select the protocol to use:" << endl << "0) rdt 3.0" << endl << "1) GBN" << endl << "2) Simplified TCP" << endl;
	cin >> protocol;
	/*cout << "Please input file name" << endl;
	cin >> filename;*/
	// set up parameters for the sender thread
	TransportParameters sp;
	sp.filename = "test.txt";
	sp.base_port = 8000;	// some large random number
	sp.m = &m;
	sp.receiver_ready = receiver_ready;		// use the handle receiver_ready 
	if (protocol == 0){
		sp.protocol = RDT3;
		sp.isFinished = CreateEvent(NULL, false, false, NULL);  // is signaled when the sender finishes
		HANDLE sender = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)SenderThread, &sp, 0, NULL);

		// set up parameters for the receiver Thread
		TransportParameters rp;
		rp.base_port = sp.base_port;
		rp.m = &m;
		rp.filename = "test-out.txt";
		rp.receiver_ready = receiver_ready;		// use the same handle receiver_ready 
		rp.protocol = sp.protocol;
		rp.isFinished = CreateEvent(NULL, false, false, NULL);  // is signaled when the receiver finishes
		HANDLE recv = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ReceiverThread, &rp, 0, NULL);

		// hang until both SenderThread and ReceiverThread are finished
		WaitForSingleObject(sp.isFinished, INFINITE);
		WaitForSingleObject(rp.isFinished, INFINITE);

		CloseHandle(sender);
		CloseHandle(recv);
	}
	else if (protocol == 1){
		sp.protocol = GBN;
		sp.isFinished = CreateEvent(NULL, false, false, NULL);  // is signaled when the sender finishes
		HANDLE sender = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)GBNSenderThread, &sp, 0, NULL);

		// set up parameters for the receiver Thread
		TransportParameters rp;
		rp.base_port = sp.base_port;
		rp.m = &m;
		rp.filename = "test-out.txt";
		rp.receiver_ready = receiver_ready;		// use the same handle receiver_ready 
		rp.protocol = sp.protocol;
		rp.isFinished = CreateEvent(NULL, false, false, NULL);  // is signaled when the receiver finishes
		HANDLE recv = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)GBNReceiverThread, &rp, 0, NULL);

		// hang until both SenderThread and ReceiverThread are finished
		WaitForSingleObject(sp.isFinished, INFINITE);
		WaitForSingleObject(rp.isFinished, INFINITE);

		CloseHandle(sender);
		CloseHandle(recv);
	}
	else{
		sp.protocol = STCP;
		sp.isFinished = CreateEvent(NULL, false, false, NULL);  // is signaled when the sender finishes
		HANDLE sender = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)STCPSenderThread, &sp, 0, NULL);

		// set up parameters for the receiver Thread
		TransportParameters rp;
		rp.base_port = sp.base_port;
		rp.m = &m;
		rp.filename = "test-out.txt";
		rp.receiver_ready = receiver_ready;		// use the same handle receiver_ready 
		rp.protocol = sp.protocol;
		rp.isFinished = CreateEvent(NULL, false, false, NULL);  // is signaled when the receiver finishes
		HANDLE recv = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)STCPReceiverThread, &rp, 0, NULL);

		// hang until both SenderThread and ReceiverThread are finished
		WaitForSingleObject(sp.isFinished, INFINITE);
		WaitForSingleObject(rp.isFinished, INFINITE);

		CloseHandle(sender);
		CloseHandle(recv);
	}
	

	printf ("Terminating main(), completion time %d ms\n", timeGetTime()-t);
	system("PAUSE");
	return 0;
}
