#! /usr/bin/python3

#NeoPeer Oblivious Transfer Sample
#License: MIT
#Date: April 6, 2023

#includes
from imports.numbthy import *
from imports.modmath import *
from imports.utility import * 
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

#polynomial class
class PolyClass:
	SUMPOLY	= 0
	D1POLY	= 0
	D2POLY	= 0
	xvalue	= 0
	modulus	= 0
	def __init__(self,m,x):
		self.modulus 	= m
		self.xvalue 	= x
		
#block encode and decode class
class BlockCoder:
	def __init__(self, data, sindex, B0, B1, B2, F0, F1, F2, pow2bits, pow2sig, primes, unblind, p2mask, p2sigmask):
		self.data = data
		self.sindex = sindex
		self.B0 = B0
		self.B1 = B1
		self.B2 = B2
		self.F0 = F0
		self.F1 = F1
		self.F2 = F2
		self.pow2bits = pow2bits
		self.pow2sig = pow2sig
		self.primes = primes
		self.unblind = unblind
		self.p2mask = p2mask
		self.p2sigmask = p2sigmask
		
	def encode_block(self, block):
		selection = self.data[block][self.sindex]
		Bp = (self.B0 * SUM[block] + self.B1 * D1[block] + self.B2 * D2[block]) & self.p2mask
		Bf = ((self.F0 * SUM[block] + self.F1 * D1[block] + self.F2 * D2[block]) >> self.pow2bits) & self.p2sigmask
		hashing_tools_instance = HashingTools()
		B = (hashing_tools_instance.sigpad(Bp) + Bf) & self.p2sigmask
		return B
	
	def decode_block(self, B, block):
		hashing_tools_instance = HashingTools()
		decode = (hashing_tools_instance.sigunpad(B) * (pow2 - n)) & self.p2mask
		decode = (decode * self.unblind) % self.primes
		if decode != self.data[block][self.sindex]:
			print("ERROR: Decode error. Stopping.")
			sys.exit()

#massive class for all the polynomical verification steps at the end. Likely candidate for breaking up later...
class PolynomialVerifier:
	def __init__(self, polycount, blockcount, storedB, POLYS):
		self.polycount = polycount
		self.blockcount = blockcount
		self.vcount = self.polycount // 2
		self.vpolys = []
		self.X = [1] * self.vcount
		self.Bacc = [0] * self.vcount
		self.storedB = storedB
		self.POLYS = POLYS

	def verify(self):
		self._choose_vpolys()
		self._compute_Bacc()
		self._check_final_accumulator()

	def _choose_vpolys(self):
		while len(self.vpolys) < self.vcount:
			match = False
			pindex = srand(self.polycount)
			for pi in self.vpolys:
				if pi == pindex:
					match = True
					break
			if match == False:
				self.vpolys.append(pindex)

	def _compute_Bacc(self):
		for block in range(self.blockcount):
			B = self.storedB[block]

			for pindex in range(self.vcount):
				P = self.POLYS[self.vpolys[pindex]]
				self.X[pindex] = (self.X[pindex] * P.xvalue) % P.modulus
				self.Bacc[pindex] = (self.Bacc[pindex] + B * self.X[pindex]) & p2sigmask

	def _check_final_accumulator(self):
		for pindex in range(self.vcount):
			hashing_tools_instance = HashingTools()
			P = self.POLYS[self.vpolys[pindex]]
			BtestC = (B0 * P.SUMPOLY + B1 * P.D1POLY + B2 * P.D2POLY)
			BtestF = ((F0 * P.SUMPOLY + F1 * P.D1POLY + F2 * P.D2POLY) >> pow2bits)
			Btest = (BtestC + hashing_tools_instance.sigunpad(BtestF)) & p2mask
			if Btest != hashing_tools_instance.sigunpad(self.Bacc[pindex]):
				print(Btest)
				print(sigunpad(self.Bacc[pindex]))
				print("ERROR: Polynomial checking has failed. Stopping.")
				sys.exit()
"""
def get_ranged_prime( hashint, roof ):
	hashint=hashint%roof
	while isprime(hashint)==False: hashint=(hashint+1)%roof
	return hashint

def sigpad(x):   return (x << sigbuffbits)	#create signature buffer space
def sigunpad(x): return (x >> sigbuffbits)	#remove signature buffer space
"""

class HashingTools:
	def get_ranged_prime(self, hashint, roof):
		hashint = hashint % roof
		while isprime(hashint) == False:
			hashint = (hashint + 1) % roof
		return hashint
	
	def sigpad(self, x):
		return (x << sigbuffbits) # create signature buffer space
	
	def sigunpad(self, x):
		return (x >> sigbuffbits) # remove signature buffer space

#
# fixed network variables
#

print("")
print("Sample 3 -",blockcount,"blocks... initializing request and preparing signatures...")

decodekeys				= "abc"					#decode keys for a waypoint of blocks
polycount				= 12					#coded for optimal bandwidth wasteage size
braid_element_count 			= 3
sigblockcount				= blockcount				#should be hard-coded to 3000 (see ot.spec.9)

#primebits				= minimum size of each primary prime	#already set
sigcoefficientmaxbits			= 12					#maximum number of bits for coefficients in signature polynomial
pow2bits 				= modulusbits				#top bit position for n ~= 2^(primebits*primecount)
kbits					= primebits 				#security level (in bits)

sigbuffbits				= int(ceil(log2(sigblockcount+braid_element_count)+sigcoefficientmaxbits))
sigbuffbits				= sigbuffbits+26			#1 in 2^26 chance of carry error 
pow2sigbits				= (pow2bits+sigbuffbits)

