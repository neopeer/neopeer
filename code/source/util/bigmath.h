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

//Troubleshooting:
//	- Any global/static big number should either be thread_local or only used by a single thread
//Performance upgrades:
//	- Prefer constructor initializer lists for structure members
//	- Avoid the use of virtual declaration of functions in structures
//	- Prefer non-const return values *if* it's safe, does not cause compile ambiguity, and does not cause unnecessary implicit copies
//	- Prefer const input parameters UNLESS:
//		- function is returning a type that can be the result of optimized operations (e.g. +=) on non-const input values
//	- Prefer const member function declarations where possible 
//	- Prefer use of the address accessing (&) for input parameters where possible for anything other than integers
//	- Explicitly define global arithmetic operators with GMP & standard types to curate optimal performance (optimal copies etc...)
//	- Avoid defining global arithmetic operators as "const" for all input types if it forces an explicit invocation of a constructor in the routine
//	- Take advantage of the pre-allocated temporary memory & swap() internal operations provided if it will be faster for a GMP operation
//	- Use single linked lists where possible if lists are needed
//Style: 
//	- Do not override const warnings with a cast to eliminate const-ness, find a way to optimize your code instead without violating const contract
//	- User-accessible non-operator functions that return internal data types with accessible members/functions are returned by reference / all others by pointer
//  - Global operators give return type precedence to lefthand type unless operation includes a fraction or a standard data type:
//			 uint_t * frac_t = frac_t
//			 int    * frac_t = frac_t
//			 double * frac_t = frac_t
//			 frac_t * uint_t = frac_t
//			 mod_t * frac_t  = frac_t
//			 mod_t * uint_t  = mod_t
//			 uint_t * mod_t  = uint_t

#ifndef BIGMATH_H
#define BIGMATH_H

//below function must be implemented in the calling program to capture functions 
//	to call to clean up number caches - this is necessary to avoid a memory leak
extern "C" { 
	void __thread_function_cleaner_add__( void (*cleaner)(void) );
}

//#define MATHCALL __attribute__((sysv_abi))
#define MATHCALL

#include <new>
#include <gmp.h>
#include "linkedlist.h"
#include "memsafety.h"
#ifndef BIGMATHNOMEMWARN
#include <inttypes.h>
#endif

//
// math memory management template
//

//to disable custom memory handling
//define BIGMATHRAWALLOC  

//to disable gmp hacks
//define BIGMATHGMPHACKSDISABLE

#ifndef BIGMATHMEMSCALE	//internal memory scaling of number over its base size before causing a memory hit
#define BIGMATHMEMSCALE 3
#endif

						 //optimization for L1 CPU caches without compromising on bank sizes, measured in bits 						 
#ifndef BIGMATHCACHESIZE //- beware this is PER instantiated type, actual cache size is rounded up with hard floor
#define BIGMATHCACHESIZE (8*1024*8)
#endif

#ifndef BIGMATHMODSCALE	//can be lowered below the max memory scaling if there is a computational advantage to it
#define BIGMATHMODSCALE BIGMATHMEMSCALE
#endif

#ifndef BIGMATHBANKSIZE	//should be size large enough to prevent thrashing on allocating/deallocating numbers
#define BIGMATHBANKSIZE 100
#endif

#ifndef BIGMATHSTRBUFFERMAX	//max output string size from math library on any single string operation
#define BIGMATHSTRBUFFERMAX 256
#endif

#ifndef BIGMATHSTRQUEUEMAX //max number of concurrent output strings before looping memory (per thread)
#define BIGMATHSTRQUEUEMAX 32
#endif

#ifndef BIGMATHNOMEMWARN //disable memory hit warnings
#ifdef NDEBUG
#define BIGMATHNOMEMWARN
#endif
#endif

#ifndef BIGMATHALIGNMALLOC //allow override of malloc alignment (processor-specific), must be power of two
#define BIGMATHALIGNMALLOC 8
#endif

#ifndef BIGMATHMINMALLOC //this must be tied to the minimum size of a GMP limb
#define BIGMATHMINMALLOC (sizeof(__mpz_struct) + _bigmath_compile::bits2bytes(GMP_NUMB_BITS))
#endif

//
// helper constant generators to minimize template code and optimize resulting assembly
//

//requires >= c++20 to evaluate constants
static_assert(__cplusplus > 201703L,"c++ version must be >= 2020");
typedef typename std::make_signed<size_t>::type ssize_t;

namespace _bigmath_compile {

	consteval size_t  	bits2bytes( size_t a )		{ return(a>>3); }
	consteval bool 		isneg( ssize_t a ) 		 	{ return(a<0?true:false); 							}
	consteval size_t 	constabs( ssize_t a ) 		{ return(a<0?-a:a); 								}
	consteval size_t	minalloc( size_t a )		{ return(a>BIGMATHMINMALLOC?a:BIGMATHMINMALLOC);	}

	consteval ssize_t 	log2( size_t val, ssize_t c=0 ) { //log2 rounded down
		size_t r = val>>c;
		if(r==0) return (c-1);
		return log2(val,c+1);
	}

	consteval size_t 	compute_cache_pow2_size( const size_t cachebits, const size_t numbits ) {
		 size_t val   = cachebits/numbits;
		ssize_t log2r = log2(val)+1;		//round up
		 size_t log2f = log2r>1?log2r:1;	//ensure there are always at least 2 cache entries
		return(1<<log2f);
	}

	consteval size_t	modgenpow2calc( ssize_t a ) 	{ return isneg(a)?-a:0; 								}

	consteval size_t 	alloc_align( size_t sz ) { 
		#define algn (BIGMATHALIGNMALLOC)
		#define mask (BIGMATHALIGNMALLOC-1)
		return (sz+((algn-(sz&mask))&mask));	//rounds up to nearest power-of-2 (algn)
		#undef algn
		#undef mask
	}
}

//
// error check the configurations
//

static_assert(BIGMATHMODSCALE<=BIGMATHMEMSCALE,"BIGMATHMODSCALE must be <= BIGMATHMEMSCALE");
static_assert((1<<_bigmath_compile::log2(BIGMATHALIGNMALLOC))==BIGMATHALIGNMALLOC,"BIGMATHALIGNMALLOC is not a power of two");

//
// direct hacks into GMP structures to speed up certain functionality (e.g. reducing a number)
//

namespace _bigmath_gmp_hacks {
#ifdef BIGMATHGMPHACKSDISABLE
	MATHCALL inline void mpz_realloc ( mpz_ptr ptr, const mp_bitcnt_t bits, _UNUSED_ const int limbs ) { 
		::mpz_realloc2(ptr,bits);
	}
	MATHCALL inline void mpz_limbs_limit ( _UNUSED_ mpz_ptr ptr, _UNUSED_ const int maxlimbs, _UNUSED_ const mpz_ptr modulus ) 	{}
	MATHCALL inline void prefetch( _UNUSED_ void *addr )	{}
#else
	MATHCALL inline void mpz_realloc ( mpz_ptr ptr, _UNUSED_ const mp_bitcnt_t bits, _UNUSED_ const int limbs ) { 
		ptr->_mp_size = 0; 
	} //ptr->_mp_alloc = limbs;
	MATHCALL inline void mpz_limbs_limit ( mpz_ptr ptr, const int maxlimbs, const mpz_ptr modulus ) {
		if(ptr->_mp_size<0) {
			ptr->_mp_size = ptr->_mp_size > -maxlimbs ? ptr->_mp_size : -maxlimbs;
			mpz_add( ptr, ptr, modulus );
		}
		else ptr->_mp_size = ptr->_mp_size < maxlimbs ? ptr->_mp_size : maxlimbs;
	}
	MATHCALL inline void prefetch( _UNUSED_ mpz_ptr addr )	{
		__builtin_prefetch( addr->_mp_d, 1, 3 ); //invoke with caution to performance
	}
#endif
}

//
// paging namespace to deal with callbacks from GMP memory requests
//
//	 virtual edge case: these functions are deliberately virtual; a function pointer callback to code 
//						where the memory work is performed against constants is the ideal. A manual function 
//						pointer implementation would be required in absence of implicit virtual calls which
//						would be just as slow, but bulkier to implement. 
//

struct mathpaging_t {

	//header structure
	struct pageheader_t { size_t m_flags, m_sz; };

	//constants & flags
	static thread_local mathpaging_t 	g_default;
	static thread_local mathpaging_t 	*g_activeptr;
	static thread_local size_t 			g_activeindex;

	constexpr static size_t FLG_USED  	= 0x01;
	constexpr static size_t FLG_MALLOC  = 0x02;

	constexpr static size_t HDR_SZ  	= sizeof(pageheader_t);

	//variables
	static bool g_setallocators;	//not thread local as is needed to prevent threads from duplicating GMP init
	#ifndef BIGMATHNOMEMWARN
	static thread_local __int64_t g_alloc_cnt, g_realloc_cnt, g_free_cnt;
	#endif

			//real allocation hit (avoid if possible)
			MATHCALL inline static char* __do_alloc( size_t sz ) {
				#ifndef BIGMATHNOMEMWARN
				g_alloc_cnt++;
				printf("[WARN] Bigmath malloc ");
				printf("%zu",sz);
				printf(" - stats:");
				printf("%" PRId64 ",", g_alloc_cnt );
				printf("%" PRId64 ",", g_realloc_cnt );
				printf("%" PRId64 "\n", g_free_cnt );
				#endif
				return( reinterpret_cast<char*>( malloc(sz+HDR_SZ) ) );
			}

			//real re-allocation hit (avoid if possible)
			MATHCALL inline static char* __do_realloc( void *ptr, size_t sz ) {
				#ifndef BIGMATHNOMEMWARN
				g_realloc_cnt++;
				printf("[WARN] Bigmath realloc ");
				printf("%zu",sz);
				printf(" - stats:");
				printf("%" PRId64 ",", g_alloc_cnt );
				printf("%" PRId64 ",", g_realloc_cnt );
				printf("%" PRId64 "\n", g_free_cnt );
				#endif
				return( reinterpret_cast<char*>( realloc(ptr, sz+HDR_SZ) ) );
			}

			//real free hit (avoid if possible)
			MATHCALL inline static void __do_free( void *ptr ) {
				#ifndef BIGMATHNOMEMWARN
				g_free_cnt++;
				printf("[WARN] Bigmath free");
				printf(" - stats:");
				printf("%" PRId64 ",", g_alloc_cnt );
				printf("%" PRId64 ",", g_realloc_cnt );
				printf("%" PRId64 "\n", g_free_cnt );
				#endif
				free(ptr);
			}

