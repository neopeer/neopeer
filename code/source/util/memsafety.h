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

#ifndef MEMSAFE_H
#define MEMSAFE_H

#define _UNUSED_ __attribute__((unused))

#if defined(NDEBUG) || defined(DISABLEMEMSAFETY)

#define SAFEHEAD(nm)
#define SAFE()			{}
#define HALTCOMPILE()	{}
#define THROW(msg)		{}
#define ASSERT(bl)		{}

struct __memsafe_pk {
	inline static void memleakcheck() {}
};

#else

#include <new>
#include <cassert>
#include <stdexcept>
#include "linkedlist.h"

struct __memsafe_pk {

	static thread_local linkbase<__memsafe_pk> 	g_base;
						linkitem<__memsafe_pk> 	m_item;
						const std::type_info  	*m_ti;

//cppcheck-suppress noExplicitConstructor
	inline __memsafe_pk( const std::type_info  *ti ) : m_item(this), m_ti(ti)  	{ g_base.add(&m_item); }
	inline ~__memsafe_pk() 														{ g_base.remove(&m_item); }

	static void memleakcheck() {

		__memsafe_pk *pk;
		char buffer[256];

		if((pk = __memsafe_pk::g_base.first())) {
			sprintf( buffer, "Leak detected for type:%s", pk->m_ti->name() );
			throw std::runtime_error(buffer);
		}

	}

};
thread_local linkbase<__memsafe_pk> __memsafe_pk::g_base;


template <typename nm>
struct __memsafe_st {

	__int64_t 		m_memsafeti = (__int64_t)(&typeid(nm));
	__memsafe_pk 	m_pk = &typeid(nm);

	__memsafe_st() 							{}	//cppcheck-suppress noExplicitConstructor
	__memsafe_st( const __memsafe_st &rh )  {	//cppcheck-suppress noExplicitConstructor
		throw std::runtime_error("Potentially unsafe copy of structure. Halting.");
	}

//cppcheck-suppress operatorEqShouldBeLeftUnimplemented
	__memsafe_st& operator=( const __memsafe_st &rh ) {
		throw std::runtime_error("Potentially unsafe copy of structure. Halting.");
	}

	~__memsafe_st() 				{ m_memsafeti=0; }
	void _memsafetycheck() const 	{ 
		assert((m_memsafeti == (__int64_t)(&typeid(nm))));
	}

};

#define SAFEHEAD(nm) 	__memsafe_st<nm> __safe_st;
#define SAFE() 			__safe_st._memsafetycheck();
#define HALTCOMPILE()	throw std::runtime_error();
#define THROW(msg)		throw std::runtime_error(#msg);
#define ASSERT(bl)		assert(bl);

#endif

#endif