k					= 2**kbits
sigbuff					= 2**sigbuffbits
sigcoefficientmax			= 2**sigcoefficientmaxbits

pow2 					= 2**pow2bits
pow2sig	 				= 2**pow2sigbits

MASKH					= (pow2sig-1)-(kbits-1)

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

#build polynomial signature set up
POLYS = []
for pindex in range(0,polycount):
	hashing_tools_instance = HashingTools()
	m = hashing_tools_instance.get_ranged_prime( hash256sum(decodekeys,pindex), sigcoefficientmax )
	x = hash256sum(decodekeys,pindex,m) % m

	POLYS.append(PolyClass(m,x))
	P = POLYS[pindex]
	P.SUMPOLY = 0
	P.D1POLY  = 0
	P.D2POLY  = 0
	X = x

	for i in range(0,blockcount):
		P.SUMPOLY += X*SUM[i] #should not overflow pow2sig
		P.D1POLY  += X*D1[i]  #should not overflow pow2sig
		P.D2POLY  += X*D2[i]  #should not overflow pow2sig
		X = (X*x)%m

	if P.SUMPOLY>pow2sig:
		print("ERROR: SUMPOLY overflow. Stopping.")
		sys.exit()
	if P.D1POLY>pow2sig:
		print("ERROR: D1POLY overflow. Stopping.")
		sys.exit()
	if P.D2POLY>pow2sig:
		print("ERROR: D2POLY overflow. Stopping.")
		sys.exit()


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

#fast masks
p2mask    = pow2-1
p2sigmask = pow2sig-1

#precompute blindings and inverses
b    	 	= srand(coset)
blind	 	= powmodCRT(2,b,primelist)	#accelerated using CRT (using primelist over primes)
iblind	 	= inverseCRT(blind,primelist)	#accelerated using CRT (using primelist over primes)
i3 	 	= inverseCRT(3,primelist)	#accelerated using CRT (using primelist over primes)
unblind		= (iblind*i3)%primes

#select content to decode
sindex 		= srand(3)			
s1 		= 1
s2 		= 1
if sindex==1:   s1=-2
elif sindex==2: s2=-2

#fast version of modulus powers in prior sanitycheck
r0q		= srand(qspace)
r1q		= srand(qspace)
r2q		= srand(qspace)
r0qpow		= powmod(2,(b+coset*r0q)%(qprime-1),qprime)
r1qpow		= powmod(2,(b+coset*r1q)%(qprime-1),qprime)
r2qpow		= powmod(2,(b+coset*r2q)%(qprime-1),qprime)
b0	 	=  CRT([blind,r0qpow],[primes,qprime])
b1	 	= (CRT([blind,r1qpow],[primes,qprime])*s1) % n
b2	 	= (CRT([blind,r2qpow],[primes,qprime])*s2) % n

#prepare carry information
inn = inverse(-n,pow2)
B0  = (b0*inn)&p2mask			#B0 = ((b0)(-n)^-1) Mod pow2
B1  = (b1*inn)&p2mask			#B1 = ((b1)(-n)^-1) Mod pow2
B2  = (b2*inn)&p2mask			#B2 = ((b2)(-n)^-1) Mod pow2

F0  = ((b0 << pow2sigbits)//n) & MASKH	#F0 = floor( 0.(b0/n) * pow2sig ) & MASKH	;truncated binary representation of traditional divide
F1  = ((b1 << pow2sigbits)//n) & MASKH	#F1 = floor( 0.(b1/n) * pow2sig ) & MASKH	;truncated binary representation of traditional divide
F2  = ((b2 << pow2sigbits)//n) & MASKH	#F2 = floor( 0.(b2/n) * pow2sig ) & MASKH	;truncated binary representation of traditional divide

if sanitycheck:
	getcontext().prec = ceil(log10(2**pow2sigbits))+1
	B0test = (b0*inn)%pow2
	B1test = (b1*inn)%pow2
	B2test = (b2*inn)%pow2
	F0test = int(Decimal(b0)/Decimal(n) * pow2sig) & MASKH
	F1test = int(Decimal(b1)/Decimal(n) * pow2sig) & MASKH
	F2test = int(Decimal(b2)/Decimal(n) * pow2sig) & MASKH
	error=False
	if B0test!=B0: error=True;
	if B1test!=B1: error=True;
	if B2test!=B2: error=True;
	if F0test!=F0: error=True;
	if F1test!=F1: error=True;
	if F2test!=F2: error=True;
	if error: 
		print("Sample 3 - Sanity check failed. Stopping.")
		sys.exit()



#
# content fetching (encoding and decoding)
#

#>>> Encode/decode blocks

print("Sample 3 -",blockcount,"blocks... executing...")

block_coder = BlockCoder(DATA, sindex, B0, B1, B2, F0, F1, F2, pow2bits, pow2sig, primes, unblind, p2mask, p2sigmask)

storedB = []
sampleclock = timeit.default_timer()
for block in range(blockcount):
	B = block_coder.encode_block(block)
	storedB.append(B)
	block_coder.decode_block(B, block)

print("Sample 3 -",blockcount,"blocks... completed in",(timeit.default_timer()-sampleclock),"seconds")


#>>> Verify blocks (proxy and possibly recipient)
poly_verifier = PolynomialVerifier(polycount, blockcount, storedB,POLYS)
poly_verifier.verify()
print("Sample 3 -",blockcount,"verification... completed in",(timeit.default_timer()-sampleclock),"seconds")


"""
Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

This permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
"""
