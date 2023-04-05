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

#ifndef BIGMATHSTRBUFFERMAX
#define BIGMATHSTRBUFFERMAX 256
#endif

#ifndef BIGMATHBANKSIZE
#define BIGMATHBANKSIZE 100
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
		linkitem<bankentry_t>	m_item;
		bank_t 					*m_bank;
		T	 					*m_v;
		char					m_strbuffer[BIGMATHSTRBUFFERMAX];
	};

	struct bank_t {

		SAFEHEAD(bank_t)

		static const int BANKSIZE = BIGMATHBANKSIZE;

		const unsigned int c_alloc_limbs = (S/mp_bits_per_limb+(S%mp_bits_per_limb==0?0:1)+1);
		const unsigned int c_alloc_bits  = c_alloc_limbs*mp_bits_per_limb;

		static thread_local linkbase<bank_t> g_base, g_freebase;	//thread specific to avoid need for mutex locks
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
			for(int x=0;x<BANKSIZE;x++) CBT::_cbinit(&m_v[x],c_alloc_bits);
		}

		~bank_t() {
			SAFE()
			g_base.remove(&m_item);
			if(isbankfree()) g_freebase.remove(&m_freeitem);
			for(int x=0;x<BANKSIZE;x++) CBT::_cbdeinit(&m_v[x],c_alloc_bits);
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
			CBT::_cbrealloc(&e->m_v[0],c_alloc_bits);				//reset gmp memory of this entry - prevents memory creep
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

		swapv = m_v;
		m_v = m_vtmp;
		m_vtmp = swapv;
		
	}

};

template <typename T, int S, typename CBT>
thread_local linkbase<typename mathbankaccess_t<T,S,CBT>::bank_t> mathbankaccess_t<T,S,CBT>::bank_t::g_base;

template <typename T, int S, typename CBT>
thread_local linkbase<typename mathbankaccess_t<T,S,CBT>::bank_t> mathbankaccess_t<T,S,CBT>::bank_t::g_freebase;


//
// uint code
//

template <int S>
struct biguint_t {

	SAFEHEAD(biguint_t<S>)

	//callbacks for the mathbank
	inline static void _cbinit(mpz_t *v, int size) 	 		{ mpz_init2(v[0],size); 	}
	inline static void _cbdeinit(mpz_t *v, int size) 		{ mpz_clear(v[0]); 		 	}
	inline static void _cbrealloc(mpz_t *v, int size) 		{ mpz_realloc2(v[0],size); 	}

	//initialize math bank
	mathbankaccess_t<mpz_t,S,biguint_t<S>> b;

	//user routines
	#define make(e,v) e=mathbankaccess_t<mpz_t,S,biguint_t<S>>::bank_t::allocnode(); v=e->m_v;
	#define kill(e)   if(e) { e->m_bank->freenode(e); }

	inline void _init() 									{ SAFE() make(b.m_e,b.m_v)   make(b.m_etmp,b.m_vtmp) }
	inline ~biguint_t() 									{ SAFE() kill(b.m_e)         kill(b.m_etmp)          }

	#undef make
	#undef kill

	inline biguint_t()  									{ _init(); }
	inline biguint_t( int val )  							{ _init(); this->operator=(val); }
	inline biguint_t( const biguint_t<S> &rhs )  			{ _init(); this->operator=(rhs); }

	inline void operator=( int val ) 						{ SAFE() mpz_set_ui(b.m_v[0],(unsigned long int)val);  }
	inline void operator=( const biguint_t<S> &rhs )		{ SAFE() mpz_set(b.m_v[0],rhs.b.m_v[0]); }

	inline void _neg()		 								{ SAFE() mpz_neg(b.m_v[0],b.m_v[0]); }
	inline void _abs()		 								{ SAFE() mpz_abs(b.m_v[0],b.m_v[0]); }

