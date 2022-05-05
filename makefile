client: Client/client.c
	gcc Client/client.c helpers/packet_utils.c -o Client_executable
server: Server/server.c
	gcc Server/server.c helpers/packet_utils.c -o Server_executable