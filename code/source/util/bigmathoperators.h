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


#ifndef BIGMATHOVERLOADS_H
#define BIGMATHOVERLOADS_H

//
// ********* biguint_t overloads ********
//

template <int S>
inline bool operator!=( const biguint_t<S> &lhs, const biguint_t<S> &rhs ) 	{ return !lhs._eq(rhs); }

template <int S>
inline bool operator==( const biguint_t<S> &lhs, const biguint_t<S> &rhs ) 	{ return lhs._eq(rhs);  }

template <int S>
inline bool operator>( const biguint_t<S> &lhs, const biguint_t<S> &rhs ) 	{ return lhs._gt(rhs);  }

template <int S>
inline bool operator>=( const biguint_t<S> &lhs, const biguint_t<S> &rhs ) 	{ return lhs._gte(rhs);  }

template <int S>
inline bool operator<( const biguint_t<S> &lhs, const biguint_t<S> &rhs ) 	{ return lhs._lt(rhs);  }

template <int S>
inline bool operator<=( const biguint_t<S> &lhs, const biguint_t<S> &rhs ) 	{ return lhs._lte(rhs);  }

template <int S>
inline biguint_t<S> operator+( biguint_t<S> lhs, const biguint_t<S> &rhs ) 	{ lhs+=rhs; return(lhs); }

template <int S>
inline biguint_t<S> operator-( biguint_t<S> lhs, const biguint_t<S> &rhs ) 	{ lhs-=rhs; return(lhs); }

template <int S>
inline biguint_t<S> operator*( biguint_t<S> lhs, const biguint_t<S> &rhs ) 	{ lhs*=rhs; return(lhs); }

template <int S>
inline biguint_t<S> operator/( biguint_t<S> lhs, const biguint_t<S> &rhs ) 	{ lhs/=rhs; return(lhs); }

template <int S>
inline biguint_t<S> operator%( biguint_t<S> lhs, const biguint_t<S> &rhs ) 	{ lhs%=rhs; return(lhs); }


//int rhs overloads

template <int S>
inline bool operator!=( const biguint_t<S> &lhs, const int rhs ) 			{ return !lhs._eq(rhs);  }

template <int S>
inline bool operator==( const biguint_t<S> &lhs, const int rhs ) 			{ return lhs._eq(rhs);  }

template <int S>
inline bool operator>( const biguint_t<S> &lhs, const int rhs ) 			{ return lhs._gt(rhs);  }

template <int S>
inline bool operator>=( const biguint_t<S> &lhs, const int rhs ) 			{ return lhs._gte(rhs);  }

template <int S>
inline bool operator<( const biguint_t<S> &lhs, const int rhs ) 			{ return lhs._lt(rhs);  }

template <int S>
inline bool operator<=( const biguint_t<S> &lhs, const int rhs ) 			{ return lhs._lte(rhs);  }

template <int S>
inline biguint_t<S> operator+( biguint_t<S> lhs, const int rhs ) 			{ lhs+=rhs; return(lhs); }

template <int S>
inline biguint_t<S> operator-( biguint_t<S> lhs, const int rhs ) 			{ lhs-=rhs; return(lhs); }

template <int S>
inline biguint_t<S> operator*( biguint_t<S> lhs, const int rhs ) 			{ lhs*=rhs; return(lhs); }

template <int S>
inline biguint_t<S> operator/( biguint_t<S> lhs, const int rhs ) 			{ lhs/=rhs; return(lhs); }

template <int S>
inline biguint_t<S> operator%( biguint_t<S> lhs, const int rhs ) 			{ lhs%=rhs; return(lhs); }


//int lhs overloads

template <int S>
inline bool operator!=( const int lhs, const biguint_t<S> &rhs ) 			{ return !rhs._eq(lhs); }

template <int S>
inline bool operator==( const int lhs, const biguint_t<S> &rhs ) 			{ return rhs._eq(lhs); }

template <int S>
inline bool operator>( const int lhs, const biguint_t<S> &rhs ) 			{ return rhs._lt(lhs); }

template <int S>
inline bool operator>=( const int lhs, const biguint_t<S> &rhs ) 			{ return rhs._lte(lhs);  }

template <int S>
inline bool operator<( const int lhs, const biguint_t<S> &rhs ) 			{ return rhs._gt(lhs); }

template <int S>
inline bool operator<=( const int lhs, const biguint_t<S> &rhs ) 			{ return rhs._gte(lhs); }

template <int S>
inline biguint_t<S> operator+( const int lhs, biguint_t<S> rhs ) 			{ rhs+=lhs; return(rhs); }

template <int S>
inline biguint_t<S> operator-( const int lhs, biguint_t<S> rhs ) 			{ rhs._neg(); rhs+=lhs; return(rhs); }

template <int S>
inline biguint_t<S> operator*( const int lhs, biguint_t<S> rhs ) 			{ rhs*=lhs; return(rhs); }

template <int S>
inline biguint_t<S> operator/( const int lhs, const biguint_t<S> &rhs ) 	{ biguint_t<S> _lhs(lhs); _lhs/=rhs; return(_lhs); }

