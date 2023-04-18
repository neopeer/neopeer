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
#Another huge class this time for making POLYS
class PolynomialGenerator:
	def __init__(self, decodekeys, polycount, sigcoefficientmax, blockcount, SUM, D1, D2, pow2sig, sigbuffbits):
		self.decodekeys = decodekeys
		self.polycount = polycount
		self.sigcoefficientmax = sigcoefficientmax
		self.blockcount = blockcount
		self.SUM = SUM
		self.D1 = D1
		self.D2 = D2
		self.pow2sig = pow2sig
		self.sigbuffbits = sigbuffbits
		
	def generate_polynomials(self):
		POLYS = []
		for pindex in range(self.polycount):
			hashing_tools_instance = HashingTools(self.sigbuffbits)
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

	def prepare_carry_information(self):
		self.B0 = (self.b0*self.inn)&self.p2mask
		self.B1 = (self.b1*self.inn)&self.p2mask
		self.B2 = (self.b2*self.inn)&self.p2mask

		self.F0 = ((self.b0 << self.pow2sigbits)//self.n) & self.MASKH
		self.F1 = ((self.b1 << self.pow2sigbits)//self.n) & self.MASKH
		self.F2 = ((self.b2 << self.pow2sigbits)//self.n) & self.MASKH

	def run_sanity_check(self, sanitycheck):
		self.sanitycheck = sanitycheck
		if not self.sanitycheck:
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
	def __init__(self, primebits, sigblockcount, braid_element_count, sigcoefficientmaxbits, pow2bits, kbits):
		self.primebits = primebits
		self.braid_element_count = braid_element_count
		self.sigblockcount = sigblockcount
		self.sigcoefficientmaxbits = sigcoefficientmaxbits
		self.pow2bits = pow2bits
		self.kbits = self.primebits
		self.sigbuffbits = int(ceil(log2(self.sigblockcount+self.braid_element_count)+self.sigcoefficientmaxbits))
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