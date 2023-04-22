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

#ifndef LINKEDLIST_H
#define LINKEDLIST_H

#if defined(NDEBUG) || defined(DISABLEMEMSAFETY)
#define __ASSERT__(bl) {}
#else
#include <stdexcept>
#define __ASSERT__(bl) assert(bl)
#endif

//
// double linked list
//

//list-item class
template <typename T>
struct linkitem
{

	T 			*m_obj;
	linkitem 	*m_next, *m_prev;

#if !(defined(NDEBUG) || defined(DISABLEMEMSAFETY))
//cppcheck-suppress operatorEqShouldBeLeftUnimplemented
	linkitem& operator=( _UNUSED_ const linkitem &rh ) {
		m_obj = 0;
		m_next = m_prev = 0;
		printf("Potentially unsafe copy of structure. Halting.\n"); throw std::runtime_error();
	}

//cppcheck-suppress noExplicitConstructor
	linkitem( _UNUSED_ const linkitem &rh ) : m_obj(0), m_next(0), m_prev(0) {
		printf("Potentially unsafe copy of structure. Halting.\n"); throw std::runtime_error();
	}
#endif

	inline linkitem() 			: m_obj(0), m_next(0), m_prev(0) 	{}//cppcheck-suppress noExplicitConstructor
	inline linkitem( T *obj ) 	: m_obj(obj), m_next(0), m_prev(0) 	{}//cppcheck-suppress noExplicitConstructor
	inline void link( T *obj ) 										{ m_obj = obj; m_next = 0; m_prev = 0; }

};


//list-base class
template <typename T>
struct linkbase
{

	linkitem<T> *m_first, *m_last;

	inline linkbase() : m_first(0), m_last() {}

	inline void reset() { m_first = 0; }

	inline void add( linkitem<T> *item ) {
		if(m_first) m_last->m_next = item;
		item->m_prev = m_last;
		m_last = item;
		if(!m_first) m_first = item;
	}

	inline void remove( linkitem<T> *item ) {
		if(item->m_prev) item->m_prev->m_next = item->m_next;
		if(item->m_next) item->m_next->m_prev = item->m_prev;
		if(m_last == item)  m_last = item->m_prev;
		if(m_first == item) m_first = item->m_next;
		item->m_prev = 0;
		item->m_next = 0;
	}	

	inline void insertbefore( linkitem<T> *item, linkitem<T> *newitem ) {
		if(item->m_prev) item->m_prev->m_next = newitem;
		newitem->m_prev = item->m_prev;
		newitem->m_next = item;
		item->m_prev = newitem;
		if(item==m_first) m_first = newitem;
	}

	inline void insertafter( linkitem<T> *item, linkitem<T> *newitem ) {
		if(item->m_next) item->m_next->m_prev = newitem;
		newitem->m_next = item->m_next;
		newitem->m_prev = item;
		item->m_next = newitem;
		if(item==m_last) m_last = newitem;
	}

	inline linkitem<T> *itemfirst() 					{ return(m_first); }
	inline linkitem<T> *itemlast() 						{ return(m_last); }
	inline linkitem<T> *itembefore( linkitem<T> *item ) { return(item->m_prev); }
	inline linkitem<T> *itemafter( linkitem<T> *item ) 	{ return(item->m_next); }

	inline T *first() 						{ if(m_first) 		return(m_first->m_obj); 		return(0); }
	inline T *last() 						{ if(m_first) 		return(m_last->m_obj); 			return(0); }
	inline T *before( linkitem<T> *item ) 	{ if(item->m_prev)	return(item->m_prev->m_obj); 	return(0); }
	inline T *after( linkitem<T> *item ) 	{ if(item->m_next)	return(item->m_next->m_obj); 	return(0); }

	inline void deleteall() {
		while(m_first) {
			linkitem<T> *point = m_first;
			remove(m_first);
			delete point->m_obj;
		}
	}

	inline void removeall() {
		while(m_first) remove(m_first);
	}

	inline bool executeall( bool (*fp)(T *obj) ) {
		linkitem<T> *i = m_first;
		while(i){
			if(!fp(i->m_obj)) return(false);
			i=i->m_next;
		}
		return(true);
	}

};

//
// optimized single linked list counting backward
// assumptions: 
//		item removal only from the end
//		can only walk the list backward
//

//list-item class
template <typename T>
struct linkitem_single
{

	T 				*m_obj;
	linkitem_single *m_prev;

#if !(defined(NDEBUG) || defined(DISABLEMEMSAFETY))
//cppcheck-suppress operatorEqShouldBeLeftUnimplemented
	linkitem_single& operator=( _UNUSED_ const linkitem_single &rh ) {
		m_obj = 0;
		m_prev = 0;
		printf("Potentially unsafe copy of structure. Halting.\n"); throw std::runtime_error();
	}

	linkitem_single( _UNUSED_ const linkitem_single &rh ) : m_obj(), m_prev() {
		printf("Potentially unsafe copy of structure. Halting.\n"); throw std::runtime_error();
	}
#endif

	inline linkitem_single() 			: m_obj(), m_prev()    {}//cppcheck-suppress noExplicitConstructor
	inline linkitem_single( T *obj ) 	: m_obj(obj), m_prev() {}//cppcheck-suppress noExplicitConstructor
	inline void link( T *obj ) 								   { m_obj = obj; }

};

//list-base class
template <typename T>
struct linkbase_single
{

	linkitem_single<T> *m_last;

	inline linkbase_single() : m_last(0) {}

	inline void reset() { m_last = 0; }

	inline void add( linkitem_single<T> *item ) {
		item->m_prev=m_last;
		m_last=item;
	}

	inline void remove( linkitem_single<T> *item ) {
		__ASSERT__(m_last==item);
		m_last = item->m_prev;
	}

	inline void insertbefore( linkitem_single<T> *item, linkitem_single<T> *newitem ) {
		newitem->m_prev = item->m_prev;
		item->m_prev = newitem;
	}

	inline void insertafter( linkitem_single<T> *item, linkitem_single<T> *newitem ) {
		newitem->m_prev = item;
		if(item==m_last) m_last = newitem;
	}

	inline linkitem_single<T> *itemlast() 								{ return(m_last); }
	inline linkitem_single<T> *itemafter( linkitem_single<T> *item ) 	{ return(item->m_next); }

	inline T *last() 													{ if(m_last) 		return(m_last->m_obj); 			return(0); }
	inline T *before( linkitem_single<T> *item ) 						{ if(item->m_prev)	return(item->m_prev->m_obj); 	return(0); }

	inline void deleteall() {
		while(m_last) {
			linkitem_single<T> *point = m_last;
			remove(m_last);
			delete point->m_obj;
		}
	}

	inline void removeall() { while(m_last) remove(m_last); }

	inline bool executeall( bool (*fp)(T *obj) ) {
		linkitem_single<T> *i = m_last;
		while(i){
			if(!fp(i->m_obj)) return(false);
			i=i->m_prev;
		}
		return(true);
	}

};

#undef __ASSERT__

#endif