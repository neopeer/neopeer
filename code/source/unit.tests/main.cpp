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

		
		/*
	***

		inline biguint_t()  														{ _init(); }
		inline biguint_t( int val )  												{ _init(); this->operator=(val); }
		inline biguint_t( const biguint_t<S> &rhs )  								{ _init(); this->operator=(rhs); }
		inline void operator=( int val ) 											{ SAFE() mpz_set_ui(b.m_v[0],(unsigned long int)val);  }
		inline void operator=( const biguint_t<S> &rhs )							{ SAFE() mpz_set(b.m_v[0],rhs.b.m_v[0]); }
		inline void _neg()		 													{ SAFE() mpz_neg(b.m_v[0],b.m_v[0]); }
		inline void _abs()		 													{ SAFE() mpz_abs(b.m_v[0],b.m_v[0]); }
		double double_from_div( const biguint_t<S> &d, int precision=64 ) const	 	{
		operator const char*() const 												{}
		operator const double() 		const 										{ SAFE() return(mpz_get_d(b.m_v[0]));  }
		operator const unsigned int() 	const 										{ SAFE() return(mpz_get_ui(b.m_v[0]));   }

		inline bool operator!=( const biguint_t<S> &lhs, const biguint_t<S> &rhs ) 	{ return lhs._eq(rhs);  }
		inline bool operator==( const biguint_t<S> &lhs, const biguint_t<S> &rhs ) 	{ return lhs._eq(rhs);  }
		inline bool operator>( const biguint_t<S> &lhs, const biguint_t<S> &rhs ) 	{ return lhs._gt(rhs);  }
		inline bool operator>=( const biguint_t<S> &lhs, const biguint_t<S> &rhs ) 	{ return lhs._gte(rhs);  }
		inline bool operator<( const biguint_t<S> &lhs, const biguint_t<S> &rhs ) 	{ return lhs._lt(rhs);  }
		inline bool operator<=( const biguint_t<S> &lhs, const biguint_t<S> &rhs ) 	{ return lhs._lte(rhs);  }
		inline biguint_t<S> operator+( biguint_t<S> lhs, const biguint_t<S> &rhs ) 	{ lhs+=rhs; return(lhs); }
		inline biguint_t<S> operator-( biguint_t<S> lhs, const biguint_t<S> &rhs ) 	{ lhs-=rhs; return(lhs); }
		inline biguint_t<S> operator*( biguint_t<S> lhs, const biguint_t<S> &rhs ) 	{ lhs*=rhs; return(lhs); }
		inline biguint_t<S> operator/( biguint_t<S> lhs, const biguint_t<S> &rhs ) 	{ lhs/=rhs; return(lhs); }
		inline biguint_t<S> operator%( biguint_t<S> lhs, const biguint_t<S> &rhs ) 	{ lhs%=rhs; return(lhs); }

		inline bool operator!=( const biguint_t<S> &lhs, const int rhs ) 			{ return lhs._eq(rhs);  }
		inline bool operator==( const biguint_t<S> &lhs, const int rhs ) 			{ return lhs._eq(rhs);  }
		inline bool operator>( const biguint_t<S> &lhs, const int rhs ) 			{ return lhs._gt(rhs);  }
		inline bool operator>=( const biguint_t<S> &lhs, const int rhs ) 			{ return lhs._gte(rhs);  }
		inline bool operator<( const biguint_t<S> &lhs, const int rhs ) 			{ return lhs._lt(rhs);  }
		inline bool operator<=( const biguint_t<S> &lhs, const int rhs ) 			{ return lhs._lte(rhs);  }
		inline biguint_t<S> operator+( biguint_t<S> lhs, const int rhs ) 			{ lhs+=rhs; return(lhs); }
		inline biguint_t<S> operator-( biguint_t<S> lhs, const int rhs ) 			{ lhs-=rhs; return(lhs); }
		inline biguint_t<S> operator*( biguint_t<S> lhs, const int rhs ) 			{ lhs*=rhs; return(lhs); }
		inline biguint_t<S> operator/( biguint_t<S> lhs, const int rhs ) 			{ lhs/=rhs; return(lhs); }
		inline biguint_t<S> operator%( biguint_t<S> lhs, const int rhs ) 			{ lhs%=rhs; return(lhs); }

		//int lhs overloads
		inline bool operator!=( const int lhs, const biguint_t<S> &rhs ) 			{ return rhs._eq(lhs); }
		inline bool operator==( const int lhs, const biguint_t<S> &rhs ) 			{ return rhs._eq(lhs); }
		inline bool operator>( const int lhs, const biguint_t<S> &rhs ) 			{ return rhs._lt(lhs); }
		inline bool operator>=( const int lhs, const biguint_t<S> &rhs ) 			{ return rhs._lte(lhs);  }
		inline bool operator<( const int lhs, const biguint_t<S> &rhs ) 			{ return rhs._gt(lhs); }
		inline bool operator<=( const int lhs, const biguint_t<S> &rhs ) 			{ return rhs._gte(lhs); }
		inline biguint_t<S> operator+( const int lhs, biguint_t<S> rhs ) 			{ rhs+=lhs; return(rhs); }
		inline biguint_t<S> operator-( const int lhs, biguint_t<S> rhs ) 			{ rhs._neg(); rhs+=lhs; return(rhs); }
		inline biguint_t<S> operator*( const int lhs, biguint_t<S> rhs ) 			{ rhs*=lhs; return(rhs); }
		inline biguint_t<S> operator/( const int lhs, const biguint_t<S> &rhs ) 	{ biguint_t<S> _lhs(lhs); _lhs/=rhs; return(_lhs); }
		inline biguint_t<S> operator%( const int lhs, const biguint_t<S> &rhs ) 	{ biguint_t<S> _lhs(lhs); _lhs%=rhs; return(_lhs); }

	***

		inline bigint_t() : biguint_t<S>()											{}
		inline bigint_t( int val ) : biguint_t<S>() 		 						{ this->operator=(val); }
		inline bigint_t( const bigint_t<S> &rhs ) : biguint_t<S>( rhs ) 			{}
		operator const int() const 													{ SAFE() return(mpz_get_si(this->b.m_v[0]));   }	

		inline void operator=( int val ) 											{ SAFE() mpz_set_si( this->b.m_v[0], val ); }
		//biginteger overloads
		inline bool operator!=( const bigint_t<S> &lhs, const bigint_t<S> &rhs ) 	{ return lhs._eq(rhs);  }
		inline bool operator==( const bigint_t<S> &lhs, const bigint_t<S> &rhs ) 	{ return lhs._eq(rhs);  }
		inline bool operator>( const bigint_t<S> &lhs, const bigint_t<S> &rhs ) 	{ return lhs._gt(rhs);  }
		inline bool operator>=( const bigint_t<S> &lhs, const bigint_t<S> &rhs ) 	{ return lhs._gte(rhs);  }
		inline bool operator<( const bigint_t<S> &lhs, const bigint_t<S> &rhs ) 	{ return lhs._lt(rhs);  }
		inline bool operator<=( const bigint_t<S> &lhs, const bigint_t<S> &rhs ) 	{ return lhs._lte(rhs);  }
		inline bigint_t<S> operator+( bigint_t<S> lhs, const bigint_t<S> &rhs ) 	{ lhs+=rhs; return(lhs); }
		inline bigint_t<S> operator-( bigint_t<S> lhs, const bigint_t<S> &rhs ) 	{ lhs-=rhs; return(lhs); }
		inline bigint_t<S> operator*( bigint_t<S> lhs, const bigint_t<S> &rhs ) 	{ lhs*=rhs; return(lhs); }
		inline bigint_t<S> operator/( bigint_t<S> lhs, const bigint_t<S> &rhs ) 	{ lhs/=rhs; return(lhs); }
		inline bigint_t<S> operator%( bigint_t<S> lhs, const bigint_t<S> &rhs ) 	{ lhs%=rhs; return(lhs); }

		//int rhs overloads
		inline bool operator!=( const bigint_t<S> &lhs, const int rhs ) 			{ return lhs._eq(rhs);  }
		inline bool operator==( const bigint_t<S> &lhs, const int rhs ) 			{ return lhs._eq(rhs);  }
		inline bool operator>( const bigint_t<S> &lhs, const int rhs ) 				{ return lhs._gt(rhs);  }
		inline bool operator>=( const bigint_t<S> &lhs, const int rhs ) 			{ return lhs._gte(rhs);  }
		inline bool operator<( const bigint_t<S> &lhs, const int rhs ) 				{ return lhs._lt(rhs);  }
		inline bool operator<=( const bigint_t<S> &lhs, const int rhs ) 			{ return lhs._lte(rhs);  }
		inline bigint_t<S> operator+( bigint_t<S> lhs, const int rhs ) 				{ lhs+=rhs; return(lhs); }
		inline bigint_t<S> operator-( bigint_t<S> lhs, const int rhs ) 				{ lhs-=rhs; return(lhs); }
		inline bigint_t<S> operator*( bigint_t<S> lhs, const int rhs ) 				{ lhs*=rhs; return(lhs); }
		inline bigint_t<S> operator/( bigint_t<S> lhs, const int rhs ) 				{ lhs/=rhs; return(lhs); }
		inline bigint_t<S> operator%( bigint_t<S> lhs, const int rhs ) 				{ lhs%=rhs; return(lhs); }

		//int lhs overloads
		inline bool operator!=( const int lhs, const bigint_t<S> &rhs ) 			{ return rhs._eq(lhs); }
		inline bool operator==( const int lhs, const bigint_t<S> &rhs ) 			{ return rhs._eq(lhs); }
		inline bool operator>( const int lhs, const bigint_t<S> &rhs ) 				{ return rhs._lt(lhs); }
		inline bool operator>=( const int lhs, const bigint_t<S> &rhs ) 			{ return rhs._lte(lhs);  }
		inline bool operator<( const int lhs, const bigint_t<S> &rhs ) 				{ return rhs._gt(lhs); }
		inline bool operator<=( const int lhs, const bigint_t<S> &rhs ) 			{ return rhs._gte(lhs); }
		inline bigint_t<S> operator+( const int lhs, bigint_t<S> rhs ) 				{ rhs+=lhs; return(rhs); }
		inline bigint_t<S> operator-( const int lhs, bigint_t<S> rhs ) 				{ rhs._neg(); rhs+=lhs; return(rhs); }
		inline bigint_t<S> operator*( const int lhs, bigint_t<S> rhs ) 				{ rhs*=lhs; return(rhs); }
		inline bigint_t<S> operator/( const int lhs, const bigint_t<S> &rhs )	 	{ bigint_t<S> _lhs(lhs); _lhs/=rhs; return(_lhs); }
		inline bigint_t<S> operator%( const int lhs, const bigint_t<S> &rhs )	 	{ bigint_t<S> _lhs(lhs); _lhs%=rhs; return(_lhs); }

	***

		inline bigfrac_t()  														{ _init(); }
		inline bigfrac_t( int val )  												{ _init(); this->operator=(val); }
		inline bigfrac_t( double val )  											{ _init(); this->operator=(val); }
		inline bigfrac_t( const biguint_t<S> &rhs )  								{ _init(); this->operator=(rhs); }
		inline bigfrac_t( const bigfrac_t<S> &rhs )  								{ _init(); this->operator=(rhs); }

		inline void operator=( int val ) 											{ SAFE() mpq_set_si(b.m_v[0],val,1); }
		inline void operator=( double val ) 										{ SAFE() mpq_set_d(b.m_v[0],val); }
		inline void operator=( const biguint_t<S> &rhs ) 							{ SAFE() mpq_set_z(b.m_v[0],rhs.b.m_v[0]); }
		inline void operator=( const bigfrac_t<S> &rhs ) 							{ SAFE() mpq_set(b.m_v[0],rhs.b.m_v[0]); }

		inline void _neg()		 													{ SAFE() mpq_neg(b.m_v[0],b.m_v[0]); }
		inline void _abs()		 													{ SAFE() mpq_abs(b.m_v[0],b.m_v[0]); }

		inline bigfrac_t<S> abs()													{ SAFE(); bigfrac_t<S> r; mpq_abs(r.b.m_v[0],b.m_v[0]); return(r); }
		bigint_t<S> round()	{ 

		operator const char*() const 												{
		operator const double() const 												{ SAFE() return(mpq_get_d(b.m_v[0]));  }
		operator const int() 	const 												{ SAFE() return((int)mpq_get_d(b.m_v[0])); }

		//frac overloads
		inline bool operator!=( const bigfrac_t<S> &lhs, const bigfrac_t<S> &rhs ) 	{ return lhs._eq(rhs);  }
		inline bool operator==( const bigfrac_t<S> &lhs, const bigfrac_t<S> &rhs ) 	{ return lhs._eq(rhs);  }
		inline bool operator>( const bigfrac_t<S> &lhs, const bigfrac_t<S> &rhs ) 	{ return lhs._gt(rhs);  }
		inline bool operator>=( const bigfrac_t<S> &lhs, const bigfrac_t<S> &rhs ) 	{ return lhs._gte(rhs);  }
		inline bool operator<( const bigfrac_t<S> &lhs, const bigfrac_t<S> &rhs ) 	{ return lhs._lt(rhs);  }
		inline bool operator<=( const bigfrac_t<S> &lhs, const bigfrac_t<S> &rhs ) 	{ return lhs._lte(rhs);  }
		inline bigfrac_t<S> operator+( bigfrac_t<S> lhs, const bigfrac_t<S> &rhs ) 	{ lhs+=rhs; return(lhs); }
		inline bigfrac_t<S> operator-( bigfrac_t<S> lhs, const bigfrac_t<S> &rhs ) 	{ lhs-=rhs; return(lhs); }
		inline bigfrac_t<S> operator*( bigfrac_t<S> lhs, const bigfrac_t<S> &rhs ) 	{ lhs*=rhs; return(lhs); }
		inline bigfrac_t<S> operator/( bigfrac_t<S> lhs, const bigfrac_t<S> &rhs ) 	{ lhs/=rhs; return(lhs); }

		//double rhs overloads
		inline bool operator!=( const bigfrac_t<S> &lhs, const double &rhs )		 { return lhs._eq(rhs);  }
		inline bool operator==( const bigfrac_t<S> &lhs, const double &rhs )		 { return lhs._eq(rhs);  }
		inline bool operator>( const bigfrac_t<S> &lhs, const double &rhs )		 	{ return lhs._gt(rhs);  }
		inline bool operator>=( const bigfrac_t<S> &lhs, const double &rhs )		{ return lhs._gte(rhs);  }
		inline bool operator<( const bigfrac_t<S> &lhs, const double &rhs )		 	{ return lhs._lt(rhs);  }
		inline bool operator<=( const bigfrac_t<S> &lhs, const double &rhs )		{ return lhs._lte(rhs);  }
		inline bigint_t<S> operator+( bigint_t<S> lhs, const double &rhs ) 			{ lhs+=rhs; return(lhs); }
		inline bigint_t<S> operator-( bigint_t<S> lhs, const double &rhs ) 			{ lhs-=rhs; return(lhs); }
		inline bigint_t<S> operator*( bigint_t<S> lhs, const double &rhs ) 			{ lhs*=rhs; return(lhs); }
		inline bigint_t<S> operator/( bigint_t<S> lhs, const double &rhs ) 			{ lhs/=rhs; return(lhs); }

		//double lhs overloads
		inline bool operator!=( const double &lhs, const bigfrac_t<S> &rhs ) 		{ return rhs._eq(lhs); }
		inline bool operator==( const double &lhs, const bigfrac_t<S> &rhs ) 		{ return rhs._eq(lhs); }
		inline bool operator>( const double &lhs, const bigfrac_t<S> &rhs ) 		{ return rhs._lt(lhs); }
		inline bool operator>=( const double &lhs, const bigfrac_t<S> &rhs ) 		{ return rhs._lte(lhs);  }
		inline bool operator<( const double &lhs, const bigfrac_t<S> &rhs ) 		{ return rhs._gt(lhs); }
		inline bool operator<=( const double &lhs, const bigfrac_t<S> &rhs ) 		{ return rhs._gte(lhs); }
		inline bigfrac_t<S> operator+( const double &lhs, bigfrac_t<S> rhs ) 		{ rhs+=lhs; return(rhs); }
		inline bigfrac_t<S> operator-( const double &lhs, bigfrac_t<S> rhs ) 		{ rhs._neg(); rhs+=lhs; return(rhs); }
		inline bigfrac_t<S> operator*( const double &lhs, bigfrac_t<S> rhs ) 		{ rhs*=lhs; return(rhs); }
		inline bigfrac_t<S> operator/( const double &lhs, const bigfrac_t<S> &rhs )	{ bigfrac_t<S> _lhs(lhs); _lhs/=rhs; return(_lhs); }
		*/
		
		
	}
	
	void start() {
		testbigmath();
	}
	
};

