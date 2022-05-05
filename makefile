.PHONY : executables
executables : Client_executable Server_executable
Client_executable: Client/client.c
	gcc Client/client.c helpers/packet_utils.c -o Client_executable
Server_executable: Server/server.c
	gcc Server/server.c helpers/packet_utils.c -o Server_executable
.PHONY : clean
clean :
	rm -f *executable