			//prepare block
			MATHCALL inline static void* __prepare_block( char *memstart, size_t flags, size_t sz )  { 
				ASSERT(sz > 0);
				pageheader_t *hdr;
				hdr = reinterpret_cast<pageheader_t*>(memstart);
				hdr->m_flags = flags;
				hdr->m_sz = sz;
				memstart+=HDR_SZ;
				return(memstart);
			}

			//update block
			MATHCALL inline static void* __update_block( char *memstart, size_t sz )  { 
				ASSERT(sz > 0);
				pageheader_t *hdr;
				hdr = reinterpret_cast<pageheader_t*>(memstart);
				hdr->m_sz = sz;
				memstart+=HDR_SZ;
				return(memstart);
			}

			//update block flags
			MATHCALL inline static void* __add_block_flags( char *memstart, size_t flags )  { 
				pageheader_t *hdr;
				hdr = reinterpret_cast<pageheader_t*>(memstart);
				hdr->m_flags |= flags;
				ASSERT(hdr->m_sz > 0);
				memstart+=HDR_SZ;
				return(memstart);
			}

			//get header for start of block
			MATHCALL inline static pageheader_t* __get_block_hdr( void *ptr ) {
				char *memstart = reinterpret_cast<char*>(ptr) - HDR_SZ;
				pageheader_t *hdr = reinterpret_cast<pageheader_t*>(memstart);
				ASSERT(hdr->m_sz > 0);
				return(hdr);
			}

		//can be overloaded by bankpaging_t
		inline virtual void *_alloc( size_t alloc_size ) {	
			size_t sz = alloc_size*2;
			return(__prepare_block( __do_alloc(sz), FLG_USED|FLG_MALLOC, sz ));
		}

		//this is not overloaded by derived classes
		inline void *_realloc( void *ptr, size_t old_size, size_t new_size ) {
			ASSERT(ptr!=0); //per gmp manual: ptr is never null

			pageheader_t *hdr;
			size_t sz, cpsz;
			void *handle;

			hdr = __get_block_hdr(ptr);
			if(new_size<=hdr->m_sz) return(ptr);												//fast return - no-op

			sz = new_size*2;
			sz = sz>0?sz:1;
			if(hdr->m_flags & FLG_MALLOC) return(__update_block( __do_realloc(hdr,sz), sz ));	//realloc - expensive

			cpsz = old_size < new_size ? old_size : new_size;
			hdr->m_flags &= (~FLG_USED);
			handle = __prepare_block( __do_alloc(sz), FLG_USED|FLG_MALLOC, sz );
			if(cpsz>0) memcpy(handle,ptr,cpsz);													//copying from page mem to alloc mem - even worse
			return(handle);
		}

		//this is not overloaded by derived classes
		inline void _free( void *ptr, _UNUSED_ size_t size ) {
			pageheader_t *hdr;
			hdr = __get_block_hdr(ptr);
			if((hdr->m_flags & FLG_MALLOC)==0) { 
				hdr->m_flags &= (~FLG_USED);													//fast return
				return;
			}
			__do_free(hdr);																		//malloc free - costly
		}

	//global functions - should forward the parameters to thread-local functions without needing to re-push values onto stack for shared calling convention
	static void *allocate_function( size_t alloc_size ) 								{ return(g_activeptr->_alloc(alloc_size)); }
	static void *reallocate_function( void *ptr, size_t old_size, size_t new_size ) 	{ return(g_activeptr->_realloc(ptr,old_size,new_size)); }
	static void free_function( void *ptr, size_t size ) 								{ g_activeptr->_free(ptr,size); }

};

thread_local mathpaging_t 	mathpaging_t::g_default;
thread_local mathpaging_t 	*mathpaging_t::g_activeptr = &mathpaging_t::g_default;
thread_local size_t 		mathpaging_t::g_activeindex;
			 bool	 		mathpaging_t::g_setallocators = false;
#ifndef BIGMATHNOMEMWARN
thread_local __int64_t mathpaging_t::g_alloc_cnt=0, mathpaging_t::g_realloc_cnt=0, mathpaging_t::g_free_cnt=0;
#endif

//
// Preallocate string paging for GMP types
//

struct mathpagingstr_t : mathpaging_t {

	static thread_local mathpagingstr_t g_strdefault;

	constexpr static size_t MEMSZ = (BIGMATHSTRBUFFERMAX+HDR_SZ)*2;

	char 	m_pagemem[ MEMSZ ];
	size_t 	m_index;
	
	MATHCALL mathpagingstr_t() : m_pagemem(), m_index() {}

	MATHCALL inline void reset() { m_index=0; }

	MATHCALL void *_alloc( size_t alloc_size ) override final {
		#define algn (BIGMATHALIGNMALLOC)
		#define mask (BIGMATHALIGNMALLOC-1)
		void *handle;
		size_t sz = alloc_size + HDR_SZ;
		size_t boost = (algn-(sz&mask))&mask;		//align to proper multiple
		sz+=boost; alloc_size+=boost;
		if((m_index+sz)>MEMSZ) return mathpaging_t::_alloc(alloc_size);
		handle = __prepare_block( &m_pagemem[m_index], FLG_USED, alloc_size );
		m_index += sz;
		return(handle);
		#undef algn
		#undef mask
	}

};

thread_local mathpagingstr_t mathpagingstr_t::g_strdefault;


//
// memory bank routines to speed up math objects
//

template <typename T, ssize_t S, typename CBT>
struct mathbankaccess_t {

	//
	// memory safety
	//
	
	SAFEHEAD(mathbankaccess_t)

	//
	// global per unique set of template parameters
	//

	struct bank_t;
	struct bankpreload_t;

	//aux structure to assist with keeping track of state of internal bank numbers
	struct bankentry_t {

		static thread_local char		g_strbuffer[BIGMATHSTRBUFFERMAX*BIGMATHSTRQUEUEMAX];	//thread specific to avoid mutex locks
		static thread_local size_t		g_strbufferpos;											//thread specific to avoid mutex locks

		linkitem_single<bankentry_t>	m_item;			//linked-list entry for fast fetching
		bank_t 							*m_bank;		//pointer back to bank that holds GMP value
		T	 							*m_v;			//pointer to actual GMP value
		bankentry_t						*m_maske;		//for modular numbers to point to masking number for fast moduli
		size_t							m_refcnt;		//for modular numbers to prevent duplicating moduli
		size_t							m_bankindex;	//index in bank

		MATHCALL inline bankentry_t() : m_item(this), m_bank(), m_v(), m_maske(), m_refcnt(), m_bankindex() {}

		MATHCALL inline char *getstringmem() {
			char *handle;
			if(g_strbufferpos>BIGMATHSTRBUFFERMAX*(BIGMATHSTRQUEUEMAX-1)) g_strbufferpos=0;
			handle = &g_strbuffer[g_strbufferpos];
			g_strbufferpos+=BIGMATHSTRBUFFERMAX;
			return(handle);
		}

		MATHCALL inline T *raw() { return(m_v); }

	};

	//page memory management for banks (reduces hard memory hits for GMP)
	struct bankpaging_t : mathpaging_t {

		SAFEHEAD(bankpaging_t)
		
		constexpr static size_t LOCALMEMSCALE		= CBT::LOCALMEMSCALE();	//calls a consteval in derived classes to scale up paging
		constexpr static size_t BANKSIZE  			= BIGMATHBANKSIZE;
		constexpr static size_t BANKSIZESCALE	  	= BIGMATHBANKSIZE*LOCALMEMSCALE;
		constexpr static size_t MAXALLOC			= _bigmath_compile::alloc_align( _bigmath_compile::minalloc( _bigmath_compile::bits2bytes( S*BIGMATHMEMSCALE )));
		constexpr static size_t PAGEVALUESZ 		= _bigmath_compile::alloc_align( MAXALLOC+sizeof(pageheader_t) );

		char m_pagemem[ BANKSIZESCALE*PAGEVALUESZ ];	//page memory for GMP internal numbers

		MATHCALL bankpaging_t() : m_pagemem() {
			SAFE();
			size_t x;
			for(x=0;x<BANKSIZESCALE;x++) __prepare_block( &m_pagemem[x*PAGEVALUESZ], 0, MAXALLOC );
		}

		MATHCALL void *_alloc( size_t alloc_size ) override final {
			SAFE();
			ASSERT(mathpaging_t::g_activeindex<BANKSIZE);
			size_t x;
			pageheader_t *hdr;
			char *ptr;
			if(alloc_size>MAXALLOC) return(mathpaging_t::_alloc(alloc_size));		//call parent allocator
			for(x=0;x<LOCALMEMSCALE;x++) {
				ptr = &m_pagemem[ ( mathpaging_t::g_activeindex * LOCALMEMSCALE + x ) * PAGEVALUESZ ];
				hdr = reinterpret_cast<pageheader_t*>(ptr);
				ASSERT(hdr->m_sz > 0);
				if((hdr->m_flags&FLG_USED)==0) return(__add_block_flags( ptr, FLG_USED ));
			}
			return(mathpaging_t::_alloc(MAXALLOC)); //call parent allocator
		}

	};

	//the actual memory bank objects
	struct bank_t {

		SAFEHEAD(bank_t)

		constexpr static size_t BANKSIZE  = BIGMATHBANKSIZE;

		//constexpr static size_t C_ALLOC_LIMBS = (S/GMP_NUMB_BITS+(S%GMP_NUMB_BITS==0?0:1)+1);
		//constexpr static size_t C_ALLOC_BITS  = C_ALLOC_LIMBS*GMP_NUMB_BITS;
		const size_t C_ALLOC_LIMBS = (S/mp_bits_per_limb+(S%mp_bits_per_limb==0?0:1)+1);
		const size_t C_ALLOC_BITS  = C_ALLOC_LIMBS*mp_bits_per_limb;

		constexpr static size_t CACHESIZE = _bigmath_compile::compute_cache_pow2_size(BIGMATHCACHESIZE,S);
		constexpr static size_t CACHEMASK = CACHESIZE-1;

		static thread_local	bankentry_t						*g_cache[CACHESIZE];
		static thread_local	size_t 							g_cachestore, g_cachefetch;

		static thread_local linkbase<bank_t> 				g_base, g_freebase;	//thread specific to avoid mutex locks
							linkitem<bank_t> 				m_item, m_freeitem;
							linkbase_single<bankentry_t> 	m_freenodebase;

