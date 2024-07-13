# Battleships
Multiplayer ship destruction game. Generalized game rules are within wiki page.

## Prerequisites
> [!NOTE]
> Code is designed to work on Linux system. It should be obvious to run it only on Linux or WSL.

### Ensure you have:
- `gcc` (for compiling)
- `make` (for using Makefile)
### If working on WSL ensure such libraries are installed:
- `libncurses-dev` (TBD if necessary)
- `build-essential` (TBD if necessary)

## To compile
You can use predefined `Makefile` instructions or compile any other way
if you know how to.
### There are several commands/targets implemented:
- `make` same as `make all`
- `make Battleships_Server`
- `make Battleships_Client`
- `make run_server`
- `make run_client`
- `make clean` <i>Good old clean if something is not working right</i>

> [!NOTE]
> All commands here will automatically do things necessary.
> That means run command will automatically do build action if necessary.

