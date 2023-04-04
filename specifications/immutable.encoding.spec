Immutable encoding spec:

	;where Ti, BiL, and BiR are one-time pad encrypted content pieces

	sum = (Ti+BiL+BiR)
	d1  = (Ti-BiL)
	d2  = (Ti-BiR)

	Negatives: 

		(Ti) could be upshifted to prevent negatives where needed

