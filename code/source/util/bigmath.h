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

//Trouble compiling:
//	- Check for use of "const" declaration on your bigmath types (supporting const for all edge cases negatively impacts performance)
//Performance notes:
//	- Prefer constructor initializer lists for structure members
//	- Avoid the use of virtual declaration of functions in structures
//	- Prefer non-const return values *if* it's safe, does not cause compile ambiguity, and does not cause unnecessary implicit copies
//	- Prefer const input parameters UNLESS:
//		- function is returning a type that can be the result of optimized operations on a non-const input value
//		- it causes inefficient use of the type as part of an operation (e.g. internal scratch memory and modulus finalization automation)
//	- Prefer const function declarations where possible 
//	- Prefer use of the address accessing (&) for input parameters where possible for anything other than integers
//	- Explicitly define global arithmetic operators with GMP & standard types to curate optimal performance (optimal copies etc...)
//	- Avoid defining global arithmetic operators that take "const" for all custom types as this may encourage inefficient use of memory
//	- Take advantages of the pre-allocated temporary memory & swap() operation provided if it will be faster for a GMP operation
//	- Use single linked lists where possible if such a construct is needed
//Style: 
//	- Do not override const warnings with a cast to eliminate const-ness, find a way to optimize your code instead without violating const contract
//	- User-accessible non-operator functions that return internal data types with accessible members/functions are returned by reference / all others by pointer
//  - Global operators give return type precedence to lefthand type unless operation includes a fraction or a standard data type:
//			 uint_t * mod_t  = uint_t
//			 uint_t * frac_t = frac_t
//			 int    * frac_t = frac_t
//			 double * frac_t = frac_t
//			 frac_t * int    = frac_t

#ifndef BIGMATH_H
#define BIGMATH_H

//below function must be implemented in the calling program to capture functions 
//	to call to clean up number caches - this is necessary to avoid a memory leak
extern "C" { 
	void __thread_function_cleaner_add__( void (*cleaner)(void) );
}

#include <new>
#include <gmp.h>
#include "linkedlist.h"
#include "memsafety.h"

//
// math memory management template
//

#ifndef BIGMATHMAXSCALE	//max scaling of number over its base size
#define BIGMATHMAXSCALE 3
#endif

#ifndef BIGMATHCACHESIZE //optimization for L1 CPU caches without compromising on bank sizes, measured in bits - beware this is PER instantiated type
#define BIGMATHCACHESIZE (32*1024*8)
#endif

#ifndef BIGMATHMODSCALE	//can be lowered below the max memory scaling if there is a computational advantage to it
#define BIGMATHMODSCALE BIGMATHMAXSCALE
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

static_assert(BIGMATHMODSCALE<=BIGMATHMAXSCALE,"BIGMATHMODSCALE must be <= BIGMATHMAXSCALE");

//
// helper routines to minimize template code and optimize resulting assembly
//

//requires >= c++20 to evaluate constants
static_assert(__cplusplus > 201703L,"c++ version must be >= 2020");

namespace _bigmath_compile {

	consteval bool 			isneg( int a ) 			{ return(a<0?true:false); 				}
	consteval unsigned int 	constabs( int a ) 		{ return(a<0?-a:a); 					}
	consteval unsigned int 	modcastszup( int a ) 	{ return constabs(a)*BIGMATHMODSCALE; 	}
	consteval unsigned int 	modcastszdown( int a )	{ return constabs(a)/BIGMATHMODSCALE; 	}
	consteval unsigned int	modpow2calc( int a ) 	{ return isneg(a)?constabs(a):0; 		}

		consteval int _log2( int val, int c ) {
			int r = val>>c;
			if(r==0) return c;
			return _log2(val,c+1);
		}

	consteval unsigned int	compute_cache_pow2_size( const int cachebits, const int numbits ) {
		int log2  = _log2(cachebits/numbits,0)-1;
		int log2f = log2>1?log2:1;
		return(1<<log2f);
	}

}


//
// memory bank routines to speed up math objects
//