	inline bool _eq( const biguint_t<S> &rhs ) const 		{ SAFE() return(mpz_cmp( b.m_v[0], rhs.b.m_v[0] ) == 0);  }
	inline bool _gt( const biguint_t<S> &rhs ) const 		{ SAFE() return(mpz_cmp( b.m_v[0], rhs.b.m_v[0] ) > 0);  }
	inline bool _gte( const biguint_t<S> &rhs ) const 		{ SAFE() return(mpz_cmp( b.m_v[0], rhs.b.m_v[0] ) >= 0); }
	inline bool _lt( const biguint_t<S> &rhs ) const 		{ SAFE() return(mpz_cmp( b.m_v[0], rhs.b.m_v[0] ) < 0);  }
	inline bool _lte( const biguint_t<S> &rhs ) const 		{ SAFE() return(mpz_cmp( b.m_v[0], rhs.b.m_v[0] ) <= 0); }
	inline void _add( const biguint_t<S> &rhs ) 			{ SAFE() mpz_add( b.m_v[0], b.m_v[0], rhs.b.m_v[0] ); }
	inline void _sub( const biguint_t<S> &rhs ) 			{ SAFE() mpz_sub( b.m_v[0], b.m_v[0], rhs.b.m_v[0] ); }
	inline void _mul( const biguint_t<S> &rhs ) 			{ SAFE() mpz_mul( b.m_vtmp[0], b.m_v[0], rhs.b.m_v[0] ); b.swap(); }
	inline void _div( const biguint_t<S> &rhs ) 			{ SAFE() mpz_tdiv_q( b.m_vtmp[0], b.m_v[0], rhs.b.m_v[0] ); b.swap(); } //tdiv to handle signed overload
	inline void _mod( const biguint_t<S> &rhs ) 			{ SAFE() mpz_mod( b.m_vtmp[0], b.m_v[0], rhs.b.m_v[0] ); b.swap(); }

	inline bool _eq( const int rhs ) const 					{ SAFE() return(mpz_cmp_ui( b.m_v[0], (unsigned long int)rhs ) == 0);  }
	inline bool _gt( const int rhs ) const 					{ SAFE() return(mpz_cmp_ui( b.m_v[0], (unsigned long int)rhs ) > 0);  }
	inline bool _gte( const int rhs ) const 				{ SAFE() return(mpz_cmp_ui( b.m_v[0], (unsigned long int)rhs ) >= 0); }
	inline bool _lt( const int rhs ) const 					{ SAFE() return(mpz_cmp_ui( b.m_v[0], (unsigned long int)rhs ) < 0);  }
	inline bool _lte( const int rhs ) const 				{ SAFE() return(mpz_cmp_ui( b.m_v[0], (unsigned long int)rhs ) <= 0); }
	inline void _add( const int rhs ) 						{ SAFE() mpz_add_ui( b.m_v[0], b.m_v[0], (unsigned long int)rhs ); }
	inline void _sub( const int rhs ) 						{ SAFE() mpz_sub_ui( b.m_v[0], b.m_v[0], (unsigned long int)rhs ); }
	inline void _mul( const int rhs ) 						{ SAFE() mpz_mul_ui( b.m_vtmp[0], b.m_v[0], (unsigned long int)rhs ); b.swap(); }
	inline void _div( const int rhs ) 						{ SAFE() mpz_fdiv_q_ui( b.m_vtmp[0], b.m_v[0], (unsigned long int)rhs ); b.swap(); } //fdiv as should never be used for signed cases
	inline void _mod( const int rhs ) 						{ SAFE() mpz_mod_ui( b.m_vtmp[0], b.m_v[0], (unsigned long int)rhs ); b.swap(); }

	double double_from_div( const biguint_t<S> &d, int precision=64 ) const {

		SAFE()

		double result;	//; n/d = n//d + (n%d)/d
		biguint_t<S> q, r, dmod(d);
		int tb, shift;

		mpz_fdiv_qr( q.b.m_v[0], r.b.m_v[0], b.m_v[0], d.b.m_v[0] );	// q=n//d, r=n%d
		result  = mpz_get_d( q.b.m_v[0] );

		tb 	  = mpz_sizeinbase(d.b.m_v[0],2)+1;							// gives top bit, function gives 1-based index
		shift = ((tb-precision)>0)?(tb-precision):0;

		mpz_fdiv_q_2exp(r.b.m_v[0],r.b.m_v[0],shift);					// r >> shift
		mpz_fdiv_q_2exp(dmod.b.m_v[0],d.b.m_v[0],shift); 				// dmod >> shift

		result += mpz_get_d(r.b.m_v[0]) / mpz_get_d(dmod.b.m_v[0]) ;

		return(result);

	}

