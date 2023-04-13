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

#ifndef BIGMATH_H
#define BIGMATH_H

#include <new>
#include <gmp.h>
#include "linkedlist.h"
#include "memsafety.h"

//
// math memory management template
//

#ifndef BIGMATHBANKSIZE
#define BIGMATHBANKSIZE 100
#endif

#ifndef BIGMATHSTRBUFFERMAX
#define BIGMATHSTRBUFFERMAX 256
#endif

#ifndef BIGMATHSTRQUEUEMAX
#define BIGMATHSTRQUEUEMAX 32
#endif

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

	struct bankentry_t {

		static thread_local char	g_strbuffer[BIGMATHSTRBUFFERMAX*BIGMATHSTRQUEUEMAX];	//thread specific to avoid mutex locks
		static thread_local int		g_strbufferpos;											//thread specific to avoid mutex locks

		linkitem<bankentry_t>		m_item;
		bank_t 						*m_bank;
		T	 						*m_v;
		bankentry_t					*m_maske;
		int							m_refcnt;

		inline char *getstringmem() {
			char *handle;
			if(g_strbufferpos>BIGMATHSTRBUFFERMAX*(BIGMATHSTRQUEUEMAX-1)) g_strbufferpos=0;
			handle = &g_strbuffer[g_strbufferpos];
			g_strbufferpos+=BIGMATHSTRBUFFERMAX;
			return(handle);
		}

	};

	struct bank_t {

		SAFEHEAD(bank_t)

		static const int BANKSIZE = BIGMATHBANKSIZE;

		const unsigned int C_ALLOC_LIMBS = (S/mp_bits_per_limb+(S%mp_bits_per_limb==0?0:1)+1);
		const unsigned int C_ALLOC_BITS  = C_ALLOC_LIMBS*mp_bits_per_limb;

		static thread_local linkbase<bank_t> g_base, g_freebase;	//thread specific to avoid mutex locks
							linkitem<bank_t> m_item, m_freeitem;
							linkbase<bankentry_t> m_freenodebase;

		T	 			m_v[BANKSIZE];
		bankentry_t 	m_nodes[BANKSIZE];
		int 			m_usedcount, m_freecount;

		inline bool isbankfree() { SAFE(); return(m_usedcount<BANKSIZE || m_freenodebase.first()); }

		bank_t() {
			SAFE()
			m_item.link(this);
			m_freeitem.link(this);
			g_base.add(&m_item);
			g_freebase.add(&m_freeitem);
			m_usedcount=m_freecount=0;
			for(int x=0;x<BANKSIZE;x++) CBT::_cbinit(&m_v[x],C_ALLOC_BITS);
		}

		~bank_t() {
			SAFE()
			g_base.remove(&m_item);
			if(isbankfree()) g_freebase.remove(&m_freeitem);
			for(int x=0;x<BANKSIZE;x++) CBT::_cbdeinit(&m_v[x],C_ALLOC_BITS);
		}

		static bankentry_t *allocnode() {

			bankentry_t 	*node;
			bank_t 			*bank = g_freebase.first();

			if(bank==0) {
				if(!(bank=new bank_t)) { throw std::bad_alloc(); }
			}

			if((node = bank->m_freenodebase.first())==0) {
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

		void freenode( bankentry_t *e ) {
			SAFE()
			e->m_item.link(e);										//linked list initialization of node
			m_freenodebase.add(&e->m_item);							//add node to bank's freenodebase
			if(isbankfree()==false) g_freebase.add(&m_freeitem);	//add bank to thread's list of banks with free elements
			CBT::_cbrealloc(&e->m_v[0],C_ALLOC_BITS);				//reset gmp memory of this entry - prevents memory creep
			if(++m_freecount>=m_usedcount) delete this;				//free this bank if all items free
		}

	};

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
thread_local char mathbankaccess_t<T,S,CBT>::bankentry_t::g_strbuffer[BIGMATHSTRBUFFERMAX*BIGMATHSTRQUEUEMAX];

template <typename T, int S, typename CBT>
thread_local int mathbankaccess_t<T,S,CBT>::bankentry_t::g_strbufferpos = 0;

template <typename T, int S, typename CBT>
thread_local linkbase<typename mathbankaccess_t<T,S,CBT>::bank_t> mathbankaccess_t<T,S,CBT>::bank_t::g_base;

template <typename T, int S, typename CBT>
thread_local linkbase<typename mathbankaccess_t<T,S,CBT>::bank_t> mathbankaccess_t<T,S,CBT>::bank_t::g_freebase;


//
// uint code
//

template <int S>
struct bigmodbase_t;

template <int S>
struct biguint_t {

	static_assert(S>0,"error: biguint_t <= 0");

	SAFEHEAD(biguint_t)
	mathbankaccess_t<mpz_t,S,biguint_t<S>> b;

	//
	// routines
	//

		//callbacks for the mathbank
		inline static void _cbinit(mpz_t *v, int size) 	 				{ mpz_init2(v[0],size); 	}
		inline static void _cbdeinit(mpz_t *v, int size) 				{ mpz_clear(v[0]); 		 	}
		inline static void _cbrealloc(mpz_t *v, int size) 				{ mpz_realloc2(v[0],size); 	}

	//user routines
	#define make(e,v) { e=mathbankaccess_t<mpz_t,S,biguint_t<S>>::bank_t::allocnode(); v=e->m_v; }
	#define kill(e)   if(e) { e->m_bank->freenode(e); }

		inline void _init() 												{ SAFE() make(b.m_e,b.m_v)   make(b.m_etmp,b.m_vtmp) }
	inline ~biguint_t() 													{ SAFE() kill(b.m_e)         kill(b.m_etmp)          }
	//the above is deliberately *not* virtual for performance

	#undef make
	#undef kill

	inline biguint_t()  													{ _init(); }						//cppcheck-suppress noExplicitConstructor
	inline biguint_t( int val )  											{ _init(); this->operator=(val); }	//cppcheck-suppress noExplicitConstructor
	inline biguint_t( const mpz_t *rhs ) 		 							{ _init(); this->operator=(rhs); }	//cppcheck-suppress noExplicitConstructor
	inline biguint_t( const biguint_t &rhs ) 								{ _init(); this->operator=(rhs); }	//cppcheck-suppress noExplicitConstructor
	inline biguint_t( const bigmodbase_t<S> &rhs )							{ _init(); this->operator=(rhs); }

	inline 		 int 				operator=( int val ) 					{ SAFE() mpz_set_ui(b.m_v[0],(unsigned long int)val); return(val); }
	inline const mpz_t* 			operator=( const mpz_t *rhs )			{ SAFE() mpz_set(b.m_v[0],rhs[0]);  return(rhs); }
	inline 	 	 biguint_t<S> & 	operator=( const biguint_t<S> &rhs )	{ SAFE() mpz_set(b.m_v[0],rhs.b.m_v[0]); return(*this); }				//overload to avoid structure copy errors
	inline const bigmodbase_t<S> & 	operator=( const bigmodbase_t<S> &rhs ) { SAFE() rhs.copyraw(b.m_v); return(rhs); }								//overload to avoid structure copy errors

		inline void _neg()		 											{ SAFE() mpz_neg(b.m_v[0],b.m_v[0]); }
		inline void _abs()		 											{ SAFE() mpz_abs(b.m_v[0],b.m_v[0]); }

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
	inline 			operator const mpz_t*()							const	{ SAFE() return(b.m_v); 					}
	inline explicit operator 	   double() 						const 	{ SAFE() return(mpz_get_d(b.m_v[0]));  		}
	inline explicit operator 	   unsigned int() 					const 	{ SAFE() return(mpz_get_ui(b.m_v[0]));   	}
	inline explicit operator 	   int() 							const 	{ SAFE() return((int)mpz_get_ui(b.m_v[0])); }

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

		inline biguint_t<S>* const _upcast() { return static_cast<biguint_t<S>*>(this); }

	inline bigint_t() : biguint_t<S>()									{}							//cppcheck-suppress noExplicitConstructor
	inline bigint_t( int val ) : biguint_t<S>() 		 				{ this->operator=(val); }	//cppcheck-suppress noExplicitConstructor
	inline bigint_t( const mpz_t *rhs ) : biguint_t<S>( rhs ) 			{}							//cppcheck-suppress noExplicitConstructor
	inline bigint_t( const bigint_t  &rhs ) : biguint_t<S>( rhs ) 		{}

	inline 		 int 			operator=( int val ) 					{ SAFE() mpz_set_si( this->b.m_v[0], val ); return(val); }
	inline const mpz_t* 		operator=( const mpz_t *rhs )			{ _upcast()->operator=(rhs); return(rhs); }
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
		inline static void _cbinit(mpq_t *v, int size) 	 				{ mpq_init(v[0]); }
		inline static void _cbdeinit(mpq_t *v, int size)  				{ mpq_clear(v[0]); }
		inline static void _cbrealloc(mpq_t *v, int size) 				{ mpq_clear(v[0]); mpq_init(v[0]); }

	//user routines
	#define make(e,v) e=mathbankaccess_t<mpq_t,S,bigfrac_t<S>>::bank_t::allocnode(); v=e->m_v;
	#define kill(e)   if(e) { e->m_bank->freenode(e); }

		inline void _init() 											{ SAFE() make(b.m_e,b.m_v)   make(b.m_etmp,b.m_vtmp) }
	inline ~bigfrac_t() 												{ SAFE() kill(b.m_e)         kill(b.m_etmp)          }

	#undef make
	#undef kill

	inline bigfrac_t()  												{ _init(); }						//cppcheck-suppress noExplicitConstructor
	inline bigfrac_t( int val )  										{ _init(); this->operator=(val); }	//cppcheck-suppress noExplicitConstructor
	inline bigfrac_t( const double &val )  								{ _init(); this->operator=(val); }	//cppcheck-suppress noExplicitConstructor
	inline bigfrac_t( const mpz_t *rhs )  								{ _init(); this->operator=(rhs); }	//cppcheck-suppress noExplicitConstructor
	inline bigfrac_t( const mpq_t *rhs )  								{ _init(); this->operator=(rhs); }	//cppcheck-suppress noExplicitConstructor
	inline bigfrac_t( const bigfrac_t<S> &rhs )  						{ _init(); this->operator=(rhs); }

	inline 		 int operator=( int val ) 								{ SAFE() mpq_set_si(b.m_v[0],val,1); 		return(val); }
	inline const double& operator=( const double &val ) 				{ SAFE() mpq_set_d(b.m_v[0],val);    		return(val); }
	inline const mpz_t* operator=( const mpz_t *rhs ) 					{ SAFE() mpq_set_z(b.m_v[0],rhs[0]); 		return(rhs); }
	inline const mpq_t* operator=( const mpq_t *rhs ) 					{ SAFE() mpq_set(b.m_v[0],rhs[0]);   		return(rhs); }
	inline       bigfrac_t<S>& operator=( const bigfrac_t<S> &rhs )		{ SAFE() mpq_set(b.m_v[0],rhs.b.m_v[0]); 	return(*this); }	//overload to avoid structure copy errors

		inline void _neg()		 										{ SAFE() mpq_neg(b.m_v[0],b.m_v[0]); }
		inline void _abs()		 										{ SAFE() mpq_abs(b.m_v[0],b.m_v[0]); }

		//TODO: add mpz_t routines

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

	inline void operator+=( const mpq_t *rhs ) 							{ _add(rhs); }
	inline void operator-=( const mpq_t *rhs ) 							{ _sub(rhs); }
	inline void operator*=( const mpq_t *rhs ) 							{ _mul(rhs); }
	inline void operator/=( const mpq_t *rhs ) 							{ _div(rhs); }

	inline void operator+=( const double &rhs ) 						{ _add(bigfrac_t<S>(rhs)); }
	inline void operator-=( const double &rhs ) 						{ _sub(bigfrac_t<S>(rhs)); }
	inline void operator*=( const double &rhs ) 						{ _mul(bigfrac_t<S>(rhs)); }
	inline void operator/=( const double &rhs ) 						{ _div(bigfrac_t<S>(rhs)); }

	inline 		 			 		mpq_t* raw()				const	{ SAFE() return(b.m_v); 							}
	inline		 	operator const 	mpq_t* ()					const	{ SAFE() return(b.m_v); 							}
	inline explicit operator 		double () 					const 	{ SAFE() return(mpq_get_d(b.m_v[0]));  				}
	inline explicit operator 		unsigned int () 			const 	{ SAFE() return((unsigned int)mpq_get_d(b.m_v[0])); }
	inline explicit operator 		int () 						const 	{ SAFE() return((int)mpq_get_d(b.m_v[0])); 			}

	operator const char*() const {
		SAFE()
		char *r = b.m_e->getstringmem();
		gmp_snprintf( r, BIGMATHSTRBUFFERMAX, "%Qd", b.m_v[0] ); 
		return r;
	}
	
	//special routines

	inline bigfrac_t<S> abs() 									const	{ SAFE() bigfrac_t<S> r; mpq_abs(r.b.m_v[0],b.m_v[0]); return(r); }

	bigint_t<S> round() const { 
		SAFE();
		bigint_t<S> r;
		__mpz_struct *numref = mpq_numref(b.m_v[0]);
		__mpz_struct *denref = mpq_denref(b.m_v[0]);
		mpz_mul_2exp( r.b.m_vtmp[0], numref, 1 );		 				  	//double numerator
		mpz_tdiv_q( r.b.m_v[0], r.b.m_vtmp[0], denref ); 				  	//divide - truncating toward zero
		r += ( 2 * mpz_tstbit( r.b.m_v[0], 0 ) * mpz_sgn( r.b.m_v[0] ) );   //rounding logic
		r /= 2;
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

//extra structure to allow for differential in template parameter counts *and* 
//	to allow non-virtual function performance in base class - most of this is 
//	here to avoid violating const-ness rules on edge case assignments in the 
//	base structure without the aforementioned performance drawback of a full 
//	virtual base
template <int S>
struct bigmodbase_t : biguint_t<S> {

	//avoid the need for a virtual base class to support edge cases of finalizing modular numbers on conversion
	typedef void (*f_copyraw_tp)(const void*,mpz_t*);
	f_copyraw_tp f_copyraw;

	//cppcheck-suppress noExplicitConstructor
	inline bigmodbase_t( 						  f_copyraw_tp cb ) : biguint_t<S>(), 		f_copyraw(cb) {}	//cppcheck-suppress noExplicitConstructor
	inline bigmodbase_t( int val, 				  f_copyraw_tp cb ) : biguint_t<S>( val ), 	f_copyraw(cb) {}	//cppcheck-suppress noExplicitConstructor
	inline bigmodbase_t( const mpz_t *rhs, 		  f_copyraw_tp cb ) : biguint_t<S>( rhs ), 	f_copyraw(cb) {}	//cppcheck-suppress noExplicitConstructor
	inline bigmodbase_t( const biguint_t<S> &rhs, f_copyraw_tp cb ) : biguint_t<S>( rhs ), 	f_copyraw(cb) {}

	//routines
	inline void copyraw( mpz_t *rhs ) const { f_copyraw(this,rhs); }

};

//big modular number
template <int S, int pow2bits>
struct bigmod_t : bigmodbase_t<S> {

	typedef typename mathbankaccess_t<mpz_t,S,biguint_t<S>>::bankentry_t bankentry_t;

	//limb alignment
	static_assert(pow2bits>=0,"error: pow2bits < 0");
	static_assert(S>0,"error: S <= 0");
	static_assert(S>pow2bits,"error: S <= pow2bits");

	//constants
	const int FLG_CLEAN  = 0x01;

	//initialize
	//cppcheck-suppress duplInheritedMember
	SAFEHEAD(bigmod_t)

	bankentry_t *m_modptr;
	int 		 m_modflags;

	//
	// routines
	//

		inline biguint_t<S>* const _upcast() { return static_cast<biguint_t<S>*>(this); }

		// >>> callback interface for simulated virtual functions

			inline static void _cb_copyraw( const void *handle, mpz_t *out ) {
				static_cast<const bigmod_t<S,pow2bits>*>(handle)->copyraw(out);
			}

		// >>> smart pointer management for modulus

			#define make(ee) { ee=mathbankaccess_t<mpz_t,S,biguint_t<S>>::bank_t::allocnode(); }
			#define kill(ee) { if(pow2bits>0) { ee->m_maske->m_bank->freenode(ee->m_maske); } ee->m_bank->freenode(ee); }
				
			inline bankentry_t* _initpow2mod( bankentry_t *r ) {
				mpz_t &v=r->m_v[0], &vtmp=this->b.m_vtmp[0];
				mpz_set_ui( v, 1 );
				mpz_mul_2exp( vtmp, v, pow2bits );
				this->b.swaptmp( &r );
				make( r->m_maske );
				mpz_sub_ui( r->m_maske->m_v[0], vtmp, 1 );
				return r;
			} //r=(2^pow2bits)-1

			inline bankentry_t* _refmod( bankentry_t *ptr ) 					{ ptr->m_refcnt++; return(ptr); }
			inline void 		_derefmod( bankentry_t *ptr ) 					{ if(--ptr->m_refcnt<=0) kill(ptr); }
			inline void 		_changemod( bankentry_t **a, bankentry_t *b )	{ b->m_refcnt++; _derefmod(a[0]); a[0]=b; }

			inline bankentry_t* _genmod( const mpz_t *d ) {
				bankentry_t *r; make(r);
				if(pow2bits>0) 	r = _initpow2mod(r);
				else 			mpz_set( r->m_v[0], d );
				r->m_refcnt=1;
				return(r);
			}

			inline bankentry_t* _genmod( int d ) {
				bankentry_t *r; make(r);
				if(pow2bits>0) 	r = _initpow2mod(r);
				else 			mpz_set_si( r->m_v[0], d );
				r->m_refcnt=1;
				return(r);
			}

			#undef make
			#undef kill

		// >>> maintain number in modulus

			inline void _cleantarget( mpz_t *rhs ) const {
				if(pow2bits>0)	mpz_and( rhs[0], this->b.m_v[0], m_modptr->m_maske->m_v[0] );
				else			mpz_mod( rhs[0], this->b.m_v[0], m_modptr->m_v[0] );
			}

			inline void _doclean() {
				if(pow2bits>0) 	_upcast()[0]&=m_modptr->m_maske->m_v;	//and mask for pow2 fields
				else 			_upcast()[0]%=m_modptr->m_v;			//actual modulus
				_markclean();
			}

			inline void _clean() 		{ if((m_modflags&FLG_CLEAN)==0) _doclean(); }
			inline void _markclean() 	{ m_modflags|=FLG_CLEAN; }
			inline void _dirty() 		{ m_modflags&=(~FLG_CLEAN); }

	inline bigmod_t( 									   	) : bigmodbase_t<S>( _cb_copyraw ),	     m_modptr(_genmod(1)),  		  m_modflags(0)	  	 	 	 {}	//cppcheck-suppress noExplicitConstructor
	inline bigmod_t( 						int d 		   	) : bigmodbase_t<S>( _cb_copyraw ),      m_modptr(_genmod(d)), 			  m_modflags(0)				 {}	//cppcheck-suppress noExplicitConstructor
	inline bigmod_t( 						const mpz_t *d 	) : bigmodbase_t<S>( _cb_copyraw ),      m_modptr(_genmod(d)), 			  m_modflags(0)				 {}	//cppcheck-suppress noExplicitConstructor
	inline bigmod_t( int rhs, 				const mpz_t *d 	) : bigmodbase_t<S>( rhs, _cb_copyraw ), m_modptr(_genmod(d)), 			  m_modflags(0) 			 {}	//cppcheck-suppress noExplicitConstructor
	inline bigmod_t( int rhs, 				int d 		   	) : bigmodbase_t<S>( rhs, _cb_copyraw ), m_modptr(_genmod(d)), 			  m_modflags(0) 			 {}	//cppcheck-suppress noExplicitConstructor
	inline bigmod_t( bigmod_t &rhs			   	  		   	) : bigmodbase_t<S>( rhs, _cb_copyraw ), m_modptr(_refmod(rhs.m_modptr)), m_modflags(rhs.m_modflags) {} //cppcheck-suppress noExplicitConstructor
	inline bigmod_t( int rhs,				bankentry_t *d	) : bigmodbase_t<S>( rhs, _cb_copyraw ), m_modptr(_refmod(d)), 	 		  m_modflags(0) 			 {} //cppcheck-suppress noExplicitConstructor
	inline bigmod_t( const mpz_t *rhs, 		bankentry_t *d 	) : bigmodbase_t<S>( rhs, _cb_copyraw ), m_modptr(_refmod(d)),		 	  m_modflags(0) 			 {}	//cppcheck-suppress noExplicitConstructor
	inline bigmod_t( const mpz_t *rhs, 		const mpz_t *d 	) : bigmodbase_t<S>( rhs, _cb_copyraw ), m_modptr(_genmod(d)), 			  m_modflags(0) 			 {}

	inline ~bigmod_t() 																		{ SAFE() _derefmod(m_modptr); }
	//the above is deliberately *not* virtual for performance

	inline 			int 					operator=( int rhs )							{ SAFE() _dirty(); 				 				    				    _upcast()->operator=(rhs); return(rhs);   }
	inline const  mpz_t* 					operator=( const mpz_t *rhs )					{ SAFE() _dirty(); 				 				    				    _upcast()->operator=(rhs); return(rhs);   }
	inline        bigmod_t<S,pow2bits>& 	operator=( const bigmod_t<S,pow2bits> &rhs )	{ SAFE() _changemod(&m_modptr,rhs.m_modptr); m_modflags=rhs.m_modflags; _upcast()->operator=(rhs); return(*this); } //pverload to avoid structure copy errors

	inline void operator+=(  const mpz_t *rhs )  											{ SAFE() _upcast()->_add(rhs); _dirty(); }												//dirty
	inline void operator-=(  const mpz_t *rhs )  											{ SAFE() _upcast()->_sub(rhs); _dirty(); }												//dirty
	inline void operator*=(  const mpz_t *rhs )  											{ SAFE() _upcast()->_mul(rhs); _doclean(); } 											//reduce now
	inline void operator/=(  const mpz_t *rhs ) 	 										{ SAFE() bigmod_t<S,pow2bits> _rhs(rhs); this[0]*=_rhs._inverse(); }					//multiply reduces
	inline void operator%=(  const mpz_t *rhs )  											{ SAFE() _clean(); _upcast()->_mod(rhs); }
	inline void operator<<=( const mpz_t *rhs )  											{ SAFE() bigmod_t<S,pow2bits> _rhs(2,m_modptr); this[0]*=_rhs._pow(rhs); }				//multiply reduces
	inline void operator>>=( const mpz_t *rhs )  											{ SAFE() bigmod_t<S,pow2bits> _rhs(2,m_modptr); this[0]*=_rhs._inverse()._pow(rhs); }	//multiply reduces
	inline void operator&=(  const mpz_t *rhs ) 											{ SAFE() _clean(); _upcast()->_and(rhs); }
	inline void operator|=(  const mpz_t *rhs ) 											{ SAFE() _clean(); _upcast()->_or(rhs);  _dirty(); }									//dirty
	inline void operator^=(  const mpz_t *rhs ) 											{ SAFE() _clean(); _upcast()->_xor(rhs); _dirty(); }									//dirty

	inline void operator+=( const int rhs ) 												{ SAFE() _upcast()->_add(rhs); _dirty(); }												//dirty
	inline void operator-=( const int rhs ) 												{ SAFE() _upcast()->_sub(rhs); _dirty(); }												//dirty
	inline void operator*=( const int rhs ) 												{ SAFE() _upcast()->_mul(rhs); _doclean(); }											//reduce now
	inline void operator/=( const int rhs ) 	 											{ SAFE() bigmod_t<S,pow2bits> _rhs(rhs,m_modptr); this[0]*=_rhs._inverse(); }			//multiply reduces
	inline void operator%=( const int rhs ) 												{ SAFE() _clean(); _upcast()->_mod(rhs); }
	inline void operator<<=( const int rhs ) 												{ SAFE() bigmod_t<S,pow2bits> _rhs(2,m_modptr); this[0]*=_rhs._pow(rhs); }				//multiply reduces
	inline void operator>>=( const int rhs ) 												{ SAFE() bigmod_t<S,pow2bits> _rhs(2,m_modptr); this[0]*=_rhs._inverse()._pow(rhs); }	//multiply reduces
	inline void operator&=( const int rhs ) 												{ SAFE() _clean(); _upcast()->_and(rhs); }
	inline void operator|=( const int rhs ) 												{ SAFE() _clean(); _upcast()->_or(rhs);  _dirty(); }									//dirty
	inline void operator^=( const int rhs ) 												{ SAFE() _clean(); _upcast()->_xor(rhs); _dirty(); }									//dirty

	inline 		 			 		biguint_t<S>& base()									{ SAFE() _clean(); return _upcast()[0]; }
	inline 		 			 	 	mpz_t* raw()											{ SAFE() _clean(); return (this->b.m_v); }
	inline 		 	operator const 	mpz_t*()												{ SAFE() _clean(); return (this->b.m_v); }
	inline explicit operator 		biguint_t<S>*()											{ SAFE() _clean(); return _upcast(); }
	inline explicit operator 		unsigned int() 		 									{ SAFE() _clean(); return (unsigned int)(_upcast()[0]); }
	inline explicit operator 		int() 				 									{ SAFE() _clean(); return (int)(_upcast()[0]); }
	inline explicit operator const 	char*()													{ SAFE() _clean(); return (const char*)(_upcast()[0]); }

	//additional modular specific routines

		inline bigmod_t<S,pow2bits>& _inverse() 												{ mpz_invert(  this->b.m_vtmp[0], this->b.m_v[0], 		  m_modptr->m_v[0] ); this->b.swap(); _markclean(); return(*this); }
		inline bigmod_t<S,pow2bits>& _pow( const mpz_t *rhs )									{ mpz_powm(    this->b.m_vtmp[0], this->b.m_v[0], rhs[0], m_modptr->m_v[0] ); this->b.swap(); _markclean(); return(*this); }
		inline bigmod_t<S,pow2bits>& _pow( int rhs )											{ mpz_powm_ui( this->b.m_vtmp[0], this->b.m_v[0], rhs, 	  m_modptr->m_v[0] ); this->b.swap(); _markclean(); return(*this); }

	inline bigmod_t<S,pow2bits> inverse() 													{ SAFE() bigmod_t<S,pow2bits> _rhs(this[0]); return(_rhs._inverse()); }
	inline bigmod_t<S,pow2bits> pow( const mpz_t *rhs )										{ SAFE() bigmod_t<S,pow2bits> _rhs(this[0]); return(_rhs._pow(rhs)); }
	inline bigmod_t<S,pow2bits> pow( int rhs )												{ SAFE() bigmod_t<S,pow2bits> _rhs(this[0]); return(_rhs._pow(rhs)); }

	inline 			void 			changemod( const mpz_t *rhs ) 							{ SAFE() _changemod(&m_modptr,_genmod(rhs)); _dirty(); }
	inline 			void 			changemod( const bankentry_t *rhs ) 					{ SAFE() _changemod(&m_modptr,rhs); _dirty(); }
	inline 		 	void 			copyraw( mpz_t *rhs )							  const { SAFE() _cleantarget(rhs); }
	inline const 	bankentry_t*	getmod() 										  const { SAFE() return(m_modptr); }
	inline const 	mpz_t*			getmodraw() 									  const { SAFE() return(m_modptr->m_v); }

	//optimized chinese remainder theorem - function requires sz>0
	static biguint_t<S> crt( bigmod_t *v, int sz, bigmod_t *scratch ) {
		int x;
		biguint_t<S> r(v[0]), delta;
		for(x=1;x<=sz;x++) { scratch[x] = bigmod_t(v[0],v[x].getmod()); }
		//for(x=1;x<=sz;x++) {
			//delta=v[x]-scratch[x]; //requires operators to satisfy
		//}
	}
};

#ifndef BIGMATHNOTYPES
typedef bigmod_t<128,0>   bigmod128_t;
typedef bigmod_t<256,0>   bigmod256_t;
typedef bigmod_t<512,0>   bigmod512_t;
typedef bigmod_t<1024,0>  bigmod1024_t;
typedef bigmod_t<2048,0>  bigmod2048_t;
typedef bigmod_t<4096,0>  bigmod4096_t;
typedef bigmod_t<8192,0>  bigmod8192_t;
typedef bigmod_t<16384,0> bigmod16384_t;
#endif


//
// long-lived stream acceleration numbers
//

template <int S>
struct bigstream_t : bigmod_t<(S*3),S> {

	static_assert(S>0,"error: bigstream_t <= 0");

	//initialize
	//cppcheck-suppress duplInheritedMember
	SAFEHEAD(bigstream_t)
	
	//
	// routines
	//
	
	inline bigstream_t() : bigmod_t<(S*3),S>() {}
	
	
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
