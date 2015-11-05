/* 
winsock.h
Define a class that handles socket APIs
*/

#pragma once  
#include "messenger.h"

class Winsock {
	Messenger	 *m;  //class Messenger
public:
	Winsock (Messenger *m);
	SOCKET	OpenSocket (int port); 
};

