Network routing can be handled in the following way:

	Aggregate bitmasks provided by peers of which braid sections they have at specific network depths
	Note: Peers may fake the depths for privacy. 
	e.g.
		If signatures are once every 250KiB we may want bitmasking to represent data sections
			which are 100x this figure.
		Given a 36TiB network this would be 36×1024×1024×1024÷3÷250=51539607 waypoint signatures
		So: 51539607÷8/100 = 64424 bitmask bytes per peer per depth

Initial peer connections can be handled via public STUN servers (possibly even advertising as torrents). 

Once a peer has known peers within a network, it's possible they could reestablish connections with each
	other inband. e.g. a network probe that sends a PoW ping to the network to see if a former peer
	is still connected with that peer being able to respond (in encrypted form) with its current
	connection information. 