	inline void operator+=(const biguint_t<S> &rhs) 		{ _add(rhs); }
	inline void operator-=(const biguint_t<S> &rhs) 		{ _sub(rhs); }
	inline void operator*=(const biguint_t<S> &rhs) 		{ _mul(rhs); }
	inline void operator/=(const biguint_t<S> &rhs) 		{ _div(rhs); }
	inline void operator%=(const biguint_t<S> &rhs) 		{ _mod(rhs); }

	inline void operator+=(const int rhs) 					{ _add(rhs); }
	inline void operator-=(const int rhs) 					{ _sub(rhs); }
	inline void operator*=(const int rhs) 					{ _mul(rhs); }
	inline void operator/=(const int rhs) 					{ _div(rhs); }
	inline void operator%=(const int rhs) 					{ _mod(rhs); }

	operator const char*() const {
		SAFE()
		gmp_snprintf( b.m_e->m_strbuffer, sizeof(b.m_e->m_strbuffer), "%Zd", b.m_v[0] ); 
		return (b.m_e->m_strbuffer);
	}

	operator const double() 		const 					{ SAFE() return(mpz_get_d(b.m_v[0]));  }
	operator const unsigned int() 	const 					{ SAFE() return(mpz_get_ui(b.m_v[0]));   }
};

typedef biguint_t<128>   biguint128_t;
typedef biguint_t<256>   biguint256_t;
typedef biguint_t<512>   biguint512_t;
typedef biguint_t<1024>  biguint1024_t;
typedef biguint_t<2048>  biguint2048_t;
typedef biguint_t<4096>  biguint4096_t;
typedef biguint_t<8192>  biguint8192_t;
typedef biguint_t<16384> biguint16384_t;


//
// integer code
//

template <int S>
struct bigint_t : biguint_t<S> {

	SAFEHEAD(bigint_t<S>)
		
	inline bigint_t() : biguint_t<S>()								{}
	inline bigint_t( int val ) : biguint_t<S>() 		 			{ this->operator=(val); }
	inline bigint_t( const bigint_t<S> &rhs ) : biguint_t<S>( rhs ) {}

	inline void operator=( int val ) 								{ SAFE() mpz_set_si( this->b.m_v[0], val ); }

	inline bool _eq( const int rhs ) const 							{ SAFE() return(mpz_cmp_si( this->b.m_v[0], rhs ) == 0);  }
	inline bool _gt( const int rhs ) const 							{ SAFE() return(mpz_cmp_si( this->b.m_v[0], rhs ) > 0);  }
	inline bool _gte( const int rhs ) const 						{ SAFE() return(mpz_cmp_si( this->b.m_v[0], rhs ) >= 0); }
	inline bool _lt( const int rhs ) const 							{ SAFE() return(mpz_cmp_si( this->b.m_v[0], rhs ) < 0);  }
	inline bool _lte( const int rhs ) const 						{ SAFE() return(mpz_cmp_si( this->b.m_v[0], rhs ) <= 0); }
	inline void _add( const int rhs ) 								{ SAFE() bigint_t<S> r(rhs); this[0]+=r; } //invokes below += overload to forward to unsigned processing
	inline void _sub( const int rhs ) 								{ SAFE() bigint_t<S> r(rhs); this[0]-=r; } //invokes below -= overload to forward to unsigned processing
	inline void _mul( const int rhs ) 								{ SAFE() mpz_mul_si( this->b.m_vtmp[0], this->b.m_v[0], rhs ); this->b.swap(); }
	inline void _div( const int rhs ) 								{ SAFE() bigint_t<S> r(rhs); this[0]/=r; } //invokes below /= overload to forward to unsigned processing
	inline void _mod( const int rhs ) 								{ SAFE() bigint_t<S> r(rhs); this[0]%=r; } //invokes below %= overload to forward to unsigned processing

