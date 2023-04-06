/*
The MIT License (MIT)

Copyright © 2023 Zeitgeist Eater

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and 
 * associated documentation files (the “Software”), to deal in the Software without restriction, 
 * including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, 
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, 
 * subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial 
 * portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT 
 * LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. 
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, 
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE 
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

//#define DISABLEMEMSAFETY
#define TESTUNITS

#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "./util/memsafety.h"
#include "./util/linkedlist.h"
#include "./util/bigmath.h"
#ifdef TESTUNITS
#include "./unit.tests/main.cpp"
#endif

#ifdef TESTUNITS
int appmain(int argc, char **argv)
{
	testunits::start();	

	printf("Unit tests successful.\n");

	return(0);
}
#else
int appmain(int argc, char **argv)
{
	printf("Done.");
	return(0);
}
#endif

#ifndef NDEBUG
#include <iostream>
extern "C" int __lsan_is_turned_off() { return 1; }
#endif
int main(int argc, char **argv) {
	int ret = appmain(argc,argv);
	__memsafe_pk::memleakcheck();
	#ifndef NDEBUG
		printf("Press enter to exit.\n");
		getchar();
	#endif
	return(ret);
}