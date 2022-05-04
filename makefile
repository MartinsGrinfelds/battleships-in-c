client: Client/client.c
	gcc Client/client.c helpers/packet_types.c -o out/client
server: Server/server.c
	gcc Server/server.c -o out/Server