template <typename T, int S, typename CBT>
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

	struct bankentry_t {

		static thread_local char		g_strbuffer[BIGMATHSTRBUFFERMAX*BIGMATHSTRQUEUEMAX];	//thread specific to avoid mutex locks
		static thread_local int			g_strbufferpos;											//thread specific to avoid mutex locks

		linkitem_single<bankentry_t>	m_item;
		bank_t 							*m_bank;
		T	 							*m_v;
		bankentry_t						*m_maske;
		int								m_refcnt;

		inline bankentry_t() : m_item(this), m_bank(), m_v(), m_maske(), m_refcnt() {}

		inline char *getstringmem() {
			char *handle;
			if(g_strbufferpos>BIGMATHSTRBUFFERMAX*(BIGMATHSTRQUEUEMAX-1)) g_strbufferpos=0;
			handle = &g_strbuffer[g_strbufferpos];
			g_strbufferpos+=BIGMATHSTRBUFFERMAX;
			return(handle);
		}

		inline T *raw() { return(m_v); }

	};

	struct bank_t {

		SAFEHEAD(bank_t)

		static const int BANKSIZE  = BIGMATHBANKSIZE;

		const unsigned int C_ALLOC_LIMBS = (S/mp_bits_per_limb+(S%mp_bits_per_limb==0?0:1)+1);
		const unsigned int C_ALLOC_BITS  = C_ALLOC_LIMBS*mp_bits_per_limb;

		static const unsigned int CACHESIZE = _bigmath_compile::compute_cache_pow2_size(BIGMATHCACHESIZE,S);
		static const unsigned int CACHEMASK = CACHESIZE-1;

		static thread_local	bankentry_t						*g_cache[CACHESIZE];
		static thread_local	unsigned int 					g_cachestore, g_cachefetch;

		static thread_local linkbase<bank_t> 				g_base, g_freebase;	//thread specific to avoid mutex locks
							linkitem<bank_t> 				m_item, m_freeitem;
							linkbase_single<bankentry_t> 	m_freenodebase;

		T	 			m_v[BANKSIZE];
		bankentry_t 	m_nodes[BANKSIZE];
		int 			m_usedcount, m_freecount;

		inline bool isbankfree() { SAFE(); return(m_freenodebase.last() || m_usedcount<BANKSIZE); }

		bank_t() : m_item(this), m_freeitem(this), m_usedcount(0), m_freecount(0) {
			g_preloader.compile();	//force compiler to include preloader for g_cache - it will be optimized out otherwise
			SAFE()
			int x;
			g_base.add(&m_item);
			g_freebase.add(&m_freeitem);
			for(x=0;x<BANKSIZE;x++)  CBT::_cbinit(&m_v[x],C_ALLOC_BITS);
		}

		~bank_t() {
			SAFE()
			g_base.remove(&m_item);
			if(isbankfree()) g_freebase.remove(&m_freeitem);
			for(int x=0;x<BANKSIZE;x++) CBT::_cbdeinit(&m_v[x],C_ALLOC_BITS);
		}

			static bankentry_t *_allocfrombank() {
				
				bankentry_t 	*node;
				bank_t 			*bank = g_freebase.first();

				if(bank==0) {
					if(!(bank=new bank_t)) { throw std::bad_alloc(); }
				}

				if((node = bank->m_freenodebase.last())==0) {
					node = &bank->m_nodes[bank->m_usedcount];
					node->m_bank 		= bank;
					node->m_v			= &bank->m_v[bank->m_usedcount++];
				}
				else {
					bank->m_freenodebase.remove(&node->m_item);
					bank->m_freecount--;
				}

				if(bank->isbankfree()==false) g_freebase.remove(&bank->m_freeitem);

				return node;

			}
			
			inline static bankentry_t *_cachefetch() {
				bankentry_t *e;
				if((e=g_cache[g_cachefetch])) { 
					g_cache[g_cachefetch]=0;
					g_cachefetch=(g_cachefetch+1)&CACHEMASK;	//fast wrap for pow2 field
					return e;
				}
				return(_allocfrombank());
			}

		inline static bankentry_t *allocnode() { return(_cachefetch()); }

			inline void _cleangmp( bankentry_t *e ) {
				CBT::_cbrealloc(&e->m_v[0],C_ALLOC_BITS);				//reset gmp memory of this entry - prevents GMP memory creep - performance hit without custom memory manager
			}

			void _freetobank( bankentry_t *e ) {
				SAFE()
				m_freenodebase.add(&e->m_item);							//add node to bank's freenodebase
				if(isbankfree()==false) g_freebase.add(&m_freeitem);	//add bank to thread's list of banks with free elements
				_cleangmp(e);											//reset gmp memory of this entry - prevents memory creep
				if(++m_freecount>=m_usedcount) delete this;				//free this bank if all items free
			}

			inline static void _cachestore( bankentry_t *e ) {
				if(g_cache[g_cachestore]) { 
					g_cache[g_cachestore]->m_bank->_freetobank( g_cache[g_cachestore] ); 
					g_cachefetch=(g_cachestore+1)&CACHEMASK; 	//move fetch up so it's using the oldest object in cache
				}
				g_cache[g_cachestore]=e;
				g_cachestore=(g_cachestore+1)&CACHEMASK;		//fast wrap for pow2 field
			}

		static inline void freenode( bankentry_t *e ) { _cachestore(e); }

	};

	struct bankpreload_t {

		static void unloadcache() {
			unsigned int x;
			for(x=0;x<bank_t::CACHESIZE;x++) {
				if(bank_t::g_cache[x]) bank_t::g_cache[x]->m_bank->_freetobank(bank_t::g_cache[x]);
			}
		}

		bankpreload_t() {
			unsigned int x;
			for(x=0;x<bank_t::CACHESIZE;x++) bank_t::g_cache[x] = bank_t::_allocfrombank();
			__thread_function_cleaner_add__(&unloadcache);
		}

		inline void compile() {} //invoked by bank_t() constructor to prevent optimizing out the preloader

	};

	static thread_local bankpreload_t g_preloader;	//preloading of number cache + memory manager at thread init

	//
	// local (per instatiation)
	//

	bankentry_t *m_e, *m_etmp;	//no need to swap these during runtime
	T		  	*m_v, *m_vtmp;

	inline void swap() {
		SAFE()
		T *swapv;
		swapv = m_v; m_v = m_vtmp; m_vtmp = swapv;
	}

	inline void swaptmp( bankentry_t **target ) {

		SAFE()
		bankentry_t *swape;

		if(m_vtmp==m_e->m_v) 	{ swape = m_e;    m_e    = target[0]; }
		else 					{ swape = m_etmp; m_etmp = target[0]; }

		m_vtmp    = target[0]->m_v;
		target[0] = swape;

	}

};

template <typename T, int S, typename CBT>
thread_local char 														mathbankaccess_t<T,S,CBT>::bankentry_t::g_strbuffer[BIGMATHSTRBUFFERMAX*BIGMATHSTRQUEUEMAX];

template <typename T, int S, typename CBT>
thread_local int 														mathbankaccess_t<T,S,CBT>::bankentry_t::g_strbufferpos = 0;

template <typename T, int S, typename CBT>
thread_local typename mathbankaccess_t<T,S,CBT>::bankentry_t 			*mathbankaccess_t<T,S,CBT>::bank_t::g_cache[CACHESIZE];

template <typename T, int S, typename CBT>
thread_local unsigned int 												mathbankaccess_t<T,S,CBT>::bank_t::g_cachestore=0;

template <typename T, int S, typename CBT>
thread_local unsigned int 												mathbankaccess_t<T,S,CBT>::bank_t::g_cachefetch=0;

template <typename T, int S, typename CBT>
thread_local linkbase<typename mathbankaccess_t<T,S,CBT>::bank_t> 		mathbankaccess_t<T,S,CBT>::bank_t::g_base;

template <typename T, int S, typename CBT>
thread_local linkbase<typename mathbankaccess_t<T,S,CBT>::bank_t> 		mathbankaccess_t<T,S,CBT>::bank_t::g_freebase;

template <typename T, int S, typename CBT>
thread_local typename mathbankaccess_t<T,S,CBT>::bankpreload_t 			mathbankaccess_t<T,S,CBT>::g_preloader;


//
// uint code
//

template <int S>
struct bigmod_t;

template <int S>
struct bigfrac_t;

template <int S>
struct biguint_t {

	static_assert(S>0,"error: biguint_t <= 0");

	SAFEHEAD(biguint_t)
	mathbankaccess_t<mpz_t,S,biguint_t<S>> b;

	//
	// routines
	//

		//callbacks for the mathbank
		inline static void _cbinit(mpz_t *v, _UNUSED_ int size) 	 		{ mpz_init2(v[0],size); 	}
		inline static void _cbdeinit(mpz_t *v, _UNUSED_ int size) 			{ mpz_clear(v[0]); 		 	}
		inline static void _cbrealloc(mpz_t *v, _UNUSED_ int size) 			{ mpz_realloc2(v[0],size); 	}

	//user routines
	#define make(e,v) { e=mathbankaccess_t<mpz_t,S,biguint_t<S>>::bank_t::allocnode(); v=e->m_v; }
	#define kill(e)   if(e) { mathbankaccess_t<mpz_t,S,biguint_t<S>>::bank_t::freenode(e); }

		inline void _init() 												{ SAFE() make(b.m_e,b.m_v)   make(b.m_etmp,b.m_vtmp) }
	inline ~biguint_t() 													{ SAFE() kill(b.m_e)         kill(b.m_etmp)          }
	//the above is deliberately *not* virtual for performance

	#undef make
	#undef kill

	#define _S1 S
	#define _S2 -S
	#define _S3 _bigmath_compile::modcastszdown(S)
	#define _S4 -_bigmath_compile::modcastszdown(S)

