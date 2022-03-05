from math import *
from random import *
from imports import numbthy
import hashlib

#utility routines
g_random = SystemRandom()

def log2(n): return log(n)/log(2)
def log10(n): return log(n)/log(10)
def srand(n): return g_random.randint(0, n-1)
def isprime(p): return numbthy.powmod(2,p,p)==2
def genprimes(ceilbits,primebits):
	primes=[]
	totalceilbits=0
	randceil=2**primebits
	while True:
		p = srand(randceil)
		while isprime(p)==False: p=p+1
		pbits = log2(p)
		if totalceilbits+pbits>ceilbits: break
		totalceilbits=totalceilbits+pbits
		primes.append(p)
	return primes
def arrayproduct(a,s=0,e=-1):
	r=1
	t=len(a)
	if s<0: s=0
	if e<0: e=t
	if e>t: e=t
	for x in xrange(s,e):
		r=r*a[x]
	return r
def hash256sumstr(*argp):
	strings=""
	for i in argp:
		strings="%s%s"%(strings,i)
	hashed_string = hashlib.sha256(strings.encode('utf-8')).hexdigest()
	return hashed_string
def hash256sum(*argp):
	return int( hash256sumstr(argp), 16 )

