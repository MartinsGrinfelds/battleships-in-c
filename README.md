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

## Adding features and creating a release
There is an automatic release creation system to ensure binary files and
changenotes availability for everyone and as fast as possible.
Developing a feature requires a unique branch that should be created
from issue or using your own preferred name.

> [!NOTE]
> After a feature is developed we should merge it to a branch that
> will be used for code testing. After then next release branch
> should be merged to main.

### Creating a release (for maintainers)
To create a release we should use git tag to mark a commit that will be used
as a release codebase. Make sure code in repo and locally is up to date.
```sh
# When you are at a commit code that you would like to use as a release
git tag v1.1.1
```
```sh
# If broken or wrong commit was tagged before we need to replace existing tag
git tag -f v1.1.1
```
```sh
# To push a tag to repository and trigger automatic build
git push origin tag v1.1.1
```

After check release draft within GitHub.

> [!IMPORTANT]
> Branch is not the same as tag.
