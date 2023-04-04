import numbthy

#modular inverse
def inverse(a, n, suppress=False):
	a %= n
	t = 0
	r = n
	newt = 1
	newr = a
	while newr != 0:
		quotient = r/newr
		t2=newt
		r2=newr
		newt=t-quotient*newt
		newr=r-quotient*newr
		t=t2
		r=r2
	if r > 1:
		if suppress==False:
			print "ERROR:",a," is not invertible"
		return -1
	if t < 0:
		t = t + n
	return t

#compute the gcd
def compute_gcd(x,y):
	while(y):
		x,y = y,x%y
	return x

#compute the lowest common multiple
def compute_lcm(x,y):
	if x==0:
		return y
	if y==0:
		return x
	lcm = (x*y)//compute_gcd(x,y)
	return lcm

#chinese remainder theorem
def CRT(nums,mods,suppress=False):
	k=len(nums)
	prod=1
	result=0
	for i in xrange(0,k):
		prod*=mods[i]
	for i in xrange(0,k):
		pp=prod/mods[i]
		inv = inverse(pp,mods[i],suppress)
		if inv<0:
			if suppress==False:
				print "ERROR: inversion routine failed for CRT."
			return -1
		result+=nums[i] * inv * pp;
	return(result % prod)

#fast multiplication of list of elements in list of primes
def fastmul( factors, primes ):
	r=[]
	for p in primes:
		result=1
		for f in factors:
			#result=(result*(f%p))%p
			result=(result*f)%p
		r.append(result)
	return CRT(r,primes)

#power for a prime list
def powmodCRT(g,e,primes):
	r=[]
	for p in primes:
		se=e%(p-1)
		r.append(numbthy.powmod(g,se,p))
	return CRT(r,primes)

#inverse for a prime list
def inverseCRT(v,primes):
	r=[]
	for p in primes:
		r.append(inverse(v,p))
	return CRT(r,primes)

