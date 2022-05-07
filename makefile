.PHONY : executables
executables : Client_executable Server_executable
Client_executable: Client/client.c Client/client_functions.c helpers/packet_utils.c helpers/packet_types.h
	gcc Client/client.c Server/server_functions.c Client/client_functions.c helpers/packet_utils.c -lncurses -o Client_executable
Server_executable: Server/server.c helpers/packet_types.h
	gcc Server/server.c Server/server_functions.c Client/client_functions.c helpers/packet_utils.c -lncurses -o Server_executable
.PHONY : clean
clean :
	rm -f *executable