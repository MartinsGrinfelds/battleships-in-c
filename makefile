.PHONY : executables
executables : Client_executable Server_executable
Client_executable: Client/client.c server_functions.o client_functions.o packet_utils.o
	gcc Client/client.c server_functions.o client_functions.o packet_utils.o -lncurses -o Client_executable
Server_executable: Server/server.c server_functions.o client_functions.o packet_utils.o
	gcc Server/server.c server_functions.o client_functions.o packet_utils.o -lncurses -o Server_executable
server_functions.o: Server/server_functions.c Server/server_functions.h helpers/packet_utils.c helpers/packet_utils.h helpers/packet_types.h
	gcc -c Server/server_functions.c helpers/packet_utils.c
client_functions.o: Client/client_functions.c Client/client_functions.h helpers/packet_utils.c helpers/packet_utils.h helpers/packet_types.h
	gcc -c Client/client_functions.c helpers/packet_utils.c
packet_utils.o: Server/server_functions.c Server/server_functions.h Client/client_functions.c Client/client_functions.h helpers/packet_utils.c helpers/packet_utils.h helpers/packet_types.h
	gcc -c Server/server_functions.c Client/client_functions.c helpers/packet_utils.c
.PHONY : clean
clean :
	rm -f *executable *.o