		T	 			m_v[BANKSIZE];
		bankentry_t 	m_nodes[BANKSIZE];
		size_t 			m_usedcount, m_freecount;
		bankpaging_t	m_paging;					//page memory for GMP numbers

		#ifndef NDEBUG
		MATHCALL void checksafety() { SAFE(); }
		#endif

		MATHCALL inline bool isbankfree() { SAFE(); return(m_freenodebase.last() || m_usedcount<BANKSIZE); }

		//arm & invoke memory functions
		MATHCALL inline void initvalue( size_t index ) {
			m_nodes[index].m_bank 		= this;
			m_nodes[index].m_v			= &m_v[index];
			m_nodes[index].m_bankindex	= index;

			mathpaging_t::g_activeindex = index;
			mathpaging_t::g_activeptr = &m_paging;
			CBT::_cbinit(&m_v[index],C_ALLOC_BITS,C_ALLOC_LIMBS);
			mathpaging_t::g_activeptr = &mathpaging_t::g_default;
		}

		//arm & invoke memory functions
		MATHCALL inline void deinitvalue( size_t index ) {
			mathpaging_t::g_activeindex = index;
			mathpaging_t::g_activeptr = &m_paging;
			CBT::_cbdeinit(&m_v[index],C_ALLOC_BITS,C_ALLOC_LIMBS);
			mathpaging_t::g_activeptr = &mathpaging_t::g_default;
		}

		//arm & invoke memory functions
		MATHCALL inline void reinitvalue( size_t index ) {
			mathpaging_t::g_activeindex = index;
			mathpaging_t::g_activeptr = &m_paging;
			CBT::_cbrealloc(&m_v[index],C_ALLOC_BITS,C_ALLOC_LIMBS);
			mathpaging_t::g_activeptr = &mathpaging_t::g_default;
		}

		MATHCALL bank_t() : m_item(this), m_freeitem(this), m_usedcount(0), m_freecount(0), m_paging() {
			SAFE()
			size_t x;
			g_base.add(&m_item);
			g_freebase.add(&m_freeitem);
			for(x=0;x<BANKSIZE;x++) initvalue(x);
		}

		MATHCALL ~bank_t() {
			SAFE()
			size_t x;
			g_base.remove(&m_item);
			if(isbankfree()) g_freebase.remove(&m_freeitem);
			for(x=0;x<BANKSIZE;x++) deinitvalue(x);
		}

			//longer-term recycling for performance
			MATHCALL static bankentry_t *_allocfrombank() {
				
				bankentry_t 	*node;
				bank_t 			*bank = g_freebase.first();

				if(bank==0) {
					if(!(bank=new bank_t)) { throw std::bad_alloc(); }
				}

				if((node = bank->m_freenodebase.last())==0) {
					node = &bank->m_nodes[bank->m_usedcount++];
				}
				else {
					bank->m_freenodebase.remove(&node->m_item);
					bank->m_freecount--;
				}

				if(bank->isbankfree()==false) g_freebase.remove(&bank->m_freeitem);

				return node;

			}

			//optimized thread-local L1 recycling for performance
			MATHCALL inline static bankentry_t *_cachefetch() {
				bankentry_t *e;
				g_preloader.compile();				//force compiler to include preloader for g_cache - it will be optimized out otherwise
				if((e=g_cache[g_cachefetch])) { 
					g_cache[g_cachefetch]=0;
					g_cachefetch=(g_cachefetch+1)&CACHEMASK;	//fast wrap for pow2 field
					return e;
				}
				return(_allocfrombank());
			}

		//find a number for usage by base number template
		MATHCALL inline static bankentry_t *allocnode() { return(_cachefetch()); }

			//longer-term recycling for performance
			MATHCALL void _freetobank( bankentry_t *e ) {				
				SAFE()
				m_freenodebase.add(&e->m_item);							//add node to bank's freenodebase
				if(isbankfree()==false) g_freebase.add(&m_freeitem);	//add bank to thread's list of banks with free elements
				reinitvalue(e->m_bankindex); 							//reset gmp memory of this entry - prevents memory creep
				if(++m_freecount>=m_usedcount) delete this;				//free this bank if all items free
			}

			//optimized thread-local L1 recycling for performance
			MATHCALL inline static void _cachestore( bankentry_t *e ) {
				if(g_cache[g_cachestore]) { 
					g_cache[g_cachestore]->m_bank->_freetobank( g_cache[g_cachestore] ); 
					g_cachefetch=(g_cachestore+1)&CACHEMASK; 	//move fetch up so it's using the oldest object in cache
				}
				g_cache[g_cachestore]=e;
				g_cachestore=(g_cachestore+1)&CACHEMASK;		//fast wrap for pow2 field
			}

		//release a number from usage by base number template
		MATHCALL static inline void freenode( bankentry_t *e ) { _cachestore(e); }

	};

	//automatically initializes bank states at thread start
	struct bankpreload_t {

		MATHCALL static void setallocators() {
			if(mathpaging_t::g_setallocators) return;
			mathpaging_t::g_setallocators = true;
			ASSERT(mp_bits_per_limb==GMP_NUMB_BITS); //ensure expected (C_ALLOC_LIMBS,C_ALLOC_BITS)
			#ifndef BIGMATHRAWALLOC
			mp_set_memory_functions( 
				mathpaging_t::allocate_function,
				mathpaging_t::reallocate_function,
				mathpaging_t::free_function
			);
			#endif
		}
		
		MATHCALL static void unloadcache() {
			size_t x;
			for(x=0;x<bank_t::CACHESIZE;x++) {
				if(bank_t::g_cache[x]) bank_t::g_cache[x]->m_bank->_freetobank(bank_t::g_cache[x]);
			}
		}

#ifndef NDEBUG
		MATHCALL static void checkcache() {
			size_t x;
			for(x=0;x<bank_t::CACHESIZE;x++) {
				if(bank_t::g_cache[x]) bank_t::g_cache[x]->m_bank->checksafety();
			}
		}
#endif

		MATHCALL bankpreload_t() {
			size_t x;
			setallocators();
			for(x=0;x<bank_t::CACHESIZE;x++) bank_t::g_cache[x] = bank_t::_allocfrombank();
			__thread_function_cleaner_add__(&unloadcache);
		}

		MATHCALL inline void compile() {} //invoked by bank_t() constructor to prevent optimizing out the preloader

	};

	static thread_local bankpreload_t g_preloader;	//preloading of number cache + memory manager for entire thread at initialization of first type

	//
	// local (per instatiation)
	//

			bankentry_t *m_e, *m_etmp;	//no need to swap these during runtime
	mutable T			*m_v, *m_vtmp;

	MATHCALL inline void swap() const {
		SAFE()
		T *swapv;
		swapv = m_v; m_v = m_vtmp; m_vtmp = swapv;
	}
};

template <typename T, ssize_t S, typename CBT>
thread_local char 														mathbankaccess_t<T,S,CBT>::bankentry_t::g_strbuffer[BIGMATHSTRBUFFERMAX*BIGMATHSTRQUEUEMAX];

template <typename T, ssize_t S, typename CBT>
thread_local size_t 													mathbankaccess_t<T,S,CBT>::bankentry_t::g_strbufferpos = 0;

template <typename T, ssize_t S, typename CBT>
thread_local typename mathbankaccess_t<T,S,CBT>::bankentry_t 			*mathbankaccess_t<T,S,CBT>::bank_t::g_cache[CACHESIZE];

template <typename T, ssize_t S, typename CBT>
thread_local size_t 													mathbankaccess_t<T,S,CBT>::bank_t::g_cachestore=0;

template <typename T, ssize_t S, typename CBT>
thread_local size_t			 											mathbankaccess_t<T,S,CBT>::bank_t::g_cachefetch=0;

template <typename T, ssize_t S, typename CBT>
thread_local linkbase<typename mathbankaccess_t<T,S,CBT>::bank_t> 		mathbankaccess_t<T,S,CBT>::bank_t::g_base;

template <typename T, ssize_t S, typename CBT>
thread_local linkbase<typename mathbankaccess_t<T,S,CBT>::bank_t> 		mathbankaccess_t<T,S,CBT>::bank_t::g_freebase;

template <typename T, ssize_t S, typename CBT>
thread_local typename mathbankaccess_t<T,S,CBT>::bankpreload_t 			mathbankaccess_t<T,S,CBT>::g_preloader;


//
// uint code
//

template <ssize_t S>
struct bigmod_t;

template <ssize_t S>
struct bigfrac_t;

template <ssize_t S>
struct biguint_t {

	static_assert(S>0,"error: biguint_t <= 0");

	SAFEHEAD(biguint_t)
	mathbankaccess_t<mpz_t,S,biguint_t<S>> b;

	//
	// constant functions for bank
	//

	consteval static size_t LOCALMEMSCALE() { return 1; }

	//
	// routines
	//

		//callbacks for the mathbank
		MATHCALL inline static void _cbinit(mpz_t *v, const size_t bits, _UNUSED_ const int limbs) 	 			{ mpz_init2(v[0],bits);    }
		MATHCALL inline static void _cbdeinit(mpz_t *v, _UNUSED_ const size_t bits, _UNUSED_ const int limbs) 	{ mpz_clear(v[0]); 		   }
		MATHCALL inline static void _cbrealloc(mpz_t *v, const size_t bits, const int limbs) 					{ _bigmath_gmp_hacks::mpz_realloc(v[0],bits,limbs); }
		MATHCALL inline static void _prefetch(mpz_t *v) 														{ _bigmath_gmp_hacks::prefetch(v[0]); }

	//user routines
	#define make(e,v) { e=mathbankaccess_t<mpz_t,S,biguint_t<S>>::bank_t::allocnode(); v=e->m_v; }
	#define kill(e)   if(e) { mathbankaccess_t<mpz_t,S,biguint_t<S>>::bank_t::freenode(e); }

		MATHCALL inline void _init() 												{ SAFE() make(b.m_e,b.m_v)   make(b.m_etmp,b.m_vtmp) }
	MATHCALL inline ~biguint_t() 													{ SAFE() kill(b.m_e)         kill(b.m_etmp)          }
	//the above is deliberately *not* virtual for performance

	#undef make
	#undef kill

	#define _S1 S
	#define _S2 -S