template <int S>
inline biguint_t<S> operator%( const int lhs, const biguint_t<S> &rhs ) 	{ biguint_t<S> _lhs(lhs); _lhs%=rhs; return(_lhs); }


//
// ********* bigint_t overloads ********
//

//biginteger overloads

template <int S>
inline bool operator!=( const bigint_t<S> &lhs, const bigint_t<S> &rhs ) 	{ return !lhs._eq(rhs);  }

template <int S>
inline bool operator==( const bigint_t<S> &lhs, const bigint_t<S> &rhs ) 	{ return lhs._eq(rhs);  }

template <int S>
inline bool operator>( const bigint_t<S> &lhs, const bigint_t<S> &rhs ) 	{ return lhs._gt(rhs);  }

template <int S>
inline bool operator>=( const bigint_t<S> &lhs, const bigint_t<S> &rhs ) 	{ return lhs._gte(rhs);  }

template <int S>
inline bool operator<( const bigint_t<S> &lhs, const bigint_t<S> &rhs ) 	{ return lhs._lt(rhs);  }

template <int S>
inline bool operator<=( const bigint_t<S> &lhs, const bigint_t<S> &rhs ) 	{ return lhs._lte(rhs);  }

template <int S>
inline bigint_t<S> operator+( bigint_t<S> lhs, const bigint_t<S> &rhs ) 	{ lhs+=rhs; return(lhs); }

template <int S>
inline bigint_t<S> operator-( bigint_t<S> lhs, const bigint_t<S> &rhs ) 	{ lhs-=rhs; return(lhs); }

template <int S>
inline bigint_t<S> operator*( bigint_t<S> lhs, const bigint_t<S> &rhs ) 	{ lhs*=rhs; return(lhs); }

template <int S>
inline bigint_t<S> operator/( bigint_t<S> lhs, const bigint_t<S> &rhs ) 	{ lhs/=rhs; return(lhs); }

template <int S>
inline bigint_t<S> operator%( bigint_t<S> lhs, const bigint_t<S> &rhs ) 	{ lhs%=rhs; return(lhs); }


//int rhs overloads

template <int S>
inline bool operator!=( const bigint_t<S> &lhs, const int rhs ) 			{ return !lhs._eq(rhs);  }

template <int S>
inline bool operator==( const bigint_t<S> &lhs, const int rhs ) 			{ return lhs._eq(rhs);  }

template <int S>
inline bool operator>( const bigint_t<S> &lhs, const int rhs ) 				{ return lhs._gt(rhs);  }

template <int S>
inline bool operator>=( const bigint_t<S> &lhs, const int rhs ) 			{ return lhs._gte(rhs);  }

template <int S>
inline bool operator<( const bigint_t<S> &lhs, const int rhs ) 				{ return lhs._lt(rhs);  }

template <int S>
inline bool operator<=( const bigint_t<S> &lhs, const int rhs ) 			{ return lhs._lte(rhs);  }

template <int S>
inline bigint_t<S> operator+( bigint_t<S> lhs, const int rhs ) 				{ lhs+=rhs; return(lhs); }

template <int S>
inline bigint_t<S> operator-( bigint_t<S> lhs, const int rhs ) 				{ lhs-=rhs; return(lhs); }

template <int S>
inline bigint_t<S> operator*( bigint_t<S> lhs, const int rhs ) 				{ lhs*=rhs; return(lhs); }

template <int S>
inline bigint_t<S> operator/( bigint_t<S> lhs, const int rhs ) 				{ lhs/=rhs; return(lhs); }

template <int S>
inline bigint_t<S> operator%( bigint_t<S> lhs, const int rhs ) 				{ lhs%=rhs; return(lhs); }


//int lhs overloads

template <int S>
inline bool operator!=( const int lhs, const bigint_t<S> &rhs ) 			{ return !rhs._eq(lhs); }

template <int S>
inline bool operator==( const int lhs, const bigint_t<S> &rhs ) 			{ return rhs._eq(lhs); }

template <int S>
inline bool operator>( const int lhs, const bigint_t<S> &rhs ) 				{ return rhs._lt(lhs); }

template <int S>
inline bool operator>=( const int lhs, const bigint_t<S> &rhs ) 			{ return rhs._lte(lhs);  }

template <int S>
inline bool operator<( const int lhs, const bigint_t<S> &rhs ) 				{ return rhs._gt(lhs); }

template <int S>
inline bool operator<=( const int lhs, const bigint_t<S> &rhs ) 			{ return rhs._gte(lhs); }

template <int S>
inline bigint_t<S> operator+( const int lhs, bigint_t<S> rhs ) 				{ rhs+=lhs; return(rhs); }

template <int S>
inline bigint_t<S> operator-( const int lhs, bigint_t<S> rhs ) 				{ rhs._neg(); rhs+=lhs; return(rhs); }

template <int S>
inline bigint_t<S> operator*( const int lhs, bigint_t<S> rhs ) 				{ rhs*=lhs; return(rhs); }

template <int S>
inline bigint_t<S> operator/( const int lhs, const bigint_t<S> &rhs )	 	{ bigint_t<S> _lhs(lhs); _lhs/=rhs; return(_lhs); }

