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
class PolynomialVerifier:
	def __init__(self, polycount, blockcount, storedB, POLYS, p2sigmask, p2mask, sigbuffbits, B0, B1, B2, F0, F1, F2, pow2bits):
		self.polycount = polycount
		self.blockcount = blockcount
		self.vcount = self.polycount // 2
		self.vpolys = []
		self.X = [1] * self.vcount
		self.Bacc = [0] * self.vcount
		self.storedB = storedB
		self.POLYS = POLYS
		self.p2sigmask = p2sigmask
		self.p2mask= p2mask
		self.sigbuffbits = sigbuffbits
		self.B0 = B0
		self.B1 = B1
		self.B2 = B2
		self.F0 = F0
		self.F1 = F1
		self.F2 = F2
		self.pow2bits = pow2bits

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
				self.Bacc[pindex] = (self.Bacc[pindex] + B * self.X[pindex]) & self.p2sigmask

	def _check_final_accumulator(self):
		for pindex in range(self.vcount):
			hashing_tools_instance = HashingTools(self.sigbuffbits)
			P = self.POLYS[self.vpolys[pindex]]
			BtestC = (self.B0 * P.SUMPOLY + self.B1 * P.D1POLY + self.B2 * P.D2POLY)
			BtestF = ((self.F0 * P.SUMPOLY + self.F1 * P.D1POLY + self.F2 * P.D2POLY) >> self.pow2bits)
			Btest = (BtestC + hashing_tools_instance.sigunpad(BtestF)) & self.p2mask
			if Btest != hashing_tools_instance.sigunpad(self.Bacc[pindex]):
				print(Btest)
				print(sigunpad(self.Bacc[pindex]))
				print("ERROR: Polynomial checking has failed. Stopping.")
				sys.exit()