	MATHCALL inline biguint_t()  													{ _init(); }						//cppcheck-suppress noExplicitConstructor
	MATHCALL inline biguint_t( int val )  											{ _init(); this->operator=(val); }	//cppcheck-suppress noExplicitConstructor
	MATHCALL inline biguint_t( const mpz_t *rhs ) 		 							{ _init(); this->operator=(rhs); }	//cppcheck-suppress noExplicitConstructor
	MATHCALL inline biguint_t( const biguint_t &rhs ) 								{ _init(); this->operator=(rhs); }	//cppcheck-suppress noExplicitConstructor
	MATHCALL inline biguint_t( const bigmod_t<_S1> &rhs )							{ _init(); this->operator=(rhs); }	//cppcheck-suppress noExplicitConstructor
	MATHCALL inline biguint_t( const bigmod_t<_S2> &rhs )							{ _init(); this->operator=(rhs); }	//cppcheck-suppress noExplicitConstructor

	MATHCALL inline 		 int 				operator=( int val ) 				{ SAFE() mpz_set_ui(b.m_v[0],(unsigned long int)val); return(val); }
	MATHCALL inline 	     mpz_t* 			operator=( const mpz_t *rhs )		{ SAFE() mpz_set(b.m_v[0],rhs[0]);  return(b.m_v); }
	MATHCALL inline 	 	 biguint_t<S> & 	operator=( const biguint_t<S> &rhs ){ SAFE() mpz_set(b.m_v[0],rhs.b.m_v[0]); return(*this); }				//overload to avoid structure copy errors
	MATHCALL inline const bigmod_t<_S1> & 	operator=( const bigmod_t<_S1> &rhs ) 	{ SAFE() this[0] = rhs.constbase(); return(rhs); }						//overload to avoid structure copy errors
	MATHCALL inline const bigmod_t<_S2> & 	operator=( const bigmod_t<_S2> &rhs ) 	{ SAFE() this[0] = rhs.constbase(); return(rhs); }						//overload to avoid structure copy errors

	#undef _S1 
	#undef _S2 

		MATHCALL inline void _neg()		 											{ SAFE() mpz_neg(b.m_v[0],b.m_v[0]); }
		MATHCALL inline void _abs()		 											{ SAFE() mpz_abs(b.m_v[0],b.m_v[0]); }

		MATHCALL inline bool _eq( const mpq_t *rhs ) 						const 	{ SAFE() return( bigfrac_t<S>( this[0] ) == rhs ); }
		MATHCALL inline bool _gt( const mpq_t *rhs ) 						const 	{ SAFE() return( bigfrac_t<S>( this[0] ) > rhs ); }
		MATHCALL inline bool _gte( const mpq_t *rhs ) 						const 	{ SAFE() return( bigfrac_t<S>( this[0] ) >= rhs ); }
		MATHCALL inline bool _lt( const mpq_t *rhs ) 						const 	{ SAFE() return( bigfrac_t<S>( this[0] ) < rhs ); }
		MATHCALL inline bool _lte( const mpq_t *rhs ) 						const 	{ SAFE() return( bigfrac_t<S>( this[0] ) <= rhs ); }

		MATHCALL inline bool _eq( const mpz_t *rhs ) 						const 	{ SAFE() return(mpz_cmp( b.m_v[0], rhs[0] ) == 0);  }
		MATHCALL inline bool _gt( const mpz_t *rhs ) 						const 	{ SAFE() return(mpz_cmp( b.m_v[0], rhs[0] ) > 0);  }
		MATHCALL inline bool _gte( const mpz_t *rhs ) 						const 	{ SAFE() return(mpz_cmp( b.m_v[0], rhs[0] ) >= 0); }
		MATHCALL inline bool _lt( const mpz_t *rhs ) 						const 	{ SAFE() return(mpz_cmp( b.m_v[0], rhs[0] ) < 0);  }
		MATHCALL inline bool _lte( const mpz_t *rhs ) 						const 	{ SAFE() return(mpz_cmp( b.m_v[0], rhs[0] ) <= 0); }
		MATHCALL inline void _add( const mpz_t *rhs ) 								{ SAFE() mpz_add( b.m_v[0], b.m_v[0], rhs[0] ); }
		MATHCALL inline void _sub( const mpz_t *rhs ) 								{ SAFE() mpz_sub( b.m_v[0], b.m_v[0], rhs[0] ); }
		MATHCALL inline void _mul( const mpz_t *rhs ) 								{ SAFE() mpz_mul( b.m_vtmp[0], b.m_v[0], rhs[0] ); b.swap(); }
		MATHCALL inline void _div( const mpz_t *rhs ) 								{ SAFE() mpz_tdiv_q( b.m_vtmp[0], b.m_v[0], rhs[0] ); b.swap(); } //tdiv to handle signed overload
		MATHCALL inline void _mod( const mpz_t *rhs ) 								{ SAFE() mpz_mod( b.m_vtmp[0], b.m_v[0], rhs[0] ); b.swap(); }
		MATHCALL inline void _and( const mpz_t *rhs ) 								{ SAFE() mpz_and( this->b.m_v[0], this->b.m_v[0], rhs[0] ); }
		MATHCALL inline void _or( const mpz_t *rhs ) 								{ SAFE() mpz_ior( this->b.m_v[0], this->b.m_v[0], rhs[0] ); }
		MATHCALL inline void _xor( const mpz_t *rhs ) 								{ SAFE() mpz_xor( this->b.m_v[0], this->b.m_v[0], rhs[0] ); }

		MATHCALL inline bool _eq( const int rhs ) 							const 	{ SAFE() return(mpz_cmp_ui( b.m_v[0], (unsigned long int)rhs ) == 0);  }
		MATHCALL inline bool _gt( const int rhs ) 							const 	{ SAFE() return(mpz_cmp_ui( b.m_v[0], (unsigned long int)rhs ) > 0);  }
		MATHCALL inline bool _gte( const int rhs ) 							const 	{ SAFE() return(mpz_cmp_ui( b.m_v[0], (unsigned long int)rhs ) >= 0); }
		MATHCALL inline bool _lt( const int rhs ) 							const 	{ SAFE() return(mpz_cmp_ui( b.m_v[0], (unsigned long int)rhs ) < 0);  }
		MATHCALL inline bool _lte( const int rhs ) 							const 	{ SAFE() return(mpz_cmp_ui( b.m_v[0], (unsigned long int)rhs ) <= 0); }
		MATHCALL inline void _add( const int rhs ) 									{ SAFE() mpz_add_ui( b.m_v[0], b.m_v[0], (unsigned long int)rhs ); }
		MATHCALL inline void _sub( const int rhs ) 									{ SAFE() mpz_sub_ui( b.m_v[0], b.m_v[0], (unsigned long int)rhs ); }
		MATHCALL inline void _mul( const int rhs ) 									{ SAFE() mpz_mul_ui( b.m_vtmp[0], b.m_v[0], (unsigned long int)rhs ); b.swap(); }
		MATHCALL inline void _div( const int rhs ) 									{ SAFE() mpz_tdiv_q_ui( b.m_vtmp[0], b.m_v[0], (unsigned long int)rhs ); b.swap(); } //tdiv to handle signed overload
		MATHCALL inline void _mod( const int rhs ) 									{ SAFE() mpz_mod_ui( b.m_vtmp[0], b.m_v[0], (unsigned long int)rhs ); b.swap(); }
		MATHCALL inline void _lsh( const int rhs ) 									{ SAFE() mpz_mul_2exp( b.m_vtmp[0], b.m_v[0], rhs ); b.swap(); }
		MATHCALL inline void _rsh( const int rhs ) 									{ SAFE() mpz_fdiv_q_2exp( b.m_vtmp[0], b.m_v[0], rhs ); b.swap();  }
		MATHCALL inline void _and( const int rhs ) 									{ SAFE() this[0]&=(biguint_t<S>(rhs)); }
		MATHCALL inline void _or( const int rhs ) 									{ SAFE() this[0]|=(biguint_t<S>(rhs)); }
		MATHCALL inline void _xor( const int rhs ) 									{ SAFE() this[0]^=(biguint_t<S>(rhs)); }

	MATHCALL double double_from_div( const mpz_t *d, int precision=64 ) const {

		SAFE()

		double result;	//; n/d = n//d + (n%d)/d
		biguint_t<S> q, r, dmod(d);
		int tb, shift;

		mpz_fdiv_qr( q.b.m_v[0], r.b.m_v[0], b.m_v[0], d[0] );			// q=n//d, r=n%d
		result  = mpz_get_d( q.b.m_v[0] );

		tb 	  = mpz_sizeinbase(d[0],2)+1;								// gives top bit, function gives 1-based index
		shift = ((tb-precision)>0)?(tb-precision):0;

		mpz_fdiv_q_2exp(r.b.m_v[0],r.b.m_v[0],shift);					// r >> shift
		mpz_fdiv_q_2exp(dmod.b.m_v[0],d[0],shift); 						// dmod >> shift

		result += mpz_get_d(r.b.m_v[0]) / mpz_get_d(dmod.b.m_v[0]) ;

		return(result);

	}

	MATHCALL inline void operator+=( const mpz_t *rhs ) 							{ _add(rhs); }
	MATHCALL inline void operator-=( const mpz_t *rhs ) 							{ _sub(rhs); }
	MATHCALL inline void operator*=( const mpz_t *rhs ) 							{ _mul(rhs); }
	MATHCALL inline void operator/=( const mpz_t *rhs ) 							{ _div(rhs); }
	MATHCALL inline void operator%=( const mpz_t *rhs ) 							{ _mod(rhs); }
	MATHCALL inline void operator&=( const mpz_t *rhs ) 							{ _and(rhs); }
	MATHCALL inline void operator|=( const mpz_t *rhs ) 							{ _or(rhs);  }
	MATHCALL inline void operator^=( const mpz_t *rhs ) 							{ _xor(rhs); }

	MATHCALL inline void operator+=( const int rhs ) 								{ _add(rhs); }
	MATHCALL inline void operator-=( const int rhs ) 								{ _sub(rhs); }
	MATHCALL inline void operator*=( const int rhs ) 								{ _mul(rhs); }
	MATHCALL inline void operator/=( const int rhs ) 								{ _div(rhs); }
	MATHCALL inline void operator%=( const int rhs ) 								{ _mod(rhs); }
	MATHCALL inline void operator<<=( const int rhs ) 								{ _lsh(rhs); }
	MATHCALL inline void operator>>=( const int rhs ) 								{ _rsh(rhs); }
	MATHCALL inline void operator&=( const int rhs ) 								{ _and(rhs); }
	MATHCALL inline void operator|=( const int rhs ) 								{ _or(rhs);  }
	MATHCALL inline void operator^=( const int rhs ) 								{ _xor(rhs); }

