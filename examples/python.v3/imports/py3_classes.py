#includes
from imports.numbthy import *
from imports.modmath import *
from imports.utility import *
from imports.py3_classes import *

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
	def __init__(self, data, sindex, B0, B1, B2, F0, F1, F2, pow2bits, pow2sig, primes, unblind, p2mask, p2sigmask, SUM, D1, D2, sigbuffbits, n):
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
		self.pow2 = 2**pow2bits
		self.primes = primes
		self.unblind = unblind
		self.p2mask = p2mask
		self.p2sigmask = p2sigmask
		self.SUM = SUM
		self.D1 = D1
		self.D2 = D2
		self.sigbuffbits = sigbuffbits
		self.n = n
		
	def encode_block(self, block_index):
		selection = self.data[block_index][self.sindex.value]
		Bp = (self.B0 * self.SUM[block_index] + self.B1 * self.D1[block_index] + self.B2 * self.D2[block_index]) & self.p2mask
		Bf = ((self.F0 * self.SUM[block_index] + self.F1 * self.D1[block_index] + self.F2 * self.D2[block_index]) >> self.pow2bits) & self.p2sigmask
		hashing_tools_instance = HashingTools(self.sigbuffbits)
		B = (hashing_tools_instance.sigpad(Bp) + Bf) & self.p2sigmask
		return B
	
	def decode_block(self, B, block_index):
		hashing_tools_instance = HashingTools(self.sigbuffbits)
		decode = (hashing_tools_instance.sigunpad(B) * (self.pow2 - self.n)) & self.p2mask
		decode = (decode * self.unblind) % self.primes
		if decode != self.data[block_index][self.sindex.value]:
			print("ERROR: Decode error. Stopping.")
			sys.exit()
#this class can be broken into three. Just doing this for speed of getting things into classes at the moment.
class HashingTools:
	def __init__(self, sigbuffbits):
		self.sigbuffbits = sigbuffbits
		
	def get_ranged_prime(self, hashint, roof):
		hashint = hashint % roof
		while isprime(hashint) == False:
			hashint = (hashint + 1) % roof
		return hashint
	
	def sigpad(self, x):
		return (x << self.sigbuffbits) # create signature buffer space
	
	def sigunpad(self, x):
		return (x >> self.sigbuffbits) # remove signature buffer space