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
"""
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
			hashing_tools_instance = HashingTools(sigbuffbits)
			P = self.POLYS[self.vpolys[pindex]]
			BtestC = (B0 * P.SUMPOLY + B1 * P.D1POLY + B2 * P.D2POLY)
			BtestF = ((F0 * P.SUMPOLY + F1 * P.D1POLY + F2 * P.D2POLY) >> pow2bits)
			Btest = (BtestC + hashing_tools_instance.sigunpad(BtestF)) & p2mask
			if Btest != hashing_tools_instance.sigunpad(self.Bacc[pindex]):
				print(Btest)
				print(sigunpad(self.Bacc[pindex]))
				print("ERROR: Polynomial checking has failed. Stopping.")
				sys.exit()
#Another huge class this time for making POLYS
class PolynomialGenerator:
	def __init__(self, decodekeys, polycount, sigcoefficientmax, blockcount, SUM, D1, D2, pow2sig):
		self.decodekeys = decodekeys
		self.polycount = polycount
		self.sigcoefficientmax = sigcoefficientmax
		self.blockcount = blockcount
		self.SUM = SUM
		self.D1 = D1
		self.D2 = D2
		self.pow2sig = pow2sig
		
	def generate_polynomials(self):
		POLYS = []
		for pindex in range(self.polycount):
			hashing_tools_instance = HashingTools(sigbuffbits)
			m = hashing_tools_instance.get_ranged_prime(hash256sum(self.decodekeys,pindex), self.sigcoefficientmax)
			x = hash256sum(self.decodekeys,pindex,m) % m

			POLYS.append(PolyClass(m,x))
			P = POLYS[pindex]
			P.SUMPOLY = 0
			P.D1POLY  = 0
			P.D2POLY  = 0
			X = x

			for i in range(self.blockcount):
				P.SUMPOLY += X*self.SUM[i] #should not overflow pow2sig
				P.D1POLY  += X*self.D1[i]  #should not overflow pow2sig
				P.D2POLY  += X*self.D2[i]  #should not overflow pow2sig
				X = (X*x)%m

			if P.SUMPOLY > self.pow2sig:
				print("ERROR: SUMPOLY overflow. Stopping.")
				sys.exit()
			if P.D1POLY > self.pow2sig:
				print("ERROR: D1POLY overflow. Stopping.")
				sys.exit()
			if P.D2POLY > self.pow2sig:
				print("ERROR: D2POLY overflow. Stopping.")
				sys.exit()
		return POLYS
#
class PrepCarryInformation:
	def __init__(self, b0, b1, b2, n, pow2sigbits, pow2):
		self.b0 = b0
		self.b1 = b1
		self.b2 = b2
		self.n = n
		self.pow2sigbits = pow2sigbits
		self.pow2 = pow2
		self.inn = inverse(-n, pow2)
		self.p2mask = (pow2 - 1)
		self.pow2sig = 2 ** (pow2sigbits + 1)
		self.MASKH = (self.pow2sig - 1) - (self.n.bit_length() - 1)
		
		self.prepare_carry_information()
		self.run_sanity_check()

	def prepare_carry_information(self):
		self.B0 = (self.b0*self.inn)&self.p2mask
		self.B1 = (self.b1*self.inn)&self.p2mask
		self.B2 = (self.b2*self.inn)&self.p2mask

		self.F0 = ((self.b0 << self.pow2sigbits)//self.n) & self.MASKH
		self.F1 = ((self.b1 << self.pow2sigbits)//self.n) & self.MASKH
		self.F2 = ((self.b2 << self.pow2sigbits)//self.n) & self.MASKH

	def run_sanity_check(self):
		if not sanitycheck:
			return
		getcontext().prec = ceil(log10(2**self.pow2sigbits))+1
		B0test = (self.b0*self.inn)%self.pow2
		B1test = (self.b1*self.inn)%self.pow2
		B2test = (self.b2*self.inn)%self.pow2
		F0test = int(Decimal(self.b0)/Decimal(self.n) * self.pow2sig) & self.MASKH
		F1test = int(Decimal(self.b1)/Decimal(self.n) * self.pow2sig) & self.MASKH
		F2test = int(Decimal(self.b2)/Decimal(self.n) * self.pow2sig) & self.MASKH
		error = False
		if B0test != self.B0: 
			error = True
		if B1test != self.B1: 
			error = True
		if B2test != self.B2: 
			error = True
		if F0test != self.F0: 
			error = True
		if F1test != self.F1: 
			error = True
		if F2test != self.F2: 
			error = True
		if error: 
			print("Sample 3 - Sanity check failed. Stopping.")
			sys.exit()
#Terrible hacky class to move all this crypto code into 1 class
class Security:
	def __init__(self, primebits, sigcoefficientmaxbits, pow2bits, kbits):
		self.primebits = primebits
		self.sigcoefficientmaxbits = sigcoefficientmaxbits
		self.pow2bits = pow2bits
		self.kbits = self.primebits
		self.sigbuffbits = int(ceil(log2(sigblockcount+braid_element_count)+sigcoefficientmaxbits))
		self.sigbuffbits += 26
		self.sigbuff = 2**self.sigbuffbits
		self.sigcoefficientmax = 2**sigcoefficientmaxbits
		self.pow2 = 2**pow2bits
		self.pow2sig = 2**(pow2bits+self.sigbuffbits)
		self.MASKH = (self.pow2sig-1)-(self.kbits-1)
		self.pow2sigbits = (self.pow2bits+self.sigbuffbits)
#tiny class to replace the FastMask below.
class FastMasks:
	def __init__(self, pow2, pow2sig):
		self.p2mask = pow2 - 1
		self.p2sigmask = pow2sig - 1
"""
class BlindingsAndInverses:
	def __init__(self, coset, primelist,primes):
		self.b = srand(coset)
		self.blind = powmodCRT(2, self.b, primelist)    # accelerated using CRT (using primelist over primes)
		self.iblind = inverseCRT(self.blind, primelist)  # accelerated using CRT (using primelist over primes)
		self.i3 = inverseCRT(3, primelist)              # accelerated using CRT (using primelist over primes)
		self.unblind = (self.iblind * self.i3) % primes
class SIndex:
	def __init__(self, value):
		self.value = value
		if self.value == 1:
			self.s1 = -2
			self.s2 = 1
		elif self.value == 2:
			self.s1 = 1
			self.s2 = -2
		else:
			self.s1 = 1
			self.s2 = 1
class ModulusPower:
	def __init__(self, qspace, b, coset, qprime, blind, primes, s1, s2, n):
		self.qspace = qspace
		self.b = b
		self.coset = coset
		self.qprime = qprime
		self.blind = blind
		self.primes = primes
		self.s1 = s1
		self.s2 = s2
		self.n = n

	def generate_random_numbers(self):
		r0q = srand(self.qspace)
		r1q = srand(self.qspace)
		r2q = srand(self.qspace)
		r0qpow = powmod(2, (self.b + self.coset*r0q) % (self.qprime-1), self.qprime)
		r1qpow = powmod(2, (self.b + self.coset*r1q) % (self.qprime-1), self.qprime)
		r2qpow = powmod(2, (self.b + self.coset*r2q) % (self.qprime-1), self.qprime)
		b0 = CRT([self.blind, r0qpow], [self.primes, self.qprime])
		b1 = (CRT([self.blind, r1qpow], [self.primes, self.qprime]) * self.s1) % self.n
		b2 = (CRT([self.blind, r2qpow], [self.primes, self.qprime]) * self.s2) % self.n
		return b0, b1, b2


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
