#! /usr/bin/python3

#NeoPeer Oblivious Transfer Sample
#License: MIT
#Date: April 6, 2023

#includes
from imports.numbthy import *
from imports.modmath import *
from imports.utility import *
from imports.py3_classes import *
#from decimal import Decimal,Context
from decimal import *
from fractions import Fraction
import timeit
import sys

def ceil(v): return int(math.ceil(v))
def floor(v): return int(math.floor(v))

# user variables
primebits = 128
modulusbits = 8320
blockcount = 1000
#sanitycheck = True
sanitycheck = False

# initialization 
print("Generating secure primes. Please wait.")
primelist 	= genprimes(modulusbits-primebits,primebits)
qprime    	= genprimes(primebits,primebits)[0]
coset     	= primelist[0]-1
for x in range(1,len(primelist)):
	coset = compute_lcm(coset,primelist[x]-1)
qspace 		= compute_lcm( coset, qprime-1 ) / coset
primes 		= arrayproduct(primelist)
n      		= primes*qprime
encodingrange 	= 2**(modulusbits-primebits-(modulusbits-primebits)//primebits*2)	#SECURITY: Check if the floor for primes is a security risk

#immutable encoding of random data
print("Generating blocks. Please wait.")
SUM  = [0]*blockcount
D1   = [0]*blockcount
D2   = [0]*blockcount
DATA = []
for block in range(0,blockcount):

	DATA.append([0]*3)

	DATA[block][0]  = srand(encodingrange) + encodingrange 		#boosted encoding to prevent negatives
	DATA[block][1]  = srand(encodingrange)
	DATA[block][2]  = srand(encodingrange)

	if DATA[block][0]>primes:
		print("ERROR: Data encoding exceeds private modulus. Stopping.")
		sys.exit()

	SUM[block]	= DATA[block][0] + DATA[block][1] + DATA[block][2];
	D1[block]	= DATA[block][0] - DATA[block][1];
	D2[block]	= DATA[block][0] - DATA[block][2];

#initial status print
print("n-size in bits:", log2(n))
print("q-space in bits:", log2(qspace))
print("coset size in bits:", log2(coset))

###############################################################################################
#
# SAMPLE3: carry counting oblivious transfer (even stronger) sample with signature checking
#
###############################################################################################
#massive class for all the polynomical verification steps at the end. Likely candidate for breaking up later...
#
# fixed network variables
#

print("")
print("Sample 3 -",blockcount,"blocks... initializing request and preparing signatures...")

decodekeys				= "abc"					#decode keys for a waypoint of blocks
polycount				= 12					#coded for optimal bandwidth wasteage size
braid_element_count 			= 3
sigblockcount				= blockcount				#should be hard-coded to 3000 (see ot.spec.9)

security = Security(128, sigblockcount, braid_element_count, 12, 8320, 128)
#variables need to be pulled out for now will make another pass to get them called straight from the class later.
sigcoefficientmax=security.sigcoefficientmax
pow2sig=security.pow2sig
pow2=security.pow2
pow2sigbits=security.pow2sigbits
pow2bits=security.pow2bits
sigbuffbits=security.sigbuffbits

#
# signature generation by network admin or publisher
#

"""
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
		P.SUMPOLY += (X)(B.SUM) ;should not exceed (pow2sig)
		P.D1POLY  += (X)(B.D1)  ;should not exceed (pow2sig)
		P.D2POLY  += (X)(B.D2)  ;should not exceed (pow2sig)
"""

polynomial_generator = PolynomialGenerator(decodekeys, polycount, sigcoefficientmax, blockcount, SUM, D1, D2, pow2sig, sigbuffbits)
POLYS = polynomial_generator.generate_polynomials()

#
# content request generation
#

"""
pow2    = 2^(enough_space_for_main_message)
pow2sec = 2^(enough_space_for_main_message + signatures)

B0 = ((b0)(-n)^-1) Mod pow2
B1 = ((b1)(-n)^-1) Mod pow2
B2 = ((b2)(-n)^-1) Mod pow2

F0 = floor( 0.(b0/n) * pow2sig )		;truncated binary representation of traditional divide
F1 = floor( 0.(b1/n) * pow2sig )		;truncated binary representation of traditional divide
F2 = floor( 0.(b2/n) * pow2sig )		;truncated binary representation of traditional divide
"""

masks = FastMasks(pow2, pow2sig)
#again pulling values out for now. Will change it all up later.
p2mask = masks.p2mask
p2sigmask = masks.p2sigmask

#precompute blindings and inverses
blindandinverse = BlindingsAndInverses(coset, primelist, primes)
#again pulling out variables. Will sort once everything is in class.
b = blindandinverse.b
blind = blindandinverse.blind
iblind = blindandinverse.iblind
i3 = blindandinverse.i3
unblind = blindandinverse.unblind

#select content to decode
sindex = SIndex(srand(3))
s1 = sindex.s1
s2 = sindex.s2

#fast version of modulus powers in prior sanitycheck
modpowers = ModulusPower(qspace, b, coset, qprime, blind, primes, s1, s2, n)
b0, b1, b2 = modpowers.generate_random_numbers()
#prepare carry information

carry_info = PrepCarryInformation(b0, b1, b2, n, pow2sigbits, pow2)
carry_info.prepare_carry_information()
carry_info.run_sanity_check(sanitycheck)
B0 = carry_info.B0
B1 = carry_info.B1
B2 = carry_info.B2
F0 = carry_info.F0
F1 = carry_info.F1
F2 = carry_info.F2

#
# content fetching (encoding and decoding)
#

#>>> Encode/decode blocks

print("Sample 3 -",blockcount,"blocks... executing...")

block_coder = BlockCoder(DATA, sindex, B0, B1, B2, F0, F1, F2, pow2bits, pow2sig, primes, unblind, p2mask, p2sigmask, SUM, D1, D2, sigbuffbits, n)

storedB = []
sampleclock = timeit.default_timer()
for block in range(blockcount):
	B = block_coder.encode_block(block)
	storedB.append(B)
	block_coder.decode_block(B, block)

print("Sample 3 -",blockcount,"blocks... completed in",(timeit.default_timer()-sampleclock),"seconds")


#>>> Verify blocks (proxy and possibly recipient)
poly_verifier = PolynomialVerifier(polycount, blockcount, storedB, POLYS, p2sigmask, p2mask, sigbuffbits, B0, B1, B2, F0, F1, F2, pow2bits)
poly_verifier.verify()
print("Sample 3 -",blockcount,"verification... completed in",(timeit.default_timer()-sampleclock),"seconds")


"""
Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

This permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
"""