	inline void operator+=(const bigint_t<S> &rhs)  				{ ((biguint_t<S>*)this)->_add(rhs); }
	inline void operator-=(const bigint_t<S> &rhs)  				{ ((biguint_t<S>*)this)->_sub(rhs); }
	inline void operator*=(const bigint_t<S> &rhs)  				{ ((biguint_t<S>*)this)->_mul(rhs); }
	inline void operator/=(const bigint_t<S> &rhs)  				{ ((biguint_t<S>*)this)->_div(rhs); }
	inline void operator%=(const bigint_t<S> &rhs)  				{ ((biguint_t<S>*)this)->_mod(rhs); }

	inline void operator+=(const int rhs) 							{ _add(rhs); }
	inline void operator-=(const int rhs) 							{ _sub(rhs); }
	inline void operator*=(const int rhs) 							{ _mul(rhs); }
	inline void operator/=(const int rhs) 							{ _div(rhs); }
	inline void operator%=(const int rhs) 							{ _mod(rhs); }
	
	operator const int() const 										{ SAFE() return(mpz_get_si(this->b.m_v[0]));   }	
};

typedef bigint_t<128>   bigint128_t;
typedef bigint_t<256>   bigint256_t;
typedef bigint_t<512>   bigint512_t;
typedef bigint_t<1024>  bigint1024_t;
typedef bigint_t<2048>  bigint2048_t;
typedef bigint_t<4096>  bigint4096_t;
typedef bigint_t<8192>  bigint8192_t;
typedef bigint_t<16384> bigint16384_t;


//
// fractional code
//

template <int S>
struct bigfrac_t {
	
	SAFEHEAD(bigfrac_t<S>)
		
	//callbacks for the mathbank
	inline static void _cbinit(mpq_t *v, int size) 	 			{ mpq_init(v[0]); }
	inline static void _cbdeinit(mpq_t *v, int size)  			{ mpq_clear(v[0]); }
	inline static void _cbrealloc(mpq_t *v, int size) 			{ mpq_clear(v[0]); mpq_init(v[0]); }

	//initialize math bank
	mathbankaccess_t<mpq_t,S,bigfrac_t<S>> b;

	//user routines
	#define make(e,v) e=mathbankaccess_t<mpq_t,S,bigfrac_t<S>>::bank_t::allocnode(); v=e->m_v;
	#define kill(e)   if(e) { e->m_bank->freenode(e); }

	inline void _init() 										{ SAFE() make(b.m_e,b.m_v)   make(b.m_etmp,b.m_vtmp) }
	inline ~bigfrac_t() 										{ SAFE() kill(b.m_e)         kill(b.m_etmp)          }

	#undef make
	#undef kill

	inline bigfrac_t()  										{ _init(); }
	inline bigfrac_t( int val )  								{ _init(); this->operator=(val); }
	inline bigfrac_t( double val )  							{ _init(); this->operator=(val); }
	inline bigfrac_t( const biguint_t<S> &rhs )  				{ _init(); this->operator=(rhs); }
	inline bigfrac_t( const bigfrac_t<S> &rhs )  				{ _init(); this->operator=(rhs); }

	inline void operator=( int val ) 							{ SAFE() mpq_set_si(b.m_v[0],val,1); }
	inline void operator=( double val ) 						{ SAFE() mpq_set_d(b.m_v[0],val); }
	inline void operator=( const biguint_t<S> &rhs ) 			{ SAFE() mpq_set_z(b.m_v[0],rhs.b.m_v[0]); }
	inline void operator=( const bigfrac_t<S> &rhs ) 			{ SAFE() mpq_set(b.m_v[0],rhs.b.m_v[0]); }

	inline void _neg()		 									{ SAFE() mpq_neg(b.m_v[0],b.m_v[0]); }
	inline void _abs()		 									{ SAFE() mpq_abs(b.m_v[0],b.m_v[0]); }