template <int S>
inline bigint_t<S> operator%( const int lhs, const bigint_t<S> &rhs )	 	{ bigint_t<S> _lhs(lhs); _lhs%=rhs; return(_lhs); }



//
// ********* bigfrac_t overloads ********
//

template <int S>
inline bool operator!=( const bigfrac_t<S> &lhs, const bigfrac_t<S> &rhs ) 	{ return !lhs._eq(rhs);  }

template <int S>
inline bool operator==( const bigfrac_t<S> &lhs, const bigfrac_t<S> &rhs ) 	{ return lhs._eq(rhs);  }

template <int S>
inline bool operator>( const bigfrac_t<S> &lhs, const bigfrac_t<S> &rhs ) 	{ return lhs._gt(rhs);  }

template <int S>
inline bool operator>=( const bigfrac_t<S> &lhs, const bigfrac_t<S> &rhs ) 	{ return lhs._gte(rhs);  }

template <int S>
inline bool operator<( const bigfrac_t<S> &lhs, const bigfrac_t<S> &rhs ) 	{ return lhs._lt(rhs);  }

template <int S>
inline bool operator<=( const bigfrac_t<S> &lhs, const bigfrac_t<S> &rhs ) 	{ return lhs._lte(rhs);  }

template <int S>
inline bigfrac_t<S> operator+( bigfrac_t<S> lhs, const bigfrac_t<S> &rhs ) 	{ lhs+=rhs; return(lhs); }

template <int S>
inline bigfrac_t<S> operator-( bigfrac_t<S> lhs, const bigfrac_t<S> &rhs ) 	{ lhs-=rhs; return(lhs); }

template <int S>
inline bigfrac_t<S> operator*( bigfrac_t<S> lhs, const bigfrac_t<S> &rhs ) 	{ lhs*=rhs; return(lhs); }

template <int S>
inline bigfrac_t<S> operator/( bigfrac_t<S> lhs, const bigfrac_t<S> &rhs ) 	{ lhs/=rhs; return(lhs); }


//double rhs overloads

template <int S>
inline bool operator!=( const bigfrac_t<S> &lhs, const double &rhs )		 { return !lhs._eq(rhs);  }

template <int S>
inline bool operator==( const bigfrac_t<S> &lhs, const double &rhs )		 { return lhs._eq(rhs);  }

template <int S>
inline bool operator>( const bigfrac_t<S> &lhs, const double &rhs )		 	{ return lhs._gt(rhs);  }

template <int S>
inline bool operator>=( const bigfrac_t<S> &lhs, const double &rhs )		{ return lhs._gte(rhs);  }

template <int S>
inline bool operator<( const bigfrac_t<S> &lhs, const double &rhs )		 	{ return lhs._lt(rhs);  }

template <int S>
inline bool operator<=( const bigfrac_t<S> &lhs, const double &rhs )		{ return lhs._lte(rhs);  }

template <int S>
inline bigint_t<S> operator+( bigint_t<S> lhs, const double &rhs ) 			{ lhs+=rhs; return(lhs); }

template <int S>
inline bigint_t<S> operator-( bigint_t<S> lhs, const double &rhs ) 			{ lhs-=rhs; return(lhs); }

template <int S>
inline bigint_t<S> operator*( bigint_t<S> lhs, const double &rhs ) 			{ lhs*=rhs; return(lhs); }

template <int S>
inline bigint_t<S> operator/( bigint_t<S> lhs, const double &rhs ) 			{ lhs/=rhs; return(lhs); }


//double lhs overloads

template <int S>
inline bool operator!=( const double &lhs, const bigfrac_t<S> &rhs ) 		{ return !rhs._eq(lhs); }

template <int S>
inline bool operator==( const double &lhs, const bigfrac_t<S> &rhs ) 		{ return rhs._eq(lhs); }

template <int S>
inline bool operator>( const double &lhs, const bigfrac_t<S> &rhs ) 		{ return rhs._lt(lhs); }

template <int S>
inline bool operator>=( const double &lhs, const bigfrac_t<S> &rhs ) 		{ return rhs._lte(lhs);  }

template <int S>
inline bool operator<( const double &lhs, const bigfrac_t<S> &rhs ) 		{ return rhs._gt(lhs); }

template <int S>
inline bool operator<=( const double &lhs, const bigfrac_t<S> &rhs ) 		{ return rhs._gte(lhs); }

template <int S>
inline bigfrac_t<S> operator+( const double &lhs, bigfrac_t<S> rhs ) 		{ rhs+=lhs; return(rhs); }

template <int S>
inline bigfrac_t<S> operator-( const double &lhs, bigfrac_t<S> rhs ) 		{ rhs._neg(); rhs+=lhs; return(rhs); }

template <int S>
inline bigfrac_t<S> operator*( const double &lhs, bigfrac_t<S> rhs ) 		{ rhs*=lhs; return(rhs); }

template <int S>
inline bigfrac_t<S> operator/( const double &lhs, const bigfrac_t<S> &rhs )	{ bigfrac_t<S> _lhs(lhs); _lhs/=rhs; return(_lhs); }


#endif