	MATHCALL inline 		 			 	mpz_t* 			raw()			const 	{ SAFE() return(b.m_v); 					}
	MATHCALL inline		 			  const char* 			str()			const	{ SAFE() return((const char*)this[0]); 		}
	MATHCALL inline 		 operator const mpz_t*			()				const	{ SAFE() return(b.m_v); 					}
	MATHCALL inline explicit operator 	  	double			() 				const 	{ SAFE() return(mpz_get_d(b.m_v[0]));  		}
	MATHCALL inline explicit operator 	   	unsigned int	() 				const 	{ SAFE() return(mpz_get_ui(b.m_v[0]));   	}
	MATHCALL inline explicit operator 	   	int				() 				const 	{ SAFE() return((int)mpz_get_ui(b.m_v[0])); }
	//operator const return of mpz_t is to prevent resolution ambiguity introduced by non-const

	MATHCALL explicit operator const char*() const {
		SAFE()
		char *r = b.m_e->getstringmem();
		mathpagingstr_t::g_strdefault.reset();
		mathpagingstr_t::g_activeptr = &mathpagingstr_t::g_strdefault;
		gmp_snprintf( r, BIGMATHSTRBUFFERMAX, "%Zd", b.m_v[0] ); 
		mathpagingstr_t::g_activeptr = &mathpaging_t::g_default;
		return r;
	}

	//
	// global routines
	//

	MATHCALL inline static biguint_t<S> gcd( const mpz_t *lhs, const mpz_t *rhs ) 	{ biguint_t<S> r; mpz_gcd( r.b.m_v[0], lhs[0], rhs[0] ); return r; }
	MATHCALL inline static biguint_t<S> lcm( const mpz_t *lhs, const mpz_t *rhs ) 	{ biguint_t<S> r; mpz_lcm( r.b.m_v[0], lhs[0], rhs[0] ); return r; }
	MATHCALL inline static biguint_t<S> nextprime( const mpz_t *v ) 				{ biguint_t<S> r; mpz_nextprime( r.b.m_v[0], v[0] );  return r; }

	MATHCALL inline static biguint_t<S> gcd( int lhs, int rhs ) 					{ return gcd(biguint_t<S>(lhs),(const mpz_t*)biguint_t<S>(rhs)); }
	MATHCALL inline static biguint_t<S> lcm( int lhs, int rhs ) 					{ return lcm(biguint_t<S>(lhs),(const mpz_t*)biguint_t<S>(rhs)); }
	MATHCALL inline static biguint_t<S> nextprime( int v ) 							{ return nextprime((const mpz_t*)biguint_t<S>(v)); }
};

#ifndef BIGMATHNOTYPES
typedef biguint_t<128>   biguint128_t;
typedef biguint_t<256>   biguint256_t;
typedef biguint_t<512>   biguint512_t;
typedef biguint_t<1024>  biguint1024_t;
typedef biguint_t<2048>  biguint2048_t;
typedef biguint_t<4096>  biguint4096_t;
typedef biguint_t<8192>  biguint8192_t;
typedef biguint_t<16384> biguint16384_t;
#endif


//
// integer code
//

template <ssize_t S>
struct bigint_t : biguint_t<S> {

	static_assert(S>0,"error: bigint_t <= 0");

	//initialize
	//cppcheck-suppress duplInheritedMember
	SAFEHEAD(bigint_t)

	//
	// routines
	//

		MATHCALL inline const 	biguint_t<S>* _upcast_const()  const 	{ return static_cast<const biguint_t<S>*>(this); }
		MATHCALL inline 		biguint_t<S>* _upcast() 				{ return static_cast<biguint_t<S>*>(this); }

	MATHCALL inline bigint_t() : biguint_t<S>()									{}							//cppcheck-suppress noExplicitConstructor
	MATHCALL inline bigint_t( int val ) : biguint_t<S>() 		 				{ this->operator=(val); }	//cppcheck-suppress noExplicitConstructor
	MATHCALL inline bigint_t( const mpz_t *rhs ) : biguint_t<S>( rhs ) 			{}							//cppcheck-suppress noExplicitConstructor
	MATHCALL inline bigint_t( const bigint_t  &rhs ) : biguint_t<S>( rhs ) 		{}							//cppcheck-suppress noExplicitConstructor

	MATHCALL inline 		 int 			operator=( int val ) 				{ SAFE() mpz_set_si( this->b.m_v[0], val ); return(val); }
	MATHCALL inline 	 	 mpz_t* 		operator=( const mpz_t *rhs )		{ _upcast()->operator=(rhs); return(this->b.m_v); }
	MATHCALL inline       bigint_t<S> & 	operator=( const bigint_t<S> &rhs )	{ _upcast()->operator=(rhs); return(*this); } //overload to avoid structure copy errors

		MATHCALL inline void _nop() 									const 	{}
		MATHCALL inline int  _abs( const int rhs ) 						const 	{ SAFE() return(rhs>=0?rhs:-rhs); }
		MATHCALL inline bool _eq( const int rhs ) 						const 	{ SAFE() return(mpz_cmp_si( this->b.m_v[0], rhs ) == 0);  }
		MATHCALL inline bool _gt( const int rhs ) 						const 	{ SAFE() return(mpz_cmp_si( this->b.m_v[0], rhs ) > 0);  }
		MATHCALL inline bool _gte( const int rhs ) 						const 	{ SAFE() return(mpz_cmp_si( this->b.m_v[0], rhs ) >= 0); }
		MATHCALL inline bool _lt( const int rhs ) 						const 	{ SAFE() return(mpz_cmp_si( this->b.m_v[0], rhs ) < 0);  }
		MATHCALL inline bool _lte( const int rhs ) 						const 	{ SAFE() return(mpz_cmp_si( this->b.m_v[0], rhs ) <= 0); }
		MATHCALL inline void _add( const int rhs ) 								{ SAFE() if(rhs<0) mpz_sub_ui( this->b.m_v[0], this->b.m_v[0], -rhs ); else mpz_add_ui( this->b.m_v[0], this->b.m_v[0], rhs ); }
		MATHCALL inline void _sub( const int rhs ) 								{ SAFE() if(rhs<0) mpz_add_ui( this->b.m_v[0], this->b.m_v[0], -rhs ); else mpz_sub_ui( this->b.m_v[0], this->b.m_v[0], rhs ); }
		MATHCALL inline void _mul( const int rhs ) 								{ SAFE() mpz_mul_si( this->b.m_vtmp[0], this->b.m_v[0], rhs ); this->b.swap(); }
		MATHCALL inline void _div( const int rhs ) 								{ SAFE() mpz_tdiv_q_ui( this->b.m_vtmp[0], this->b.m_v[0], _abs(rhs) ); this->b.swap(); rhs<0?this->_neg():_nop(); } //tdiv to handle signed
		MATHCALL inline void _mod( const int rhs ) 								{ SAFE() mpz_mod_ui( this->b.m_vtmp[0], this->b.m_v[0], _abs(rhs) ); this->b.swap(); rhs<0?this[0]+=rhs:_nop(); }

	MATHCALL inline void operator+=( const mpz_t *rhs )  						{ _upcast()->_add(rhs); }
	MATHCALL inline void operator-=( const mpz_t *rhs )  						{ _upcast()->_sub(rhs); }
	MATHCALL inline void operator*=( const mpz_t *rhs )  						{ _upcast()->_mul(rhs); }
	MATHCALL inline void operator/=( const mpz_t *rhs )  						{ _upcast()->_div(rhs); }
	MATHCALL inline void operator%=( const mpz_t *rhs )  						{ _upcast()->_mod(rhs); }

	MATHCALL inline void operator+=( const int rhs ) 							{ _add(rhs); }
	MATHCALL inline void operator-=( const int rhs ) 							{ _sub(rhs); }
	MATHCALL inline void operator*=( const int rhs ) 							{ _mul(rhs); }
	MATHCALL inline void operator/=( const int rhs ) 							{ _div(rhs); }
	MATHCALL inline void operator%=( const int rhs ) 							{ _mod(rhs); }

	MATHCALL inline 		 		 const 	biguint_t<S>& 	constbase()	const	{ SAFE() return _upcast_const()[0]; 						}
	MATHCALL inline 		 			 	biguint_t<S>& 	base()		const	{ SAFE() return _upcast()[0]; 								}
	MATHCALL inline 		 			 	mpz_t* 			raw()		const 	{ SAFE() return this->b.m_v; 								}
	MATHCALL inline		 			 const 	char* 			str()		const	{ SAFE() return((const char*)this[0]); 						}
	MATHCALL inline explicit operator 		biguint_t<S>*	()					{ SAFE() return _upcast(); 									}
	MATHCALL inline explicit operator 		unsigned int	() 			const 	{ SAFE() return((unsigned int)mpz_get_si(this->b.m_v[0]));	}	
	MATHCALL inline explicit operator 		int				() 			const 	{ SAFE() return(mpz_get_si(this->b.m_v[0]));  	 			}	
};

#ifndef BIGMATHNOTYPES
typedef bigint_t<128>   bigint128_t;
typedef bigint_t<256>   bigint256_t;
typedef bigint_t<512>   bigint512_t;
typedef bigint_t<1024>  bigint1024_t;
typedef bigint_t<2048>  bigint2048_t;
typedef bigint_t<4096>  bigint4096_t;
typedef bigint_t<8192>  bigint8192_t;
typedef bigint_t<16384> bigint16384_t;
#endif


//
// fractional code
//

template <ssize_t S>
struct bigfrac_t {

	static_assert(S>0,"error: bigfrac_t <= 0");

	//initialize
	SAFEHEAD(bigfrac_t)
	mathbankaccess_t<mpq_t,S,bigfrac_t<S>> b;

	//
	// constant functions for bank
	//

	consteval static size_t LOCALMEMSCALE() { return 3; } //3 to compensate for initial init + numerator + denominator

	//
	// routines
	//

		MATHCALL inline static void _cbinit(mpq_t *v, const size_t bits, _UNUSED_ const int limbs) { 
			mpq_init(v[0]); 
			mpz_realloc2(mpq_numref(v[0]),bits); 
			mpz_realloc2(mpq_denref(v[0]),bits);
		}

		MATHCALL inline static void _cbdeinit(mpq_t *v, _UNUSED_ const size_t bits, _UNUSED_ const int limbs) {
			mpq_clear(v[0]);
		}

		MATHCALL inline static void _cbrealloc(mpq_t *v, const size_t bits, const int limbs) {
			_bigmath_gmp_hacks::mpz_realloc(mpq_numref(v[0]),bits,limbs); 
			_bigmath_gmp_hacks::mpz_realloc(mpq_denref(v[0]),bits,limbs);
		}