	inline biguint_t()  													{ _init(); }						//cppcheck-suppress noExplicitConstructor
	inline biguint_t( int val )  											{ _init(); this->operator=(val); }	//cppcheck-suppress noExplicitConstructor
	inline biguint_t( const mpz_t *rhs ) 		 							{ _init(); this->operator=(rhs); }	//cppcheck-suppress noExplicitConstructor
	inline biguint_t( const biguint_t &rhs ) 								{ _init(); this->operator=(rhs); }	//cppcheck-suppress noExplicitConstructor
	inline biguint_t( const bigmod_t<_S1> &rhs )							{ _init(); this->operator=(rhs); }	//cppcheck-suppress noExplicitConstructor
	inline biguint_t( const bigmod_t<_S2> &rhs )							{ _init(); this->operator=(rhs); }	//cppcheck-suppress noExplicitConstructor
	inline biguint_t( const bigmod_t<_S3> &rhs )							{ _init(); this->operator=(rhs); }	//cppcheck-suppress noExplicitConstructor
	inline biguint_t( const bigmod_t<_S4> &rhs )							{ _init(); this->operator=(rhs); }	//cppcheck-suppress noExplicitConstructor

	inline 		 int 				operator=( int val ) 					{ SAFE() mpz_set_ui(b.m_v[0],(unsigned long int)val); return(val); }
	inline 	     mpz_t* 			operator=( const mpz_t *rhs )			{ SAFE() mpz_set(b.m_v[0],rhs[0]);  return(b.m_v); }
	inline 	 	 biguint_t<S> & 	operator=( const biguint_t<S> &rhs )	{ SAFE() mpz_set(b.m_v[0],rhs.b.m_v[0]); return(*this); }				//overload to avoid structure copy errors
	inline const bigmod_t<_S1> & 	operator=( const bigmod_t<_S1> &rhs ) 	{ SAFE() rhs.copyraw(b.m_v); return(rhs); }								//overload to avoid structure copy errors
	inline const bigmod_t<_S2> & 	operator=( const bigmod_t<_S2> &rhs ) 	{ SAFE() rhs.copyraw(b.m_v); return(rhs); }								//overload to avoid structure copy errors
	inline const bigmod_t<_S3> & 	operator=( const bigmod_t<_S3> &rhs ) 	{ SAFE() rhs.copyraw(b.m_v); return(rhs); }								//overload to avoid structure copy errors
	inline const bigmod_t<_S4> & 	operator=( const bigmod_t<_S4> &rhs ) 	{ SAFE() rhs.copyraw(b.m_v); return(rhs); }								//overload to avoid structure copy errors

	#undef _S1 
	#undef _S2 
	#undef _S3 
	#undef _S4 

		inline void _neg()		 											{ SAFE() mpz_neg(b.m_v[0],b.m_v[0]); }
		inline void _abs()		 											{ SAFE() mpz_abs(b.m_v[0],b.m_v[0]); }

		inline bool _eq( const mpq_t *rhs ) 						const 	{ SAFE() return( bigfrac_t<S>( this[0] ) == rhs ); }
		inline bool _gt( const mpq_t *rhs ) 						const 	{ SAFE() return( bigfrac_t<S>( this[0] ) > rhs ); }
		inline bool _gte( const mpq_t *rhs ) 						const 	{ SAFE() return( bigfrac_t<S>( this[0] ) >= rhs ); }
		inline bool _lt( const mpq_t *rhs ) 						const 	{ SAFE() return( bigfrac_t<S>( this[0] ) < rhs ); }
		inline bool _lte( const mpq_t *rhs ) 						const 	{ SAFE() return( bigfrac_t<S>( this[0] ) <= rhs ); }

		inline bool _eq( const mpz_t *rhs ) 						const 	{ SAFE() return(mpz_cmp( b.m_v[0], rhs[0] ) == 0);  }
		inline bool _gt( const mpz_t *rhs ) 						const 	{ SAFE() return(mpz_cmp( b.m_v[0], rhs[0] ) > 0);  }
		inline bool _gte( const mpz_t *rhs ) 						const 	{ SAFE() return(mpz_cmp( b.m_v[0], rhs[0] ) >= 0); }
		inline bool _lt( const mpz_t *rhs ) 						const 	{ SAFE() return(mpz_cmp( b.m_v[0], rhs[0] ) < 0);  }
		inline bool _lte( const mpz_t *rhs ) 						const 	{ SAFE() return(mpz_cmp( b.m_v[0], rhs[0] ) <= 0); }
		inline void _add( const mpz_t *rhs ) 								{ SAFE() mpz_add( b.m_v[0], b.m_v[0], rhs[0] ); }
		inline void _sub( const mpz_t *rhs ) 								{ SAFE() mpz_sub( b.m_v[0], b.m_v[0], rhs[0] ); }
		inline void _mul( const mpz_t *rhs ) 								{ SAFE() mpz_mul( b.m_vtmp[0], b.m_v[0], rhs[0] ); b.swap(); }
		inline void _div( const mpz_t *rhs ) 								{ SAFE() mpz_tdiv_q( b.m_vtmp[0], b.m_v[0], rhs[0] ); b.swap(); } //tdiv to handle signed overload
		inline void _mod( const mpz_t *rhs ) 								{ SAFE() mpz_mod( b.m_vtmp[0], b.m_v[0], rhs[0] ); b.swap(); }
		inline void _and( const mpz_t *rhs ) 								{ SAFE() mpz_and( this->b.m_v[0], this->b.m_v[0], rhs[0] ); }
		inline void _or( const mpz_t *rhs ) 								{ SAFE() mpz_ior( this->b.m_v[0], this->b.m_v[0], rhs[0] ); }
		inline void _xor( const mpz_t *rhs ) 								{ SAFE() mpz_xor( this->b.m_v[0], this->b.m_v[0], rhs[0] ); }

