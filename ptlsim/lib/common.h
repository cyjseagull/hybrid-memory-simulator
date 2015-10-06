#ifndef _COMMON_H_
#define _COMMON_H_

#include <iostream>
#include <string>
#include <stdarg.h>
#include <stdint.h>
#include <vector>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "global.h"
#define DEBUG

inline void fatal( std::string format_str , ...)
{
   format_str = "Fatal: " + format_str + "\n";
   va_list ap;
   va_start(ap , format_str);	//ap point to argv after format_str
   vfprintf(stderr , format_str.c_str(),ap);
   va_end(ap);	//assign ap to NULL
   exit(-1);
 }

inline void warning( std::string format_str , ...)
{
	format_str = "Warning: "+format_str + "\n";						    va_list ap;

	va_start(ap , format_str);	//ap point to argv after format_str

	vfprintf(stdout , format_str.c_str(),ap);

	va_end(ap);	//assign ap to NULL

}

inline void debug_printf( std::string format_str , ...)
{
	#ifdef DEBUG
	format_str = "Debug:" + format_str+"\n";
	va_list parg;
	va_start(parg , format_str);
	vfprintf( stdout , format_str.c_str() , parg );
	va_end(parg);
	#endif																
}