		MATHCALL inline static void _prefetch(mpq_t *v) { 
			_bigmath_gmp_hacks::prefetch(mpq_numref(v[0]));
			_bigmath_gmp_hacks::prefetch(mpq_denref(v[0]));
		}

	//user routines
	#define make(e,v) e=mathbankaccess_t<mpq_t,S,bigfrac_t<S>>::bank_t::allocnode(); v=e->m_v;
	#define kill(e)   if(e) { mathbankaccess_t<mpq_t,S,bigfrac_t<S>>::bank_t::freenode(e); }

		MATHCALL inline void _init() 												{ SAFE() make(b.m_e,b.m_v)   make(b.m_etmp,b.m_vtmp) }
	MATHCALL inline ~bigfrac_t() 													{ SAFE() kill(b.m_e)         kill(b.m_etmp)          }

	#undef make
	#undef kill

	MATHCALL inline bigfrac_t()  													{ _init(); }						//cppcheck-suppress noExplicitConstructor
	MATHCALL inline bigfrac_t( int val )  											{ _init(); this->operator=(val); }	//cppcheck-suppress noExplicitConstructor
	MATHCALL inline bigfrac_t( const double &val )  								{ _init(); this->operator=(val); }	//cppcheck-suppress noExplicitConstructor
	MATHCALL inline bigfrac_t( const mpz_t *rhs )  									{ _init(); this->operator=(rhs); }	//cppcheck-suppress noExplicitConstructor
	MATHCALL inline bigfrac_t( const mpq_t *rhs )  									{ _init(); this->operator=(rhs); }	//cppcheck-suppress noExplicitConstructor
	MATHCALL inline bigfrac_t( const bigfrac_t<S> &rhs )  							{ _init(); this->operator=(rhs); }	//cppcheck-suppress noExplicitConstructor

	MATHCALL inline 		int 			operator=( int val ) 					{ SAFE() mpq_set_si(b.m_v[0],val,1); 		return(val); }
	MATHCALL inline const 	double& 		operator=( const double &val ) 			{ SAFE() mpq_set_d(b.m_v[0],val);    		return(val); }
	MATHCALL inline const 	mpz_t* 			operator=( const mpz_t *rhs )	 		{ SAFE() mpq_set_z(b.m_v[0],rhs[0]); 		return(rhs); }
	MATHCALL inline  	 	mpq_t* 			operator=( const mpq_t *rhs ) 			{ SAFE() mpq_set(b.m_v[0],rhs[0]);   		return(b.m_v); }
	MATHCALL inline       	bigfrac_t<S>& 	operator=( const bigfrac_t<S> &rhs )	{ SAFE() mpq_set(b.m_v[0],rhs.b.m_v[0]); 	return(*this); }	//overload to avoid structure copy errors

		MATHCALL inline void 			_neg()		 								{ SAFE() mpq_neg(b.m_v[0],b.m_v[0]); }
		MATHCALL inline void 			_abs()		 								{ SAFE() mpq_abs(b.m_v[0],b.m_v[0]); }
		MATHCALL inline bigfrac_t<S>& 	_inverse()		 							{ SAFE() mpz_swap( mpq_numref(b.m_v[0]), mpq_denref(b.m_v[0]) ); return(this[0]); }

		//TODO: Improve these with direct calls on the numref/denref
		MATHCALL inline void _add( const mpz_t *rhs ) 								{ SAFE() bigfrac_t _rhs(rhs); this->_add(_rhs.b.m_v); }
		MATHCALL inline void _sub( const mpz_t *rhs ) 								{ SAFE() bigfrac_t _rhs(rhs); this->_sub(_rhs.b.m_v); }
		MATHCALL inline void _mul( const mpz_t *rhs ) 								{ SAFE() bigfrac_t _rhs(rhs); this->_mul(_rhs.b.m_v); }
		MATHCALL inline void _div( const mpz_t *rhs ) 								{ SAFE() bigfrac_t _rhs(rhs); this->_div(_rhs.b.m_v); }
		MATHCALL inline bool _eq( const mpz_t *rhs ) 						const 	{ SAFE() bigfrac_t _rhs(rhs); return(this->_eq(_rhs.b.m_v));  }
		MATHCALL inline bool _gt( const mpz_t *rhs ) 						const 	{ SAFE() bigfrac_t _rhs(rhs); return(this->_gt(_rhs.b.m_v));  }
		MATHCALL inline bool _gte( const mpz_t *rhs ) 						const 	{ SAFE() bigfrac_t _rhs(rhs); return(this->_gte(_rhs.b.m_v)); }
		MATHCALL inline bool _lt( const mpz_t *rhs ) 						const 	{ SAFE() bigfrac_t _rhs(rhs); return(this->_lt(_rhs.b.m_v));  }
		MATHCALL inline bool _lte( const mpz_t *rhs ) 						const 	{ SAFE() bigfrac_t _rhs(rhs); return(this->_lte(_rhs.b.m_v)); }

		MATHCALL inline void _add( const mpq_t *rhs ) 								{ SAFE() mpq_add(b.m_vtmp[0], b.m_v[0], rhs[0]); b.swap(); }
		MATHCALL inline void _sub( const mpq_t *rhs ) 								{ SAFE() mpq_sub(b.m_vtmp[0], b.m_v[0], rhs[0]); b.swap(); }
		MATHCALL inline void _mul( const mpq_t *rhs ) 								{ SAFE() mpq_mul(b.m_vtmp[0], b.m_v[0], rhs[0]); b.swap(); }
		MATHCALL inline void _div( const mpq_t *rhs ) 								{ SAFE() mpq_div(b.m_vtmp[0], b.m_v[0], rhs[0]); b.swap(); }
		MATHCALL inline bool _eq( const mpq_t *rhs ) 						const 	{ SAFE() return(mpq_cmp( b.m_v[0], rhs[0] ) == 0);  }
		MATHCALL inline bool _gt( const mpq_t *rhs ) 						const 	{ SAFE() return(mpq_cmp( b.m_v[0], rhs[0] ) > 0);  }
		MATHCALL inline bool _gte( const mpq_t *rhs ) 						const 	{ SAFE() return(mpq_cmp( b.m_v[0], rhs[0] ) >= 0); }
		MATHCALL inline bool _lt( const mpq_t *rhs ) 						const 	{ SAFE() return(mpq_cmp( b.m_v[0], rhs[0] ) < 0);  }
		MATHCALL inline bool _lte( const mpq_t *rhs ) 						const 	{ SAFE() return(mpq_cmp( b.m_v[0], rhs[0] ) <= 0); }

		MATHCALL inline bool _eq( const double &rhs ) 						const 	{ SAFE() return(mpq_get_d( b.m_v[0] ) == rhs);  }
		MATHCALL inline bool _gt( const double &rhs ) 						const 	{ SAFE() return(mpq_get_d( b.m_v[0] ) > rhs);  }
		MATHCALL inline bool _gte( const double &rhs ) 						const 	{ SAFE() return(mpq_get_d( b.m_v[0] ) >= rhs); }
		MATHCALL inline bool _lt( const double &rhs ) 						const 	{ SAFE() return(mpq_get_d( b.m_v[0] ) < rhs);  }
		MATHCALL inline bool _lte( const double &rhs ) 						const 	{ SAFE() return(mpq_get_d( b.m_v[0] ) <= rhs); }

	MATHCALL inline void operator+=( const mpz_t *rhs ) 							{ _add(rhs); }
	MATHCALL inline void operator-=( const mpz_t *rhs ) 							{ _sub(rhs); }
	MATHCALL inline void operator*=( const mpz_t *rhs ) 							{ _mul(rhs); }
	MATHCALL inline void operator/=( const mpz_t *rhs ) 							{ _div(rhs); }

	MATHCALL inline void operator+=( const mpq_t *rhs ) 							{ _add(rhs); }
	MATHCALL inline void operator-=( const mpq_t *rhs ) 							{ _sub(rhs); }
	MATHCALL inline void operator*=( const mpq_t *rhs ) 							{ _mul(rhs); }
	MATHCALL inline void operator/=( const mpq_t *rhs ) 							{ _div(rhs); }

	MATHCALL inline void operator+=( const double &rhs ) 							{ _add(bigfrac_t<S>(rhs)); }
	MATHCALL inline void operator-=( const double &rhs ) 							{ _sub(bigfrac_t<S>(rhs)); }
	MATHCALL inline void operator*=( const double &rhs ) 							{ _mul(bigfrac_t<S>(rhs)); }
	MATHCALL inline void operator/=( const double &rhs ) 							{ _div(bigfrac_t<S>(rhs)); }

	MATHCALL inline 		 			 	mpq_t* 			raw()			const	{ SAFE() return(b.m_v); 							}
	MATHCALL inline		 			 const 	char* 			str()			const	{ SAFE() return((const char*)this[0]); 				}
	MATHCALL inline		 	operator const 	mpq_t* 			()				const	{ SAFE() return(b.m_v); 							}
	MATHCALL inline explicit operator 		double 			() 				const 	{ SAFE() return(mpq_get_d(b.m_v[0]));  				}
	MATHCALL inline explicit operator 		unsigned int 	() 				const 	{ SAFE() return((unsigned int)mpq_get_d(b.m_v[0])); }
	MATHCALL inline explicit operator 		int 			() 				const 	{ SAFE() return((int)mpq_get_d(b.m_v[0])); 			}
	//operator const mpq_t* () <- prevents ambiguity

	MATHCALL operator const char*() const {
		SAFE()
		char *r = b.m_e->getstringmem();
		mathpagingstr_t::g_strdefault.reset();
		mathpagingstr_t::g_activeptr = &mathpagingstr_t::g_strdefault;
		gmp_snprintf( r, BIGMATHSTRBUFFERMAX, "%Qd", b.m_v[0] ); 
		mathpagingstr_t::g_activeptr = &mathpaging_t::g_default;
		return r;
	}
	
	//special routines

	MATHCALL inline bigfrac_t<S> abs() 										const	{ SAFE() bigfrac_t<S> r; mpq_abs(r.b.m_v[0],b.m_v[0]); return(r); }
	MATHCALL inline bigfrac_t<S> inverse()		 							const 	{ SAFE() bigfrac_t<S> r(this[0]); return(r._inverse()); }