		inline bool _eq( const int rhs ) 							const 	{ SAFE() return(mpz_cmp_ui( b.m_v[0], (unsigned long int)rhs ) == 0);  }
		inline bool _gt( const int rhs ) 							const 	{ SAFE() return(mpz_cmp_ui( b.m_v[0], (unsigned long int)rhs ) > 0);  }
		inline bool _gte( const int rhs ) 							const 	{ SAFE() return(mpz_cmp_ui( b.m_v[0], (unsigned long int)rhs ) >= 0); }
		inline bool _lt( const int rhs ) 							const 	{ SAFE() return(mpz_cmp_ui( b.m_v[0], (unsigned long int)rhs ) < 0);  }
		inline bool _lte( const int rhs ) 							const 	{ SAFE() return(mpz_cmp_ui( b.m_v[0], (unsigned long int)rhs ) <= 0); }
		inline void _add( const int rhs ) 									{ SAFE() mpz_add_ui( b.m_v[0], b.m_v[0], (unsigned long int)rhs ); }
		inline void _sub( const int rhs ) 									{ SAFE() mpz_sub_ui( b.m_v[0], b.m_v[0], (unsigned long int)rhs ); }
		inline void _mul( const int rhs ) 									{ SAFE() mpz_mul_ui( b.m_vtmp[0], b.m_v[0], (unsigned long int)rhs ); b.swap(); }
		inline void _div( const int rhs ) 									{ SAFE() mpz_fdiv_q_ui( b.m_vtmp[0], b.m_v[0], (unsigned long int)rhs ); b.swap(); } //fdiv as should never be used for signed cases
		inline void _mod( const int rhs ) 									{ SAFE() mpz_mod_ui( b.m_vtmp[0], b.m_v[0], (unsigned long int)rhs ); b.swap(); }
		inline void _lsh( const int rhs ) 									{ SAFE() mpz_mul_2exp( b.m_vtmp[0], b.m_v[0], rhs ); b.swap(); }
		inline void _rsh( const int rhs ) 									{ SAFE() mpz_fdiv_q_2exp( b.m_vtmp[0], b.m_v[0], rhs ); b.swap();  }
		inline void _and( const int rhs ) 									{ SAFE() this[0]&=(biguint_t<S>(rhs)); }
		inline void _or( const int rhs ) 									{ SAFE() this[0]|=(biguint_t<S>(rhs)); }
		inline void _xor( const int rhs ) 									{ SAFE() this[0]^=(biguint_t<S>(rhs)); }

