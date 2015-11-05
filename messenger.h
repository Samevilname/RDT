/* messenger.h
Define a class that prints messenges on screen from threads
*/
#pragma once 
#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <time.h>

class Messenger {
	HANDLE				printMutex;
	LARGE_INTEGER		freq;			//used for hi-performance timer
	LARGE_INTEGER		start_time;     //used for hi-performance timer

public:
	Messenger (HANDLE printMutex);
	void	printf (const char* format, ...);
	double	ElapsedTime (void);
	double	GetTime (void);
	double	RelativeTime (double time);
};