	MATHCALL bigint_t<S> round() const { 
		SAFE();
		bigint_t<S> r;
		__mpz_struct *numref = mpq_numref(b.m_v[0]);
		__mpz_struct *denref = mpq_denref(b.m_v[0]);
		mpz_mul_2exp( r.b.m_vtmp[0], numref, 1 );		 				  	//double numerator
		mpz_tdiv_q( r.b.m_v[0], r.b.m_vtmp[0], denref ); 				  	//divide - truncating toward zero
		r += ( 2 * mpz_tstbit( r.b.m_v[0], 0 ) * mpz_sgn( r.b.m_v[0] ) );   //rounding logic
		r /= 2;																//not a shift to avoid issues with negatives
		return(r);
	}
};

#ifndef BIGMATHNOTYPES
typedef bigfrac_t<128>   bigfrac128_t;
typedef bigfrac_t<256>   bigfrac256_t;
typedef bigfrac_t<512>   bigfrac512_t;
typedef bigfrac_t<1024>  bigfrac1024_t;
typedef bigfrac_t<2048>  bigfrac2048_t;
typedef bigfrac_t<4096>  bigfrac4096_t;
typedef bigfrac_t<8192>  bigfrac8192_t;
typedef bigfrac_t<16384> bigfrac16384_t;
#endif


//
// modular code
//

//helper to shorten code
#define _S _bigmath_compile::constabs(S)

//big modular number
template <ssize_t S>
struct bigmod_t : biguint_t<_S> {

	//constant calculation and typedefs
	constexpr static size_t POW2BITS = _bigmath_compile::modgenpow2calc(S);
#ifdef BIGMATHGMPHACKSDISABLE
	constexpr static bool 	POW2FAST = false;
	constexpr static int 	POW2LIMBSLIMIT = 0;
#else
	constexpr static bool 	POW2FAST = (POW2BITS>0) && ((POW2BITS%GMP_NUMB_BITS)==0);
	constexpr static int 	POW2LIMBSLIMIT = POW2BITS/GMP_NUMB_BITS;
#endif
	typedef typename mathbankaccess_t<mpz_t,_S,biguint_t<_S>>::bankentry_t bankentry_t;

	//constants
	constexpr static size_t FLG_CLEAN  = 0x01;

	//initialize
	//cppcheck-suppress duplInheritedMember
	SAFEHEAD(bigmod_t)

	static thread_local bankentry_t	*g_defmodptr;
	bankentry_t 					*m_modptr;
	mutable size_t 		 			m_modflags;

	//
	// routines
	//

		MATHCALL inline const 	biguint_t<_S>* _upcast_const()  const 	{ return static_cast<const biguint_t<_S>*>(this); }
		MATHCALL inline 		biguint_t<_S>* _upcast() 				{ return static_cast<biguint_t<_S>*>(this); }


		// >>> smart pointer management for modulus

			MATHCALL inline void _makenode( bankentry_t **ee ) const { 
				ee[0]=mathbankaccess_t<mpz_t,_S,biguint_t<_S>>::bank_t::allocnode();
				#ifndef NDEBUG
				ee[0]->m_maske = 0;
				#endif
			}

			MATHCALL inline void _killnode( bankentry_t *ee )  const {
				if(ee==g_defmodptr) 	g_defmodptr=0;
				if(POW2FAST==false && POW2BITS>0) { //cppcheck-suppress knownConditionTrueFalse
					ASSERT(ee->m_maske!=0);			//if this assert trips, ensure you are not moving non-pow2 modulis to pow2 modulis
					ee->m_maske->m_bank->freenode(ee->m_maske);	
				}
				else ASSERT(ee->m_maske==0);		//if this assert trips, ensure you are not moving non-pow2 modulis to pow2 modulis
				ee->m_bank->freenode(ee);
			}

			MATHCALL inline void _initpow2mod( bankentry_t *r ) {
				mpz_t &v=r->m_v[0], &vtmp=this->b.m_vtmp[0];
				mpz_set_ui( vtmp, 1 );
				mpz_mul_2exp( v, vtmp, POW2BITS );
				if(POW2FAST==false) {	//cppcheck-suppress knownConditionTrueFalse
					_makenode( &r->m_maske );
					mpz_sub_ui( r->m_maske->m_v[0], v, 1 );
					#if defined(BIGMATHNOMEMWARN)==false && defined(BIGMATHGMPHACKSDISABLE)==false
					printf("[WARN] Bigmath pow2 modulus is not a multiple of %i bits\n",GMP_NUMB_BITS);
					#endif	
				}
			} //r=(2^POW2BITS)-1

			MATHCALL inline bankentry_t* _refmod( bankentry_t *ptr ) 					const 	{ ptr->m_refcnt++; return(ptr); }
			MATHCALL inline void 		 _derefmod( bankentry_t *ptr ) 					const	{ if(--ptr->m_refcnt<=0) _killnode(ptr); }
			MATHCALL inline void 		 _changemod( bankentry_t **a, bankentry_t *b )	const	{ b->m_refcnt++; _derefmod(a[0]); a[0]=b; }

			MATHCALL inline bankentry_t* _genmod( const mpz_t *d ) {
				bankentry_t *r; _makenode(&r);
				if(POW2BITS>0) 	_initpow2mod(r);
				else 			mpz_set( r->m_v[0], d[0] );
				r->m_refcnt=1;
				return(r);
			}

			MATHCALL inline bankentry_t* _genmod( int d ) {
				bankentry_t *r; _makenode(&r);
				if(POW2BITS>0) 	_initpow2mod(r);
				else 			mpz_set_si( r->m_v[0], d );
				r->m_refcnt=1;
				return(r);
			}

			MATHCALL inline bankentry_t* _getdefmod() {
				if(g_defmodptr==0) { return(g_defmodptr=_genmod(1)); }
				return(_refmod(g_defmodptr));
			}

		// >>> maintain number in modulus

			MATHCALL inline void _doclean() const {
				_markclean();
				if(POW2FAST) { //cppcheck-suppress knownConditionTrueFalse
					_bigmath_gmp_hacks::mpz_limbs_limit( this->b.m_v[0], POW2LIMBSLIMIT, m_modptr->m_v[0] );
				}
				else {
					if(POW2BITS>0) 	{ ASSERT(m_modptr->m_maske!=0) 	mpz_and( this->b.m_v[0], this->b.m_v[0], m_modptr->m_maske->m_v[0] ); } 			//and mask for pow2 fields
					else 			{ 								mpz_mod( this->b.m_vtmp[0], this->b.m_v[0], m_modptr->m_v[0] ); this->b.swap(); }	//actual modulus
				}
			}

			MATHCALL inline void _clean() 		const { if ((m_modflags&FLG_CLEAN)==0) _doclean(); 	}
			MATHCALL inline void _markclean() 	const { m_modflags|=FLG_CLEAN;    					}
			MATHCALL inline void _dirty() 		const { m_modflags&=(~FLG_CLEAN); 					}

	MATHCALL inline bigmod_t( 									   			 ) 	: biguint_t<_S>(),	     						m_modptr(_getdefmod()),  		  m_modflags(0)	  	 	 	 {}	//cppcheck-suppress noExplicitConstructor
	MATHCALL inline bigmod_t( 							int d 		   		 ) 	: biguint_t<_S>(),      						m_modptr(_genmod(d)), 			  m_modflags(0)				 {}	//cppcheck-suppress noExplicitConstructor
	MATHCALL inline bigmod_t( 							const mpz_t *d		 ) 	: biguint_t<_S>(),      						m_modptr(_genmod(d)), 			  m_modflags(0)				 {}	//cppcheck-suppress noExplicitConstructor
	MATHCALL inline bigmod_t( int rhs, 				 	const mpz_t *d 		 ) 	: biguint_t<_S>( rhs ), 						m_modptr(_genmod(d)), 			  m_modflags(0) 			 {}	//cppcheck-suppress noExplicitConstructor
	MATHCALL inline bigmod_t( int rhs, 				 	int d 		   		 ) 	: biguint_t<_S>( rhs ), 						m_modptr(_genmod(d)), 			  m_modflags(0) 			 {}	//cppcheck-suppress noExplicitConstructor
	MATHCALL inline bigmod_t( const bigmod_t &rhs				  			 ) 	: biguint_t<_S>( rhs._upcast_const()[0] ),		m_modptr(_refmod(rhs.m_modptr)),  m_modflags(rhs.m_modflags) {} //cppcheck-suppress noExplicitConstructor
	MATHCALL inline bigmod_t( int rhs,				 	bankentry_t &d 	 	 ) 	: biguint_t<_S>( rhs ), 						m_modptr(_refmod(&d)), 	 	  	  m_modflags(0) 			 {} //cppcheck-suppress noExplicitConstructor
	MATHCALL inline bigmod_t( const mpz_t *rhs, 		bankentry_t &d 	 	 ) 	: biguint_t<_S>( rhs ), 						m_modptr(_refmod(&d)),		 	  m_modflags(0) 			 {}	//cppcheck-suppress noExplicitConstructor
	MATHCALL inline bigmod_t( const mpz_t *rhs, 		const mpz_t *d 	 	 ) 	: biguint_t<_S>( rhs ), 						m_modptr(_genmod(d)), 			  m_modflags(0) 			 {} //cppcheck-suppress noExplicitConstructor
	MATHCALL inline bigmod_t( bankentry_t &rhs, 		bankentry_t &d 		 )	: biguint_t<_S>( rhs.m_v ), 					m_modptr(_refmod(&d)),		  	  m_modflags(0) 			 {}	

	MATHCALL inline ~bigmod_t() 															{ SAFE() _derefmod(m_modptr); }
	//the above is deliberately *not* virtual for performance

	MATHCALL inline 		int 					operator=( int rhs )					{ SAFE() _dirty(); 				 				    				    _upcast()->operator=(rhs); return(rhs);   		}
	MATHCALL inline   	  	mpz_t* 					operator=( const mpz_t *rhs )			{ SAFE() _dirty(); 				 				    				    _upcast()->operator=(rhs); return(this->b.m_v); }
	MATHCALL inline        bigmod_t<S>& 			operator=( const bigmod_t<S> &rhs )		{ SAFE() _changemod(&m_modptr,rhs.m_modptr); m_modflags=rhs.m_modflags; _upcast()->operator=(rhs); return(*this); 		} //overload to avoid structure copy errors

	MATHCALL inline void neg()  															{ SAFE() _upcast()->_neg(); _dirty(); }													//dirty