	double double_from_div( const mpz_t *d, int precision=64 ) const {

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

	inline void operator+=( const mpz_t *rhs ) 								{ _add(rhs); }
	inline void operator-=( const mpz_t *rhs ) 								{ _sub(rhs); }
	inline void operator*=( const mpz_t *rhs ) 								{ _mul(rhs); }
	inline void operator/=( const mpz_t *rhs ) 								{ _div(rhs); }
	inline void operator%=( const mpz_t *rhs ) 								{ _mod(rhs); }
	inline void operator&=( const mpz_t *rhs ) 								{ _and(rhs); }
	inline void operator|=( const mpz_t *rhs ) 								{ _or(rhs);  }
	inline void operator^=( const mpz_t *rhs ) 								{ _xor(rhs); }

	inline void operator+=( const int rhs ) 								{ _add(rhs); }
	inline void operator-=( const int rhs ) 								{ _sub(rhs); }
	inline void operator*=( const int rhs ) 								{ _mul(rhs); }
	inline void operator/=( const int rhs ) 								{ _div(rhs); }
	inline void operator%=( const int rhs ) 								{ _mod(rhs); }
	inline void operator<<=( const int rhs ) 								{ _lsh(rhs); }
	inline void operator>>=( const int rhs ) 								{ _rsh(rhs); }
	inline void operator&=( const int rhs ) 								{ _and(rhs); }
	inline void operator|=( const int rhs ) 								{ _or(rhs);  }
	inline void operator^=( const int rhs ) 								{ _xor(rhs); }

	inline 		 			 	   mpz_t* raw()						const 	{ SAFE() return(b.m_v); 					}
	inline		 			 const 	char* str()						const	{ SAFE() return((const char*)this[0]); 		}
	inline 			operator const mpz_t*()							const	{ SAFE() return(b.m_v); 					}
	inline explicit operator 	   double() 						const 	{ SAFE() return(mpz_get_d(b.m_v[0]));  		}
	inline explicit operator 	   unsigned int() 					const 	{ SAFE() return(mpz_get_ui(b.m_v[0]));   	}
	inline explicit operator 	   int() 							const 	{ SAFE() return((int)mpz_get_ui(b.m_v[0])); }
	//operator const return of mpz_t is to prevent resolution ambiguity introduced by non-const

	explicit operator const char*() const {
		SAFE()
		char *r = b.m_e->getstringmem();
		gmp_snprintf( r, BIGMATHSTRBUFFERMAX, "%Zd", b.m_v[0] ); 
		return r;
	}

	//
	// global routines
	//

	inline static biguint_t<S> gcd( const mpz_t *lhs, const mpz_t *rhs ) 	{ biguint_t<S> r; mpz_gcd( r.b.m_v[0], lhs[0], rhs[0] ); return r; }
	inline static biguint_t<S> lcm( const mpz_t *lhs, const mpz_t *rhs ) 	{ biguint_t<S> r; mpz_lcm( r.b.m_v[0], lhs[0], rhs[0] ); return r; }
	inline static biguint_t<S> nextprime( const mpz_t *v ) 					{ biguint_t<S> r; mpz_nextprime( r.b.m_v[0], v[0] );  return r; }

	inline static biguint_t<S> gcd( int lhs, int rhs ) 						{ return gcd(biguint_t<S>(lhs),(const mpz_t*)biguint_t<S>(rhs)); }
	inline static biguint_t<S> lcm( int lhs, int rhs ) 						{ return lcm(biguint_t<S>(lhs),(const mpz_t*)biguint_t<S>(rhs)); }
	inline static biguint_t<S> nextprime( int v ) 							{ return nextprime((const mpz_t*)biguint_t<S>(v)); }
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

template <int S>
struct bigint_t : biguint_t<S> {

	static_assert(S>0,"error: bigint_t <= 0");

	//initialize
	//cppcheck-suppress duplInheritedMember
	SAFEHEAD(bigint_t)

	//
	// routines
	//

		inline biguint_t<S>* _upcast() { return static_cast<biguint_t<S>*>(this); }

	inline bigint_t() : biguint_t<S>()									{}							//cppcheck-suppress noExplicitConstructor
	inline bigint_t( int val ) : biguint_t<S>() 		 				{ this->operator=(val); }	//cppcheck-suppress noExplicitConstructor
	inline bigint_t( const mpz_t *rhs ) : biguint_t<S>( rhs ) 			{}							//cppcheck-suppress noExplicitConstructor
	inline bigint_t( const bigint_t  &rhs ) : biguint_t<S>( rhs ) 		{}							//cppcheck-suppress noExplicitConstructor

	inline 		 int 			operator=( int val ) 					{ SAFE() mpz_set_si( this->b.m_v[0], val ); return(val); }
	inline 	 	 mpz_t* 		operator=( const mpz_t *rhs )			{ _upcast()->operator=(rhs); return(this->b.m_v); }
	inline       bigint_t<S> & 	operator=( const bigint_t<S> &rhs )		{ _upcast()->operator=(rhs); return(*this); } //overload to avoid structure copy errors

		inline bool _eq( const int rhs ) 						const 	{ SAFE() return(mpz_cmp_si( this->b.m_v[0], rhs ) == 0);  }
		inline bool _gt( const int rhs ) 						const 	{ SAFE() return(mpz_cmp_si( this->b.m_v[0], rhs ) > 0);  }
		inline bool _gte( const int rhs ) 						const 	{ SAFE() return(mpz_cmp_si( this->b.m_v[0], rhs ) >= 0); }
		inline bool _lt( const int rhs ) 						const 	{ SAFE() return(mpz_cmp_si( this->b.m_v[0], rhs ) < 0);  }
		inline bool _lte( const int rhs ) 						const 	{ SAFE() return(mpz_cmp_si( this->b.m_v[0], rhs ) <= 0); }
		inline void _add( const int rhs ) 								{ SAFE() bigint_t<S> r(rhs); this[0]+=r; } //invokes below += overload to forward to unsigned processing
		inline void _sub( const int rhs ) 								{ SAFE() bigint_t<S> r(rhs); this[0]-=r; } //invokes below -= overload to forward to unsigned processing
		inline void _mul( const int rhs ) 								{ SAFE() mpz_mul_si( this->b.m_vtmp[0], this->b.m_v[0], rhs ); this->b.swap(); }
		inline void _div( const int rhs ) 								{ SAFE() bigint_t<S> r(rhs); this[0]/=r; } //invokes below /= overload to forward to unsigned processing
		inline void _mod( const int rhs ) 								{ SAFE() bigint_t<S> r(rhs); this[0]%=r; } //invokes below %= overload to forward to unsigned processing

	inline void operator+=( const mpz_t *rhs )  						{ _upcast()->_add(rhs); }
	inline void operator-=( const mpz_t *rhs )  						{ _upcast()->_sub(rhs); }
	inline void operator*=( const mpz_t *rhs )  						{ _upcast()->_mul(rhs); }
	inline void operator/=( const mpz_t *rhs )  						{ _upcast()->_div(rhs); }
	inline void operator%=( const mpz_t *rhs )  						{ _upcast()->_mod(rhs); }

	inline void operator+=( const int rhs ) 							{ _add(rhs); }
	inline void operator-=( const int rhs ) 							{ _sub(rhs); }
	inline void operator*=( const int rhs ) 							{ _mul(rhs); }
	inline void operator/=( const int rhs ) 							{ _div(rhs); }
	inline void operator%=( const int rhs ) 							{ _mod(rhs); }

	inline 		 			 		biguint_t<S>& base()		const	{ SAFE() return _upcast()[0]; 								}
	inline 		 			 	 	mpz_t* raw()				const 	{ SAFE() return this->b.m_v; 								}
	inline		 			 const 	char* str()					const	{ SAFE() return((const char*)this[0]); 						}
	inline explicit operator 		biguint_t<S>*()				const 	{ SAFE() return _upcast(); 									}
	inline explicit operator 		unsigned int() 				const 	{ SAFE() return((unsigned int)mpz_get_si(this->b.m_v[0]));	}	
	inline explicit operator 		int() 						const 	{ SAFE() return(mpz_get_si(this->b.m_v[0]));  	 			}	
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

template <int S>
struct bigfrac_t {

	static_assert(S>0,"error: bigfrac_t <= 0");

	//initialize
	SAFEHEAD(bigfrac_t)
	mathbankaccess_t<mpq_t,S,bigfrac_t<S>> b;

	//
	// routines
	//

		//callbacks for the mathbank
		inline static void _cbinit(mpq_t *v, _UNUSED_ int size) 	 	{ mpq_init(v[0]); }
		inline static void _cbdeinit(mpq_t *v, _UNUSED_ int size)  		{ mpq_clear(v[0]); }
		inline static void _cbrealloc(mpq_t *v, _UNUSED_ int size) 		{ mpq_clear(v[0]); mpq_init(v[0]); }

	//user routines
	#define make(e,v) e=mathbankaccess_t<mpq_t,S,bigfrac_t<S>>::bank_t::allocnode(); v=e->m_v;
	#define kill(e)   if(e) { mathbankaccess_t<mpq_t,S,bigfrac_t<S>>::bank_t::freenode(e); }

		inline void _init() 											{ SAFE() make(b.m_e,b.m_v)   make(b.m_etmp,b.m_vtmp) }
	inline ~bigfrac_t() 												{ SAFE() kill(b.m_e)         kill(b.m_etmp)          }

	#undef make
	#undef kill

	inline bigfrac_t()  												{ _init(); }						//cppcheck-suppress noExplicitConstructor
	inline bigfrac_t( int val )  										{ _init(); this->operator=(val); }	//cppcheck-suppress noExplicitConstructor
	inline bigfrac_t( const double &val )  								{ _init(); this->operator=(val); }	//cppcheck-suppress noExplicitConstructor
	inline bigfrac_t( const mpz_t *rhs )  								{ _init(); this->operator=(rhs); }	//cppcheck-suppress noExplicitConstructor
	inline bigfrac_t( const mpq_t *rhs )  								{ _init(); this->operator=(rhs); }	//cppcheck-suppress noExplicitConstructor
	inline bigfrac_t( const bigfrac_t<S> &rhs )  						{ _init(); this->operator=(rhs); }	//cppcheck-suppress noExplicitConstructor

	inline 		 int operator=( int val ) 								{ SAFE() mpq_set_si(b.m_v[0],val,1); 		return(val); }
	inline const double& operator=( const double &val ) 				{ SAFE() mpq_set_d(b.m_v[0],val);    		return(val); }
	inline const mpz_t* operator=( const mpz_t *rhs ) 					{ SAFE() mpq_set_z(b.m_v[0],rhs[0]); 		return(rhs); }
	inline  	 mpq_t* operator=( const mpq_t *rhs ) 					{ SAFE() mpq_set(b.m_v[0],rhs[0]);   		return(b.m_v); }
	inline       bigfrac_t<S>& operator=( const bigfrac_t<S> &rhs )		{ SAFE() mpq_set(b.m_v[0],rhs.b.m_v[0]); 	return(*this); }	//overload to avoid structure copy errors

		inline void 			_neg()		 							{ SAFE() mpq_neg(b.m_v[0],b.m_v[0]); }
		inline void 			_abs()		 							{ SAFE() mpq_abs(b.m_v[0],b.m_v[0]); }
		inline bigfrac_t<S>& 	_inverse()		 						{ SAFE() mpz_swap( mpq_numref(b.m_v[0]), mpq_denref(b.m_v[0]) ); return(this[0]); }

		inline void _add( const mpz_t *rhs ) 							{ SAFE() bigfrac_t _rhs(rhs); this->_add(_rhs.b.m_v); }
		inline void _sub( const mpz_t *rhs ) 							{ SAFE() bigfrac_t _rhs(rhs); this->_sub(_rhs.b.m_v); }
		inline void _mul( const mpz_t *rhs ) 							{ SAFE() bigfrac_t _rhs(rhs); this->_mul(_rhs.b.m_v); }
		inline void _div( const mpz_t *rhs ) 							{ SAFE() bigfrac_t _rhs(rhs); this->_div(_rhs.b.m_v); }
		inline bool _eq( const mpz_t *rhs ) 					const 	{ SAFE() bigfrac_t _rhs(rhs); return(this->_eq(_rhs.b.m_v));  }
		inline bool _gt( const mpz_t *rhs ) 					const 	{ SAFE() bigfrac_t _rhs(rhs); return(this->_gt(_rhs.b.m_v));  }
		inline bool _gte( const mpz_t *rhs ) 					const 	{ SAFE() bigfrac_t _rhs(rhs); return(this->_gte(_rhs.b.m_v)); }
		inline bool _lt( const mpz_t *rhs ) 					const 	{ SAFE() bigfrac_t _rhs(rhs); return(this->_lt(_rhs.b.m_v));  }
		inline bool _lte( const mpz_t *rhs ) 					const 	{ SAFE() bigfrac_t _rhs(rhs); return(this->_lte(_rhs.b.m_v)); }

		inline void _add( const mpq_t *rhs ) 							{ SAFE() mpq_add(b.m_vtmp[0], b.m_v[0], rhs[0]); b.swap(); }
		inline void _sub( const mpq_t *rhs ) 							{ SAFE() mpq_sub(b.m_vtmp[0], b.m_v[0], rhs[0]); b.swap(); }
		inline void _mul( const mpq_t *rhs ) 							{ SAFE() mpq_mul(b.m_vtmp[0], b.m_v[0], rhs[0]); b.swap(); }
		inline void _div( const mpq_t *rhs ) 							{ SAFE() mpq_div(b.m_vtmp[0], b.m_v[0], rhs[0]); b.swap(); }
		inline bool _eq( const mpq_t *rhs ) 					const 	{ SAFE() return(mpq_cmp( b.m_v[0], rhs[0] ) == 0);  }
		inline bool _gt( const mpq_t *rhs ) 					const 	{ SAFE() return(mpq_cmp( b.m_v[0], rhs[0] ) > 0);  }
		inline bool _gte( const mpq_t *rhs ) 					const 	{ SAFE() return(mpq_cmp( b.m_v[0], rhs[0] ) >= 0); }
		inline bool _lt( const mpq_t *rhs ) 					const 	{ SAFE() return(mpq_cmp( b.m_v[0], rhs[0] ) < 0);  }
		inline bool _lte( const mpq_t *rhs ) 					const 	{ SAFE() return(mpq_cmp( b.m_v[0], rhs[0] ) <= 0); }

		inline bool _eq( const double &rhs ) 					const 	{ SAFE() return(mpq_get_d( b.m_v[0] ) == rhs);  }
		inline bool _gt( const double &rhs ) 					const 	{ SAFE() return(mpq_get_d( b.m_v[0] ) > rhs);  }
		inline bool _gte( const double &rhs ) 					const 	{ SAFE() return(mpq_get_d( b.m_v[0] ) >= rhs); }
		inline bool _lt( const double &rhs ) 					const 	{ SAFE() return(mpq_get_d( b.m_v[0] ) < rhs);  }
		inline bool _lte( const double &rhs ) 					const 	{ SAFE() return(mpq_get_d( b.m_v[0] ) <= rhs); }

	inline void operator+=( const mpz_t *rhs ) 							{ _add(rhs); }
	inline void operator-=( const mpz_t *rhs ) 							{ _sub(rhs); }
	inline void operator*=( const mpz_t *rhs ) 							{ _mul(rhs); }
	inline void operator/=( const mpz_t *rhs ) 							{ _div(rhs); }

	inline void operator+=( const mpq_t *rhs ) 							{ _add(rhs); }
	inline void operator-=( const mpq_t *rhs ) 							{ _sub(rhs); }
	inline void operator*=( const mpq_t *rhs ) 							{ _mul(rhs); }
	inline void operator/=( const mpq_t *rhs ) 							{ _div(rhs); }

	inline void operator+=( const double &rhs ) 						{ _add(bigfrac_t<S>(rhs)); }
	inline void operator-=( const double &rhs ) 						{ _sub(bigfrac_t<S>(rhs)); }
	inline void operator*=( const double &rhs ) 						{ _mul(bigfrac_t<S>(rhs)); }
	inline void operator/=( const double &rhs ) 						{ _div(bigfrac_t<S>(rhs)); }

	inline 		 			 		mpq_t* raw()				const	{ SAFE() return(b.m_v); 							}
	inline		 			 const 	char* str()					const	{ SAFE() return((const char*)this[0]); 				}
	inline		 	operator const 	mpq_t* ()					const	{ SAFE() return(b.m_v); 							}
	inline explicit operator 		double () 					const 	{ SAFE() return(mpq_get_d(b.m_v[0]));  				}
	inline explicit operator 		unsigned int () 			const 	{ SAFE() return((unsigned int)mpq_get_d(b.m_v[0])); }
	inline explicit operator 		int () 						const 	{ SAFE() return((int)mpq_get_d(b.m_v[0])); 			}
	//operator const mpq_t* () <- prevents ambiguity

	operator const char*() const {
		SAFE()
		char *r = b.m_e->getstringmem();
		gmp_snprintf( r, BIGMATHSTRBUFFERMAX, "%Qd", b.m_v[0] ); 
		return r;
	}
	
	//special routines

	inline bigfrac_t<S> abs() 									const	{ SAFE() bigfrac_t<S> r; mpq_abs(r.b.m_v[0],b.m_v[0]); return(r); }
	inline bigfrac_t<S> inverse()		 						const 	{ SAFE() bigfrac_t<S> r(this[0]); return(r._inverse()); }

	bigint_t<S> round() const { 
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
#define _S _bigmath_compile::modcastszup(S)

//big modular number
template <int S>
struct bigmod_t : biguint_t<_S> {

	//constant calculation and typedefs
	static const unsigned int pow2bits = _bigmath_compile::modpow2calc(S);
	typedef typename mathbankaccess_t<mpz_t,_S,biguint_t<_S>>::bankentry_t bankentry_t;

	//constants
	const int FLG_CLEAN  = 0x01;

	//initialize
	//cppcheck-suppress duplInheritedMember
	SAFEHEAD(bigmod_t)

	static thread_local bankentry_t	*g_defmodptr;
	bankentry_t 					*m_modptr;
	int 		 					m_modflags;

	//
	// routines
	//

		inline const biguint_t<_S>* _upcast_const() const 	{ return static_cast<const biguint_t<_S>*>(this); }
		inline 		 biguint_t<_S>* _upcast() 				{ return static_cast<biguint_t<_S>*>(this); }

		// >>> smart pointer management for modulus

			inline void _makenode( bankentry_t **ee ) const { ee[0]=mathbankaccess_t<mpz_t,_S,biguint_t<_S>>::bank_t::allocnode(); }

			inline void _killnode( bankentry_t *ee )  const {
				if(ee==g_defmodptr) g_defmodptr=0;
				if(pow2bits>0) 		ee->m_maske->m_bank->freenode(ee->m_maske);
				mathbankaccess_t<mpz_t,_S,biguint_t<_S>>::bank_t::freenode(ee);
			}

			inline bankentry_t* _initpow2mod( bankentry_t *r ) {
				mpz_t &v=r->m_v[0], &vtmp=this->b.m_vtmp[0];
				mpz_set_ui( v, 1 );
				mpz_mul_2exp( vtmp, v, pow2bits );
				this->b.swaptmp( &r );
				_makenode( &r->m_maske );
				mpz_sub_ui( r->m_maske->m_v[0], vtmp, 1 );
				return r;
			} //r=(2^pow2bits)-1

			inline bankentry_t* _refmod( bankentry_t *ptr ) 					const 	{ ptr->m_refcnt++; return(ptr); }
			inline void 		_derefmod( bankentry_t *ptr ) 					const	{ if(--ptr->m_refcnt<=0) _killnode(ptr); }
			inline void 		_changemod( bankentry_t **a, bankentry_t *b )	const	{ b->m_refcnt++; _derefmod(a[0]); a[0]=b; }

			inline bankentry_t* _genmod( const mpz_t *d ) {
				bankentry_t *r; _makenode(&r);
				if(pow2bits>0) 	r = _initpow2mod(r);
				else 			mpz_set( r->m_v[0], d );
				r->m_refcnt=1;
				return(r);
			}

			inline bankentry_t* _genmod( int d ) {
				bankentry_t *r; _makenode(&r);
				if(pow2bits>0) 	r = _initpow2mod(r);
				else 			mpz_set_si( r->m_v[0], d );
				r->m_refcnt=1;
				return(r);
			}

			inline bankentry_t* _getdefmod() {
				if(g_defmodptr==0) { return(g_defmodptr=_genmod(1)); }
				return(_refmod(g_defmodptr));
			}

		// >>> maintain number in modulus

			inline void _copycleanraw( mpz_t *rhs ) const {
				if((m_modflags&FLG_CLEAN)==0) {
					if(pow2bits>0)	mpz_and( rhs[0], this->b.m_v[0], m_modptr->m_maske->m_v[0] );
					else			mpz_mod( rhs[0], this->b.m_v[0], m_modptr->m_v[0] );
					return;
				}
				mpz_set( rhs[0], this->b.m_v[0] );
			}

			inline void _doclean() {
				if(pow2bits>0) 	_upcast()[0]&=m_modptr->m_maske->m_v;	//and mask for pow2 fields
				else 			_upcast()[0]%=m_modptr->m_v;			//actual modulus
				_markclean();
			}

			inline void _clean() 		{ if((m_modflags&FLG_CLEAN)==0) _doclean(); }
			inline void _markclean() 	{ m_modflags|=FLG_CLEAN; }
			inline void _dirty() 		{ m_modflags&=(~FLG_CLEAN); }

	inline bigmod_t( 									   			) : biguint_t<_S>(),	     					m_modptr(_getdefmod()),  		  m_modflags(0)	  	 	 	 {}	//cppcheck-suppress noExplicitConstructor
	inline bigmod_t( 						 int d 		   			) : biguint_t<_S>(),      						m_modptr(_genmod(d)), 			  m_modflags(0)				 {}	//cppcheck-suppress noExplicitConstructor
	inline bigmod_t( 						 const mpz_t *d 		) : biguint_t<_S>(),      						m_modptr(_genmod(d)), 			  m_modflags(0)				 {}	//cppcheck-suppress noExplicitConstructor
	inline bigmod_t( int rhs, 				 const mpz_t *d 		) : biguint_t<_S>( rhs ), 						m_modptr(_genmod(d)), 			  m_modflags(0) 			 {}	//cppcheck-suppress noExplicitConstructor
	inline bigmod_t( int rhs, 				 int d 		   			) : biguint_t<_S>( rhs ), 						m_modptr(_genmod(d)), 			  m_modflags(0) 			 {}	//cppcheck-suppress noExplicitConstructor
	inline bigmod_t( const bigmod_t &rhs				  			) : biguint_t<_S>( rhs._upcast_const()[0] ),	m_modptr(_refmod(rhs.m_modptr)),  m_modflags(rhs.m_modflags) {} //cppcheck-suppress noExplicitConstructor
	inline bigmod_t( int rhs,				 const bankentry_t &d	) : biguint_t<_S>( rhs ), 						m_modptr(_refmod(&d)), 	 	  	  m_modflags(0) 			 {} //cppcheck-suppress noExplicitConstructor
	inline bigmod_t( const mpz_t *rhs, 		 const bankentry_t &d 	) : biguint_t<_S>( rhs ), 						m_modptr(_refmod(&d)),		 	  m_modflags(0) 			 {}	//cppcheck-suppress noExplicitConstructor
	inline bigmod_t( const mpz_t *rhs, 		 const mpz_t *d 		) : biguint_t<_S>( rhs ), 						m_modptr(_genmod(d)), 			  m_modflags(0) 			 {} //cppcheck-suppress noExplicitConstructor
	inline bigmod_t( const bankentry_t &rhs, const bankentry_t &d 	) : biguint_t<_S>( rhs.m_v ), 					m_modptr(_refmod(&d)),		  	  m_modflags(0) 			 {}	

	inline ~bigmod_t() 																		{ SAFE() _derefmod(m_modptr); }
	//the above is deliberately *not* virtual for performance

	inline 			int 					operator=( int rhs )							{ SAFE() _dirty(); 				 				    				    _upcast()->operator=(rhs); return(rhs);   		}
	inline   	  mpz_t* 					operator=( const mpz_t *rhs )					{ SAFE() _dirty(); 				 				    				    _upcast()->operator=(rhs); return(this->b.m_v); }
	inline        bigmod_t<S>& 				operator=( const bigmod_t<S> &rhs )				{ SAFE() _changemod(&m_modptr,rhs.m_modptr); m_modflags=rhs.m_modflags; _upcast()->operator=(rhs); return(*this); 		} //overload to avoid structure copy errors

	inline void neg()  																		{ SAFE() _upcast()->_neg(); _dirty(); }													//dirty

	inline void operator+=(  const mpz_t *rhs )  											{ SAFE() _upcast()->_add(rhs); _dirty(); }												//dirty
	inline void operator-=(  const mpz_t *rhs )  											{ SAFE() _upcast()->_sub(rhs); _dirty(); }												//dirty
	inline void operator*=(  const mpz_t *rhs )  											{ SAFE() _upcast()->_mul(rhs); _doclean(); } 											//reduce now
	inline void operator/=(  const mpz_t *rhs ) 	 										{ SAFE() bigmod_t<S> _rhs(rhs); this[0]*=_rhs._inverse(); }								//multiply reduces
	inline void operator%=(  const mpz_t *rhs )  											{ SAFE() _clean(); _upcast()->_mod(rhs); }												//clean - cannot expand number
	inline void operator<<=( const mpz_t *rhs )  											{ SAFE() bigmod_t<S> _rhs(2,m_modptr); this[0]*=_rhs._pow(rhs); }						//multiply reduces
	inline void operator>>=( const mpz_t *rhs )  											{ SAFE() bigmod_t<S> _rhs(2,m_modptr); this[0]*=_rhs._inverse()._pow(rhs); }			//multiply reduces
	inline void operator&=(  const mpz_t *rhs ) 											{ SAFE() _clean(); _upcast()->_and(rhs); }												//clean - cannot expand number
	inline void operator|=(  const mpz_t *rhs ) 											{ SAFE() _clean(); _upcast()->_or(rhs);  _dirty(); }									//dirty
	inline void operator^=(  const mpz_t *rhs ) 											{ SAFE() _clean(); _upcast()->_xor(rhs); _dirty(); }									//dirty

	inline void operator+=( const int rhs ) 												{ SAFE() _upcast()->_add(rhs); _dirty(); }												//dirty
	inline void operator-=( const int rhs ) 												{ SAFE() _upcast()->_sub(rhs); _dirty(); }												//dirty
	inline void operator*=( const int rhs ) 												{ SAFE() _upcast()->_mul(rhs); _doclean(); }											//reduce now
	inline void operator/=( const int rhs ) 	 											{ SAFE() bigmod_t<S> _rhs(rhs,m_modptr); this[0]*=_rhs._inverse(); }					//multiply reduces
	inline void operator%=( const int rhs ) 												{ SAFE() _clean(); _upcast()->_mod(rhs); }												//clean - cannot expand number
	inline void operator<<=( const int rhs ) 												{ SAFE() bigmod_t<S> _rhs(2,m_modptr); this[0]*=_rhs._pow(rhs); }						//multiply reduces
	inline void operator>>=( const int rhs ) 												{ SAFE() bigmod_t<S> _rhs(2,m_modptr); this[0]*=_rhs._inverse()._pow(rhs); }			//multiply reduces
	inline void operator&=( const int rhs ) 												{ SAFE() _clean(); _upcast()->_and(rhs); }												//clean - cannot expand number
	inline void operator|=( const int rhs ) 												{ SAFE() _clean(); _upcast()->_or(rhs);  _dirty(); }									//dirty
	inline void operator^=( const int rhs ) 												{ SAFE() _clean(); _upcast()->_xor(rhs); _dirty(); }									//dirty

	inline 		 			 		biguint_t<_S>& base()									{ SAFE() _clean(); return _upcast()[0]; 				}
	inline 		 			 	 	mpz_t* raw()											{ SAFE() _clean(); return (this->b.m_v); 				}
	inline		 			 const 	char* str()												{ SAFE() _clean(); return((const char*)this[0]); 		}
	inline 		 	operator const 	mpz_t*()												{ SAFE() _clean(); return (this->b.m_v); 				}
	inline 		 	operator const 	mpz_t*()										const	{ SAFE() _modularcastsafety_(this[0]); return(0);	}
	inline explicit operator 		biguint_t<_S>*()										{ SAFE() _clean(); return _upcast(); 					}
	inline explicit operator 		unsigned int() 		 									{ SAFE() _clean(); return (unsigned int)(_upcast()[0]); }
	inline explicit operator 		int() 				 									{ SAFE() _clean(); return (int)(_upcast()[0]); 			}
	inline explicit operator const 	char*()													{ SAFE() _clean(); return (const char*)(_upcast()[0]); 	}
	//operator const return of mpz_t is to prevent resolution ambiguity introduced by non-const

	//
	// additional modular specific routines
	//

		inline bigmod_t<S>& _inverse() 														{ SAFE() mpz_invert(  this->b.m_vtmp[0], this->b.m_v[0], 		  m_modptr->m_v[0] ); this->b.swap(); _markclean(); return(*this); }
		inline bigmod_t<S>& _pow( const mpz_t *rhs )										{ SAFE() mpz_powm(    this->b.m_vtmp[0], this->b.m_v[0], rhs[0], m_modptr->m_v[0] ); this->b.swap(); _markclean(); return(*this); }
		inline bigmod_t<S>& _pow( int rhs )													{ SAFE() mpz_powm_ui( this->b.m_vtmp[0], this->b.m_v[0], rhs, 	  m_modptr->m_v[0] ); this->b.swap(); _markclean(); return(*this); }

	inline bigmod_t<S> inverse() 													const	{ SAFE() bigmod_t<S> _rhs(this[0]); return(_rhs._inverse()); }
	inline bigmod_t<S> pow( const mpz_t *rhs )										const	{ SAFE() bigmod_t<S> _rhs(this[0]); return(_rhs._pow(rhs));  }
	inline bigmod_t<S> pow( int rhs )												const	{ SAFE() bigmod_t<S> _rhs(this[0]); return(_rhs._pow(rhs));  }

	inline 			void 			changemod( const mpz_t *rhs ) 							{ SAFE() _changemod(&m_modptr,_genmod(rhs)); _dirty(); }
	inline 			void 			changemod( bankentry_t &rhs ) 							{ SAFE() _changemod(&m_modptr,&rhs); _dirty(); }
	inline 		 	void 			copyraw( mpz_t *target )						const 	{ SAFE() _copycleanraw(target); }
	inline 			bankentry_t&	getmodentry() 									const 	{ SAFE() return(m_modptr[0]); }
	inline 			mpz_t*			getmod() 										const 	{ SAFE() return(m_modptr->raw()); }

	//chinese remainder optimized for big numbers - avoids big multiplies and big modulus reductions
	//	this form may favor r/wordsize > sz
	static biguint_t<S> crt_scratch( bigmod_t *v, int sz, bigmod_t *s1, bigmod_t *s2 ) {
		int x, y;
		bigmod_t delta;
		biguint_t<S> r(v[0]), scale(1);
		for(x=1;x<sz;x++) {
			s1[x] = bigmod_t( v[0],			 	  v[x].getmodentry() );		//set all moduli to value in first mod
			s2[x] = bigmod_t( v[0].getmodentry(), v[x].getmodentry() );		//set all multipliers for all moduli to first mod
		}
		for(x=1;x<sz;x++) {
			delta=(v[x]-s1[x])/s2[x];	//calculate steps to align to answer in field mod[x]
			for(y=x+1;y<sz;y++) {
				s1[y]+=s2[y]*delta;		//update answers in remaining mods - s2 on lefthand to prefer s2 modulus
				s2[y]*=v[x].getmod();	//update multipliers in remaining mods
			}
			scale*=v[x-1].getmod();		//update global multiplier, lags behind to prevent unnecessary tail multiply
			r+=scale*delta;				//update the return answer we are calculating - scale on lefthand for biguint result
		}
		return r;
	}
};

template <int S>
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

template <int S>
struct bigstream_t : bigmod_t<-S> {

	static_assert(S>0,"error: bigstream_t <= 0");

	//initialize
	//cppcheck-suppress duplInheritedMember
	SAFEHEAD(bigstream_t)
	
	//
	// routines
	//
	
	inline bigstream_t() : bigmod_t<-S>() {}
	
	
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

#endif
