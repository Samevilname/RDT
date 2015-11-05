/* 
transport.h  
*/

#pragma once
#include "winsock.h"
#include "messenger.h"

//These two functions are where the sender/receiver threads start
UINT SenderThread(LPVOID pParam);
UINT ReceiverThread(LPVOID pParam);
UINT GBNSenderThread(LPVOID pParam);
UINT GBNReceiverThread(LPVOID pParam);
UINT STCPSenderThread(LPVOID pParam);
UINT STCPReceiverThread(LPVOID pParam);

#define MAX_PKT_SIZE		1500
#define RDT_HDR_SIZE		12  //rdt headers are 12 bytes
//payload size of application data
#define MAX_RDTdata_SIZE			(MAX_PKT_SIZE - RDT_HDR_SIZE)		 
// two-bit field in the header
#define RDT3		0
#define GBN			1
#define STCP		2   //simplied TCP

//this class defines the RDT header 
class RdtHeader{	// 12-byte header 
public: 
	//1-byte flags field
	u_char protocol:2;		// 2 bits: protocol type = 0 for RDT3, 1 for GBN, and 2 for STCP	
	u_char syn:1;			// 1 bit: SYN = 1 for connection setup	
	u_char fin:1;			// 1 bit: FIN = 1 for termination
	u_char ok:1;			// 1 bit: OK = 1 receiver agrees, SYN_OK or FIN_OK
	u_char reserved:3;		// 3 bits: unused
	
	u_char unused; 			// 1-byte unused filed; 

	u_short win;			// 2-byte receiver window size (the number of packets)
	u_long seq;				// 4-byte sequence number
	u_long ack;				// 4-byte ack number
}; 

// this class is passed to the Sender thread and Receiver thread, acts as shared memory
class TransportParameters{
public:
	const char	*filename;			// file to send/receive
	int			base_port;			// base port #, from which all other port #s are derived 
	Messenger	*m;					// Messenger class
	int			protocol;			// user-selected protocol
	HANDLE		receiver_ready;		// event signaled when receiver has finished initialization
	HANDLE		isFinished;			// event signaled when sender/receiver finishes 
	DWORD timeout;
	int seqn;
	int window;
	int num_of_timeout = -1;
	int num_of_repacket = -1;
	int num_of_dupacket = 0;
	int filesize = 0;
};

//the class for the Sender thread
class Sender{
public:
	int windowsize = 100, Isequencenum = 0;
	DWORD timeout = 2000;
	int timeoutnum = 0, totalretranpkt = 0, duppkt = 0;
	TransportParameters	*sp;		// initial parameters
	SOCKET				sock;		// socket used for receiving ACKs and sending data
	struct sockaddr_in	send_addr;	// address where to send

	Sender (TransportParameters *sp);
	~Sender ();
	void	Begin (void);			// main loop
	void STCPBegin(void);
	void GBNBegin(void);
	// define functions you need!
	
};

class Receiver{
public:
	TransportParameters	*rp;		// initial parameters
	SOCKET				sock;		// socket used for receiving data and sending ACKs
	struct sockaddr_in	send_addr;	// address where to send

	Receiver(TransportParameters *rp);
	~Receiver();
	void	Begin (void);// main loop
	void STCPBegin(void);
	void GBNBegin(void);
};
