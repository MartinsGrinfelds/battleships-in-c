# To better understand this Makefile:
# TARGET: DEPENDENCY <- Structure
# $< <- Takes dependency file names, this can be used as a variable in command
# $@ <- Takes target name, this can be used as a variable in command

# Do not associate target with file name
.PHONY: all run_client run_server clean
COMPILER=gcc
# Some flags for good coding practices
CFLAGS=-Wall -Wextra
# Automatically get the version from Git tags. Provide info if uncommited changes.
ifndef VERSION
  VERSION = $(shell git describe --tags --always --dirty)
else
  $(info VERSION is already defined: $(VERSION))
endif

SERVER_TARGET=BattleshipsServer
SERVER_DEPENDENCIES=testing.o
CLIENT_TARGET=BattleshipsClient
#  -lncurses -pthread
CLIENT_DEPENDENCIES=testing.o

all: $(SERVER_TARGET) $(CLIENT_TARGET)

%.o: %.c
	$(COMPILER) $(CFLAGS) -DBATTLESHIPS_VERSION=\"$(VERSION)\" -c $< -o $@

$(SERVER_TARGET): $(SERVER_DEPENDENCIES)
	echo "Compiling $@ Version: $(VERSION)"
	$(COMPILER) $(CFLAGS) $< -o $@

$(CLIENT_TARGET): $(CLIENT_DEPENDENCIES)
	echo "Compiling $@ Version: $(VERSION)"
	$(COMPILER) $(CFLAGS) $< -o $@

run_server: $(SERVER_TARGET)
	./$(SERVER_TARGET)

run_client: $(CLIENT_TARGET)
	./$(CLIENT_TARGET)

clean:
	rm -f *.o *.out
	rm -f $(SERVER_TARGET)
	rm -f $(CLIENT_TARGET)