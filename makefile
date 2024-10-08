# To better understand this Makefile:
# TARGET: DEPENDENCY <- Structure of tasks defined here.
# $< <- Takes first dependency (if seperated with space), this can be used as a variable in command.
# $^ <- Takes all dependancies (if seperated with space), this can be used as a variable in command.
# $@ <- Takes target name, this can be used as a variable in command.

# Do not associate target with file name.
.PHONY: all run_client run_server clean
COMPILER=gcc
# Some flags for good coding practices.
CFLAGS=-Wall -Wextra
# Automatically get the version from Git tags. Provide info if uncommited changes.
ifndef VERSION
  VERSION = $(shell git describe --tags --always --dirty)
else
  $(info VERSION is already defined: $(VERSION))
endif

SERVER_TARGET=BattleshipsServer
SERVER_DEPENDENCIES=server.o packets/connection.o graphical/text_formatter.o packets/shared_packets.o
CLIENT_TARGET=BattleshipsClient
#  -lm (math "libm" library)
CLIENT_DEPENDENCIES=client.o packets/connection.o graphical/text_formatter.o graphical/ui_functions.o packets/shared_packets.o graphical/map.o
CLIENT_EXT_DEPENDANCIES=external_libraries/raylib-5.0_linux_amd64/lib/libraylib.a -lm
# Other possibly required ones: -lraylib -lGL -lpthread -ldl -lrt -lX11


all: $(SERVER_TARGET) $(CLIENT_TARGET)

# Here double quotes within single quotes '"$(VERSION)"' are used because VScode doesn't show any warnings this way.
# Other working way is to use backslash \"$(VERSION)\" although it is working there was IDE warning within C code.
%.o: %.c
	$(COMPILER) $(CFLAGS) -DBATTLESHIPS_VERSION='"$(VERSION)"' -c $< -o $@

$(SERVER_TARGET): $(SERVER_DEPENDENCIES)
	$(info Compiling $@ Version: $(VERSION))
	$(COMPILER) $(CFLAGS) $^ -o $@

$(CLIENT_TARGET): $(CLIENT_DEPENDENCIES) $(CLIENT_EXT_DEPENDANCIES)
	$(info Compiling $@ Version: $(VERSION))
	$(COMPILER) $(CFLAGS) $^ -o $@

run_server: $(SERVER_TARGET)
	./$(SERVER_TARGET)

run_client: $(CLIENT_TARGET)
	./$(CLIENT_TARGET)

clean:
	rm -f *.o *.out
	rm -f $(SERVER_TARGET)
	rm -f $(CLIENT_TARGET)
	rm -f $(SERVER_DEPENDENCIES)
	rm -f $(CLIENT_DEPENDENCIES)