	inline void _add( const bigfrac_t<S> &rhs ) 				{ SAFE() mpq_add(b.m_vtmp[0], b.m_v[0], rhs.b.m_v[0]); b.swap(); }
	inline void _sub( const bigfrac_t<S> &rhs ) 				{ SAFE() mpq_sub(b.m_vtmp[0], b.m_v[0], rhs.b.m_v[0]); b.swap(); }
	inline void _mul( const bigfrac_t<S> &rhs ) 				{ SAFE() mpq_mul(b.m_vtmp[0], b.m_v[0], rhs.b.m_v[0]); b.swap(); }
	inline void _div( const bigfrac_t<S> &rhs ) 				{ SAFE() mpq_div(b.m_vtmp[0], b.m_v[0], rhs.b.m_v[0]); b.swap(); }

	inline bool _eq( const bigfrac_t<S> &rhs ) const 			{ SAFE() return(mpq_cmp( b.m_v[0], rhs.b.m_v[0] ) == 0);  }
	inline bool _gt( const bigfrac_t<S> &rhs ) const 			{ SAFE() return(mpq_cmp( b.m_v[0], rhs.b.m_v[0] ) > 0);  }
	inline bool _gte( const bigfrac_t<S> &rhs ) const 			{ SAFE() return(mpq_cmp( b.m_v[0], rhs.b.m_v[0] ) >= 0); }
	inline bool _lt( const bigfrac_t<S> &rhs ) const 			{ SAFE() return(mpq_cmp( b.m_v[0], rhs.b.m_v[0] ) < 0);  }
	inline bool _lte( const bigfrac_t<S> &rhs ) const 			{ SAFE() return(mpq_cmp( b.m_v[0], rhs.b.m_v[0] ) <= 0); }

	inline bool _eq( const double &rhs ) const 					{ SAFE() return(mpq_get_d( b.m_v[0] ) == 0);  }
	inline bool _gt( const double &rhs ) const 					{ SAFE() return(mpq_get_d( b.m_v[0] ) > 0);  }
	inline bool _gte( const double &rhs ) const 				{ SAFE() return(mpq_get_d( b.m_v[0] ) >= 0); }
	inline bool _lt( const double &rhs ) const 					{ SAFE() return(mpq_get_d( b.m_v[0] ) < 0);  }
	inline bool _lte( const double &rhs ) const 				{ SAFE() return(mpq_get_d( b.m_v[0] ) <= 0); }

	inline bigfrac_t<S> abs()									{ SAFE(); bigfrac_t<S> r; mpq_abs(r.b.m_v[0],b.m_v[0]); return(r); }

	bigint_t<S> round()	{ 
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

	inline void operator+=( const bigfrac_t<S> &rhs ) 			{ _add(rhs); }
	inline void operator-=( const bigfrac_t<S> &rhs ) 			{ _sub(rhs); }
	inline void operator*=( const bigfrac_t<S> &rhs ) 			{ _mul(rhs); }
	inline void operator/=( const bigfrac_t<S> &rhs ) 			{ _div(rhs); }

	operator const char*() const {
		SAFE()
		gmp_snprintf( b.m_e->m_strbuffer, sizeof(b.m_e->m_strbuffer), "%Qd", b.m_v[0] ); 
		return (b.m_e->m_strbuffer);
	}

	operator const double() const 								{ SAFE() return(mpq_get_d(b.m_v[0]));  }
	operator const int() 	const 								{ SAFE() return((int)mpq_get_d(b.m_v[0])); }
};

typedef bigfrac_t<128>   bigfrac128_t;
typedef bigfrac_t<256>   bigfrac256_t;
typedef bigfrac_t<512>   bigfrac512_t;
typedef bigfrac_t<1024>  bigfrac1024_t;
typedef bigfrac_t<2048>  bigfrac2048_t;
typedef bigfrac_t<4096>  bigfrac4096_t;
typedef bigfrac_t<8192>  bigfrac8192_t;
typedef bigfrac_t<16384> bigfrac16384_t;

//
// stream multiplier numbers
//



//
// global operators
//

#include "bigmathoperators.h"

#endif
