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

#include <cassert>

namespace testunits {

	void testbigmath() {

		//basic big uint test
		{
			biguint16384_t v1, v2(2);
			v1=2;
			assert(v1==v2);
			assert(v1>=v2);
			assert(v1<=v2);
			assert(!(v1<v2));
			assert(!(v1>v2));
			
			v1=3;
			assert(v1>v2);
			assert(v1>=v2);
			assert(!(v1==v2));
			assert(!(v1<=v2));
			assert(!(v1<v2));

			assert(strcmp((const char*)v1,"3")==0);
			assert((double)v1==3.0);
			assert((unsigned int)v1==3);
		}

		//...others...

		//fractional rounding
		{
			bigfrac16384_t v1;
			v1 = -22.25;
			assert(strcmp((const char*)v1,"-89/4")==0);
			bigint16384_t rounded=v1.round();
			assert(strcmp((const char*)rounded,"-22")==0);
		}

		//global routines (rough handling of temp objects straight to strings)
		{
			assert(strcmp((const char*)bigint128_t::gcd(6,9),"3")==0);
			assert(strcmp((const char*)bigint128_t::lcm(6,9),"18")==0);
			assert(strcmp((const char*)bigint128_t::nextprime(14),"17")==0);
		}
		
	}
	
	void start() {
		testbigmath();
	}
	
};

