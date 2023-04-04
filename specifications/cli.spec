Basic CLI invocation:

neopeer listnetwork
neopeer makenetwork [creds]
neopeer joinnetwork [creds]
neopeer modnetwork [creds]
neopeer removenetwork [creds]

neopeer liststorage[, netid]
neopeer addstorage netid, /localfolder, /network_folder [,options]
neopeer modstorage /localfolder [options]
neopeer removestorage ./localfolder[,permanently=False]

neopeer listpeers
neopeer invitepeers [options]
neopeer addpeer [options]
neopeer connectpeer peer_name, [options]
neopeer modpeer peer_name,[options]
neopeer removepeer peer_name

neopeer listnests
neopeer makenest nest_name[,options,peer_name,peer_name,....]
neopeer modnest nest_name[,options]
neopeer addnestpeer peer_name|peer_ip
neopeer removenestpeer peer_name|peer_ip
neopeer removenest nest_name

neopeer start [,options]
neopeer stop
neopeer ls netname@./file_or_folder_in_network
neopeer cp netname@./file_or_folder_in_network ./file_or_folder_locally [,options]
neopeer jobs [,options]

