Oblivious transfer using projective, double-blinded, partially masked carry counting with undisclosed composite modulus. 

Notation: 

	// is truncated division
	[] denotes an optional list
	0. denotes a binary division where the resulting digit before the decimal is zero
	f. denotes a binary division where the resulting digit before the decimal is unknown

Conceptual model:

	General idea:

		Most of the work is delegated to peers upon request, but some of the work is retained
			by the requestor to improve their privacy. 

		Peers perform the response/proxy operations on the carry count to enable an oblivious 
			transfer protocol where (n) and its factors work as a secret key. Blindings help 
			prevent trivial privacy breaks. 

		We do this in such a way where middle parties (proxies) can verify the construction was 
			legal (without ability to decode it).

	Carry counting preliminaries:

		It is born from the following modulus projection function:

			f(x,n,q) = (x + (x//n)(q-n)) Mod q = (x Mod n) Mod q

		Where:

			x = 2^e
			n = undisclosed requestor modulus
			q = pow2 = 2^maxcarries

		Yielding:

			f(x,n,pow2)  =  (2^e + ((2^e)//n)(pow2-n)) Mod pow2  =  ((2^e) Mod n) Mod pow2

			If e >= maxcarries then:

				f(x,n,q) = ((2^e)//n)(-n) Mod pow2

			If (-n) is stripped off by requestor we then have:

				f(x,n,q)/-n  Mod pow2 = (2^e)//n Mod pow2

			Or more specifically, one bit set for each carry of the last (maxcarries) 
				iterations of multiplying in base (2). 

			Crucially:

				 The carries are measured as overflows of the undisclosed modulus (n)

				 Any legal (2^e Mod n) value can be restored using the carries in a 
					pow2 field if we know (n). 


	Immutable encoding provides data in the following form:

		;where Ti, BiL, and BiR are symmetrically encrypted content pieces

		sum = (Ti+BiL+BiR)
		d1  = (Ti-BiL)
		d2  = (Ti-BiR)

                Negatives: 

                       The above needs to be packed as multiple blocks of data into a single message to cater
                               to the block size differentials, which grants the OT encoder the ability to 
                               "boost" Ti to prevent negative numbers.

                       Signatures should take this boosting into account.

                       The requestor can perform the "unboost" on their end after Ti is retrieved if that is
                               the element they are seeking.

	Specific idea:

		Normally determining cosets in small moduli is trivial ...IF... you know the modulus ...AND... its factors.

		Even when the moduli is known, it is difficult to find interior cosets on large composite moduli with large
			interior factors where you do not know any of the factors of (n) - see multiprime RSA.

		By using composite moduli and also stripping out knowledge of the modulus, we have potentially strengthened 
			coset hiding (e.g. against ECM attacks) which helps with oblivious transfer protocols.

		The idea is to create random spins about an unspecified coset in an unspecified modulus, where the offsets from
			the coset indicate the content math to be performed. 

		Normally (when modulus is known), completing a request works out to simple multiplies and additions. e.g.

			================================
			b  = blinding
			b0 = 2^b 		Mod primes
			b1 = (2^b)[1,-2] 	Mod primes
			b2 = (2^b)[1,-2] 	Mod primes
			response = ( (b0)(sum) + (b1)(d1) + (b2)(d2) ) Mod primes
			decode   = (response)(2^-b)(3^-1)              Mod primes
			================================

		The above model is prone to a trivial attack where the "b[0,1,2]" variables can be matched.

		The model can be extended with an exterior factor brought into the modulus:

			================================
			q  = randomprimes
			n  = primes * q

			coset  = λ(primes)
			qspace = λ(n)/coset
			b      = rand(coset)

			r0 = (2^(coset*rand(qspace))) 	Mod (n)				;note that this is (2^0) Mod primes
			r1 = (2^(coset*rand(qspace))) 	Mod (n)				;note that this is (2^0) Mod primes
			r2 = (2^(coset*rand(qspace))) 	Mod (n)				;note that this is (2^0) Mod primes

			b0 = (r0)(2^b) 			Mod (n)				;r0 does not change anything when going Mod primes
			b1 = (r1)(2^b)[1,-2] 		Mod (n)				;r1 does not change anything when going Mod primes
			b2 = (r2)(2^b)[1,-2]	 	Mod (n)				;r2 does not change anything when going Mod primes

			response = ( (b0)(sum) + (b1)(d1) + (b2)(d2) ) Mod (n)		;note this is unchanged from original sample (other than modulus)
			decode   = ( (response)(2^-b)(3^-1) )          Mod (primes)	;note this is unchanged from original sample
			================================

		This, in essense, provides a double-blinding. One common blinding (b) and then an individual blinding (r...) for each factor. 

			Security note: If the primes of (n) can be known this is not secure. (see security document)

		To reinforce security, we strip out knowledge of (n), adding the burden of calculating only in carry space.

		To facilitate proper carry counting the values < n are converted to fixed point decimal (b0/n,b1/n,b2/n) with 
			enough resolution such that multiplication/addition operations can offset the carry properly. 

		After decoding, the peer should always check the  content to guard against trojan infection if the oblivious transfer 
			protocol/implementation is ever compromised or broken.

        Proxy verification/signatures:

                Some (possibly many computers) will be high speed low storage computers. We need to render these nodes useful network members.

                For them to be useful, they need not to be confused with a malicious peer (i.e. they must not relay malformed content). 
                        Specifically, if any content is malformed from a peer the conclusion must be that this immediate peer is the 
                        responsible party so the software can respond appropriately (e.g. forced disconnect). 

                A key observeration is that the content for a stream does not change, only the (b0,b1,b2) factors do. This allows
                        us to pregenerate random polynomial coefficients to function as a weak signature scheme. It is "weak" in the
                        sense that it requires an adversary to guess at which polynomial set the proxy is verifying against to malform
                        content in a way that would pass. The objective is to reduce their odds enough that detecting malicious peers
                        has a high enough probability to prevent them from getting too far into disrupting the network. 

		Requestor transmits in following form (to hide "n"):

                        ======================================================================
                        pow2    = 2^(enough_space_for_main_message)
                        pow2sig = 2^(enough_space_for_main_message + signatures)

			B0 = ((b0)(-n)^-1) Mod pow2
			B1 = ((b1)(-n)^-1) Mod pow2
			B2 = ((b2)(-n)^-1) Mod pow2

			F0 = floor( 0.(b0/n) * pow2sig )		;truncated binary representation of traditional divide
			F1 = floor( 0.(b1/n) * pow2sig )		;truncated binary representation of traditional divide
			F2 = floor( 0.(b2/n) * pow2sig )		;truncated binary representation of traditional divide
                        ======================================================================

                This motivates the following signature checking:

                        ======================================================================
			sigunpad(x) = (x >> sigbuffbits)	;eliminate signature buffer space

                        P = POLYS[polyselected]
                        m = P.modulus
                        x = P.xvalue

                        checkC = 0
                        checkF = 0

                        for i in range(0,blockcount):
                                B = blocks[i]
                                X = (x^(i+1))%m
			        checkC += ( ( (B0)(B.SUM) +  (B1)(B.D1) +  (B2)(B.D2) )	        )(X)	  	Mod (pow2)
			        checkF += ( ( (F0)(B.SUM) +  (F1)(B.D1) +  (F2)(B.D2) ) // pow2 )(X)	  	Mod (pow2sig)

			check = ( checkC + sigunpad(checkF) ) Mod (pow2)

                        challenge  =           ( (B0)P.SUMPOLY + (B1)P.D1POLY + (B2)P.D2POLY )         		Mod (pow2)
                        challenge += sigunpad( ( (F0)P.SUMPOLY + (F1)P.D1POLY + (F2)P.D2POLY ) // pow2 ) 	Mod (pow2)

                        if check != challenge:
                                ;content is malicious
                        ======================================================================

		Why this works: Signature padding allows //pow2 to happen in different orders provided (X) multiplier
				does not exceed the resolution of the padding and the challenge polys are not wrapped.

		Therefore: SUMPOLY, D1POLY, and D2POLY are "compressed" polynomials of the form:

                        ======================================================================
			for pindex in range(0,polycount):

		                P = POLYS[pindex]
		                m = P.modulus = get_ranged_prime( hash(decodekeys,pindex), sigcoefficientmax )
		                x = P.xvalue = hash(decodekeys,pindex,m) % m

				P.SUMPOLY = 0
				P.D1POLY  = 0
				P.D2POLY  = 0

		                for i in range(0,blockcount):
		                        B = blocks[i]
		                        X = (x^(i+1))%m
					P.SUMPOLY += (X)(B.SUM) 	;should not overflow (pow2sig)
					P.D1POLY  += (X)(B.D1)  	;should not overflow (pow2sig)
					P.D2POLY  += (X)(B.D2)  	;should not overflow (pow2sig)
                        ======================================================================

		Note that a peer may validate multiple polynomials against the data to reduce the odds of forgery (see notes at bottom).

	Privacy improvement:
	
		In the above form, privacy depends on the inability to solve: ((F0)(n)) // pow2 == ((-B0)(n)) Mod pow2  (see security analysis)

		Privacy may be tightened by a subtle change where we recognize that encoded data may only be of bit size (nbits-kbits)

		With this we are able to safely truncate (kbits) worth of precision off of the fractional component. 


Implementation:

	Requestor construction:

		braid_element_count 			= number of elements in OT braid
		sigblockcount				= number of blocks to a signature

		primebits				= minimum size of each primary prime 				;possibly rsa sized
		sigcoefficientmaxbits			= maximum number of bits for coefficients in signature polynomial
		pow2bits 				= top bit position for n ~= 2^(primebits*primecount)
		kbits					= primebits = security level (in bits)

		sigbuffbits				= ceil(log2(sigblockcount+braid_element_count)+sigcoefficientmaxbits)

		k					= 2^kbits
		sigbuff					= 2^sigbuffbits
		sigcoefficientmax			= 2^sigcoefficientmaxbits

		pow2 					= 2^pow2bits
		pow2sig	 				= 2^(pow2bits+sigbuffbits)

		qprime					= genprimes(kbits,kbits)			;generate small randomizer prime
		primes[]				= genprimes(pow2bits-kbits,primebits)		;generate list of primes who multiply up to first param
		n					= (primes)(qprime)
		coset					= λ(primes)
		qspace 					= λ(n)/coset

		MASKH					= (pow2sig-1)-(kbits-1)

		r0 = (2^(coset*rand(qspace))) 		Mod n
		r1 = (2^(coset*rand(qspace))) 		Mod n
		r2 = (2^(coset*rand(qspace))) 		Mod n

		b  = rand(coset)
		b0 = ((r0)(2^b))        		Mod n					;[] denote optional selection from list
		b1 = ((r1)(2^b)[1,-2])  		Mod n					;[] denote optional selection from list
		b2 = ((r2)(2^b)[1,-2])  		Mod n					;[] denote optional selection from list

		B0 = ((b0)(-n)^-1)			Mod pow2
		B1 = ((b1)(-n)^-1)			Mod pow2
		B2 = ((b2)(-n)^-1)			Mod pow2

		F0 = floor( 0.(b0/n) * pow2sig ) & MASKH					;truncated binary representation of traditional divide
		F1 = floor( 0.(b1/n) * pow2sig ) & MASKH					;truncated binary representation of traditional divide
		F2 = floor( 0.(b2/n) * pow2sig ) & MASKH					;truncated binary representation of traditional divide

		;transmit B0, B1, B2, F0, F1, F2

	Prover response:

		sigpad(x)   = (x << sigbuffbits)			;create signature buffer space

		Bp = ( (B0)sum +  (B1)d1 +  (B2)d2 ) 		Mod pow2 		;primary carries
		Bf = ( (F0)sum +  (F1)d1 +  (F2)d2 ) // pow2 	Mod pow2sig 		;fractional carries
		B  = ( sigpad(Bp) + Bf ) 			Mod pow2sig		;final regular carry, should not overflow (pow2sig)(2)

		;transmit B

	Requestor decoding:

		;privacy note: requestor should also likely use proxy checking code 
		;	       first to render itself indistinguishable from a proxy
		;	       in event of malicious peer

		sigunpad(x)  = (x >> sigbuffbits)				;eliminate signature buffer space

		B = sigunpad(B)(-n)			Mod pow2		;convert back to modular form
		B = ((B)(2^-b)(3^-1))			Mod primes		;(sum) + (d1)[1,-2] + (d2)[1,-2]

		;requestor should perform a traditional hash check of data after enough blocks are downloaded

	Proxy checking (requestor may also use):

		At upstream request:

			Bacc=0

                        P = POLYS[rand()%polysetsize]
                        m = P.modulus
                        x = P.xvalue
											;	the x-values themselves may be deterministic
		Downstream per-block processing:

			;i = current index in block set for verification
			X = (x^i) Mod m
			Bacc = ( Bacc + (B)(X) ) Mod pow2sig

		Downstream multi-block verification:

			sigunpad(x) = (x >> sigbuffbits)					;eliminate signature buffer space

			;see prior notes for implementation of poly calculations

			BtestC = (  (B0)P.SUMPOLY +  (B1)P.D1POLY +  (B2)P.D2POLY )
                        BtestF = (  (F0)P.SUMPOLY +  (F1)P.D1POLY +  (F2)P.D2POLY ) // pow2
			Btest  = ( BtestC + sigunpad(BtestF) ) Mod pow2

			    	  Btest =? sigunpad(Bacc Mod pow2sig)

	Proxy:

		Simulate Tor relays. PoW lease agreements established for relaying. 

		A proxy will always give the appearance of establishing connections to many exterior 
			nodes even if they hold the requested content themselves.

		Exception: If target data is sufficiently far away (high depth) network peer should negotiate
				a STUN connection if both peers at both ends consent to being directly connected.
				A proxy will only facilitate this if the depth is inappropriately high. This
				is for the purposes of preventing a single attacker from too easily harvesting
				the IPs of all participants in the network. 

Optimizations:

	The pow2 fields are essentially binary fields performing standard binary arithmetic. No actual modulus
		work here is necessary beyond trimming overflow bits in the last processor register. 

	When irregular modulus work is performed it is done only by the requestor. Since the requestor knows
		the individual primes of (n) they are able to use the Chinese Remainder Theorem (CRT) function
		to reconstruct the larger number much more efficiently by encoding/decoding each small prime 
		individually.

	The proxy checking math is ideal performance wise as it only needs to multiply the passing data by small 
		"compressed" polynomial coefficients, preventing heavy multiplication burden on proxies. Their
		only heavy check is final verification of many blocks, but even here the modulus is pow2 aligned. 

Packing and data sizes:

	If 128 bit smallest prime in composite modulus requires, this is 128 bits of waste for OT before signature buffers.
	We want 8320 bits per message before signature buffers. 
	This provides 8192 bits for actual user data (64 x 128-bit blocks)
	If 1/47.67 for OT waste is permitted then (128+48)/(1/47.67) = 8390 bit messages (1048 bytes) ;< 8196 bit messages desired for speed, <1050 bytes required for tunnels
	If we want signatures for every 3MiB of data this works out to maximum blockcount of of: (3*1024*1024*8)/8390 = 2999~ blocks
	With (8390−48−1) bits of actual data permitted per message (pow2) and +24 bits to sum a run of blocks ((sigblockcount)(sigcoefficientmax))
		bringing us to (8390-48-1+24)=8365 bits per thread signature with 3x this figure for a single braid sig (25095 bits)
		This allows one packet containing the signtature for a set of blocks for one thread.
		If there are 3 threads in a braid, 3 packets would be required to sign the entire braid.
		This would be multiplied by the number of duplicate sigs.
	If we accept up to 1/30 for OT waste + sigs this yields (1/30-1/47.67)(3*1024*1024*8) = 310943 bits for sigs
	If we take the above figure and divide it by braid sig size we get: (310943) / (25095) = (12 sigs)
	12 sigs with 6 chosen at random yields:
		(12!)/((6!)((12-6)!)) = 924  ;1/924 odds of forgery one proxy level deep
	If we presume 256KiB per second on privacy networks, this yields a buffering time of (3)(1024)/256 = 12 seconds
	It is for this we reason it is suggestable to have variable block length with the possibility of fewer data blocks
		at the beginning of each video stream (where the number of blocks provided allow at least 12 seconds of play back time)
	