	MATHCALL inline void operator+=(  const mpz_t *rhs )  									{ SAFE() _upcast()->_add(rhs); _dirty(); }												//dirty
	MATHCALL inline void operator-=(  const mpz_t *rhs )  									{ SAFE() _upcast()->_sub(rhs); _dirty(); }												//dirty
	MATHCALL inline void operator*=(  const mpz_t *rhs )  									{ SAFE() _upcast()->_mul(rhs); _doclean(); } 											//reduce now
	MATHCALL inline void operator/=(  const mpz_t *rhs ) 	 								{ SAFE() bigmod_t<S> _rhs(rhs,m_modptr[0]); this[0]*=_rhs._inverse(); }					//multiply reduces
	MATHCALL inline void operator%=(  const mpz_t *rhs )  									{ SAFE() _clean(); _upcast()->_mod(rhs); }												//clean - cannot expand number
	MATHCALL inline void operator<<=( const mpz_t *rhs )  									{ SAFE() bigmod_t<S> _rhs(2,m_modptr[0]); this[0]*=_rhs._pow(rhs); }					//multiply reduces
	MATHCALL inline void operator>>=( const mpz_t *rhs )  									{ SAFE() bigmod_t<S> _rhs(2,m_modptr[0]); this[0]*=_rhs._inverse()._pow(rhs); }			//multiply reduces
	MATHCALL inline void operator&=(  const mpz_t *rhs ) 									{ SAFE() _clean(); _upcast()->_and(rhs); }												//clean - cannot expand number
	MATHCALL inline void operator|=(  const mpz_t *rhs ) 									{ SAFE() _clean(); _upcast()->_or(rhs);  _dirty(); }									//dirty
	MATHCALL inline void operator^=(  const mpz_t *rhs ) 									{ SAFE() _clean(); _upcast()->_xor(rhs); _dirty(); }									//dirty

	MATHCALL inline void operator+=( const int rhs ) 										{ SAFE() _upcast()->_add(rhs); _dirty(); }												//dirty
	MATHCALL inline void operator-=( const int rhs ) 										{ SAFE() _upcast()->_sub(rhs); _dirty(); }												//dirty
	MATHCALL inline void operator*=( const int rhs ) 										{ SAFE() _upcast()->_mul(rhs); _doclean(); }											//reduce now
	MATHCALL inline void operator/=( const int rhs ) 	 									{ SAFE() bigmod_t<S> _rhs(rhs,m_modptr[0]); this[0]*=_rhs._inverse(); }					//multiply reduces
	MATHCALL inline void operator%=( const int rhs ) 										{ SAFE() _clean(); _upcast()->_mod(rhs); }												//clean - cannot expand number
	MATHCALL inline void operator<<=( const int rhs ) 										{ SAFE() bigmod_t<S> _rhs(2,m_modptr[0]); this[0]*=_rhs._pow(rhs); }					//multiply reduces
	MATHCALL inline void operator>>=( const int rhs ) 										{ SAFE() bigmod_t<S> _rhs(2,m_modptr[0]); this[0]*=_rhs._inverse()._pow(rhs); }			//multiply reduces
	MATHCALL inline void operator&=( const int rhs ) 										{ SAFE() _clean(); _upcast()->_and(rhs); }												//clean - cannot expand number
	MATHCALL inline void operator|=( const int rhs ) 										{ SAFE() _clean(); _upcast()->_or(rhs);  _dirty(); }									//dirty
	MATHCALL inline void operator^=( const int rhs ) 										{ SAFE() _clean(); _upcast()->_xor(rhs); _dirty(); }									//dirty

	MATHCALL inline 		 		  const biguint_t<_S>& 	constbase()				const 	{ SAFE() _clean(); return _upcast_const()[0]; 					}
	MATHCALL inline 		 		  		biguint_t<_S>& 	base()							{ SAFE() _clean(); return _upcast()[0]; 						}
	MATHCALL inline 		 				mpz_t* 			raw()					const	{ SAFE() _clean(); return (this->b.m_v); 						}
	MATHCALL inline		 			  const char* 			str()					const	{ SAFE() _clean(); return((const char*)this[0]); 				}
	MATHCALL inline 		 operator const mpz_t*			()						const	{ SAFE() _clean(); return (this->b.m_v); 						}
	MATHCALL inline explicit operator 		biguint_t<_S>*	()						const	{ SAFE() _clean(); return _upcast(); 							}
	MATHCALL inline explicit operator 		unsigned int	() 		 				const	{ SAFE() _clean(); return (unsigned int)(_upcast_const()[0]); 	}
	MATHCALL inline explicit operator 		int				() 				 		const	{ SAFE() _clean(); return (int)(_upcast_const()[0]); 			}
	MATHCALL inline explicit operator const char*			()						const	{ SAFE() _clean(); return (const char*)(_upcast_const()[0]); 	}
	//operator const return of mpz_t is to prevent resolution ambiguity caused if non-const

	//
	// additional modular specific routines
	//

		MATHCALL inline bigmod_t<S>& _inverse() 											{ SAFE() mpz_invert(  this->b.m_vtmp[0], this->b.m_v[0], 		 m_modptr->m_v[0] ); this->b.swap(); _markclean(); return(*this); }
		MATHCALL inline bigmod_t<S>& _pow( const mpz_t *rhs )								{ SAFE() mpz_powm(    this->b.m_vtmp[0], this->b.m_v[0], rhs[0], m_modptr->m_v[0] ); this->b.swap(); _markclean(); return(*this); }
		MATHCALL inline bigmod_t<S>& _pow( int rhs )										{ SAFE() mpz_powm_ui( this->b.m_vtmp[0], this->b.m_v[0], rhs, 	 m_modptr->m_v[0] ); this->b.swap(); _markclean(); return(*this); }

	MATHCALL inline bigmod_t<S> inverse() 											const	{ SAFE() bigmod_t<S> _rhs = this[0]; return(_rhs._inverse()); }
	MATHCALL inline bigmod_t<S> pow( const mpz_t *rhs )								const	{ SAFE() bigmod_t<S> _rhs = this[0]; return(_rhs._pow(rhs));  }
	MATHCALL inline bigmod_t<S> pow( int rhs )										const	{ SAFE() bigmod_t<S> _rhs = this[0]; return(_rhs._pow(rhs));  }

	MATHCALL inline void 			changemod( const mpz_t *rhs ) 							{ SAFE() _changemod(&m_modptr,_genmod(rhs)); _dirty(); }
	MATHCALL inline void 			changemod( bankentry_t &rhs ) 							{ SAFE() _changemod(&m_modptr,&rhs); _dirty(); }
	MATHCALL inline bankentry_t&	getmodentry() 									const 	{ SAFE() return(m_modptr[0]); }
	MATHCALL inline mpz_t*			getmod() 										const 	{ SAFE() return(m_modptr->raw()); }

	//nested chinese remainder theorem solver structure to encourage/simplify optimized memory usage
	template <size_t SR, int sz>
	struct crtsolver {

		static_assert(S>0,"Modular CRT should not use a base type of a power of two.");

		//internal memory
		bigmod_t<S> m_s1[sz], m_s2[sz];

		//chinese remainder optimized for big numbers - avoids big multiplies and big modulus reductions
		MATHCALL biguint_t<SR> crt1( const bigmod_t<S> *v ) {
			int x, y;
			bigmod_t<S> delta;
			biguint_t<SR> r(v[0]), scale(1);
			for(x=1;x<sz;x++) {
				m_s1[x] = bigmod_t<S>( v[0],			 	  v[x].getmodentry() );		//set all moduli to value in first mod
				m_s2[x] = bigmod_t<S>( v[0].getmodentry(), v[x].getmodentry() );		//set all multipliers for all moduli to first mod
			}
			for(x=1;x<sz;x++) {
				delta=(v[x]-m_s1[x])/m_s2[x];	//calculate steps to align to answer in field mod[x]
				for(y=x+1;y<sz;y++) {
					m_s1[y]+=m_s2[y]*delta;		//update answers in remaining mods - s2 on lefthand to prefer s2 modulus
					m_s2[y]*=v[x].getmod();		//update multipliers in remaining mods
				}
				scale*=v[x-1].getmod();			//update global multiplier, lags behind to prevent unnecessary tail multiply
				r+=scale*delta;					//update the return answer we are calculating - scale on lefthand for biguint result
			}
			return r;
		}

		//chinese remainder - more direct but larger calculations
		MATHCALL biguint_t<SR> crt2( const bigmod_t<S> *v ) {
			int x;
			bigmod_t<S> delta;
			biguint_t<SR> r(v[0]), scale(1);
			for(x=1;x<sz;x++) {
				scale*=v[x-1].getmod();			//update global multiplier, lags behind to prevent unnecessary tail multiply
				delta=(v[x]-r)/scale;			//calculate steps to align to answer in field mod[x]
				r+=scale*delta;					//update the return answer we are calculating - scale on lefthand for biguint result
			}
			return r;
		}		
	};

};

template <ssize_t S>
thread_local typename mathbankaccess_t<mpz_t,_S,biguint_t<_S>>::bankentry_t *bigmod_t<S>::g_defmodptr = 0;

#undef _S

#ifndef BIGMATHNOTYPES
typedef bigmod_t<128>   bigmod128_t;
typedef bigmod_t<256>   bigmod256_t;
typedef bigmod_t<512>   bigmod512_t;
typedef bigmod_t<1024>  bigmod1024_t;
typedef bigmod_t<2048>  bigmod2048_t;
typedef bigmod_t<4096>  bigmod4096_t;
typedef bigmod_t<8192>  bigmod8192_t;
typedef bigmod_t<16384> bigmod16384_t;
#endif


//
// long-lived stream acceleration numbers
//

template <ssize_t S>
struct bigstream_t : bigmod_t<-S> {

	static_assert(S>0,"error: bigstream_t <= 0");

	//initialize
	//cppcheck-suppress duplInheritedMember
	SAFEHEAD(bigstream_t)
	
	//
	// routines
	//
	
	MATHCALL inline bigstream_t() : bigmod_t<-S>() {}
	
	
};

#ifndef BIGMATHNOTYPES
typedef bigstream_t<128>   uint128_t;
typedef bigstream_t<256>   uint256_t;
typedef bigstream_t<512>   uint512_t;
typedef bigstream_t<1024>  uint1024_t;
typedef bigstream_t<2048>  uint2048_t;
typedef bigstream_t<4096>  uint4096_t;
typedef bigstream_t<8192>  uint8192_t;
typedef bigstream_t<16384> uint16384_t;
#endif


//
// global operators
//

#include "bigmathoperators.h"

#undef MATHCALL

#endif
