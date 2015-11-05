#include "messenger.h"

Messenger::Messenger( HANDLE printMutex )
{
	this->printMutex = printMutex;
	QueryPerformanceFrequency(&freq);	  //freq gets the "current" performance-counter frequency, in counts per second
	QueryPerformanceCounter(&start_time); //start_time receives the "current" performance-counter value, in counts	
	//start_time = timeGetTime();
}

void Messenger::printf (const char* format, ...)
{
	va_list ap;
	va_start(ap, format);

	// get string length
	int required_length = _vscprintf(format, ap) + 1;  
	char *buf = new char [required_length]; 

	// assemble the string
	vsnprintf_s (buf, required_length, required_length, format, ap);
	va_end(ap);

	WaitForSingleObject(printMutex, INFINITE);
	::printf ("%s", buf);	// call the usual printf from stdio.h
	ReleaseMutex (printMutex);
	delete buf;
}

double Messenger::ElapsedTime(void)
{
	LARGE_INTEGER tmp;
	QueryPerformanceCounter(&tmp);	
	return (double)(tmp.QuadPart - start_time.QuadPart) / freq.QuadPart;
}

double Messenger::GetTime(void)
{
	LARGE_INTEGER tmp;
	QueryPerformanceCounter(&tmp);	
	return (double) tmp.QuadPart / freq.QuadPart;
}

double Messenger::RelativeTime(double cur)
{
	return cur - (double) start_time.QuadPart / freq.QuadPart;
}
