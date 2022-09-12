* Installing and Running Valhalla

*nix versions of Valhalla clients and standalone servers.
The clients function identical to the win32 client.

Please run "valhalla_unix" from the root Valhalla directory (not from inside the "bin_unix"
directory) to launch these, or set the TESS_DATA variable at the top of the "valhalla_unix"
script to an absolute path to allow it to be run from any location. Note that the "valhalla_unix"
script is set up to write any files (saved maps, configuration files, etc.) into the user's home
directory at "~/.valhallaproject".

Clients will need the following dynamic link libraries present:
* libGL (OpenGL)
* SDL2 (>= 2.0.0)
* SDL2_image
* SDL2_mixer
* libpng
* libjpeg
* zlib

If native binaries for your platform are not included, then try the following:
1) Ensure you have the development version of the above libraries installed.
2) Type "make -C src install".
3) Re-run the "valhalla_unix" script if it succeeded.

The servers (bin_unix/linux_server or bin_unix/native_server) should need no libraries
other than libstdc++ and zlib. Note that for the server to see the "data/config/server-init.cfg",
it must be run from the root Valhalla directory. If you run a server with the
"valhalla_unix -d" command, this happens automatically. However, if you wish to
run the standalone servers instead, then you may need to write an appropriate wrapper
script to change to the appropriate data directory before running the standalone
server binary, as described below in the packaging guide.


* Packaging Guide for Unix-like Operating System Developers/Maintainers

If you are packaging Valhalla up for redistribution in a Linux distribution or other
similar OS, please avoid using the "valhalla_unix" script in its default/unmodified form.
You should at least set the TESS_DATA variable to appropriately find the common Valhalla
data files, or better yet replace it with a more appropriate way of starting Valhalla using
the script as a basis. If the distribution happens to place program binaries in a specific
directory separate from data files, such as "/usr/bin", then much of the lines within the script
that deal with finding an appropriate binary can be removed, as they only exist to help people
running from the original Valhalla tarball. An example run script is shown further
below.

Also, please note, that if your distribution chooses to package the binaries and game data
as separate packages due to whatever licensing rules or cross-architecture package sharing,
that client binaries from newer Valhalla editions are NOT compatible with older versions of
game data, on whole, nor obviously compatible with newer versions of game data. Game data, as a
whole, and client binaries are only roughly compatible within individual Valhalla editions,
though patches to individual Valhalla editions generally always retain compatibility with
the game data.

For those writing custom Valhalla run scripts for packaging, they should adhere to the following
guidelines:

Valhalla finds all game files relative to the current directory from which Valhalla is run,
so any script running Valhalla should set its current directory to where the Valhalla data
files are located before it runs the Valhalla client binaries. No silly symlinking tricks should
be at all necessary.

When running the Valhalla client, one command-line switch should always be supplied to
the client binary. This is "-u${HOME}/.Valhalla", which will instruct Valhalla to
write any user private files such as saved maps and configuration files to a private ".Valhalla"
directory within each user's home directory. Valhalla will automatically create this directory
and any subdirectories for each user running it, so do not pre-create this directory or install
any symlinks within it. All command-line switches supplied to the Valhalla run script should be
passed to the Valhalla client after the "-u${HOME}/.Valhalla" switch.

A simple script such as the following (with directory/file names set as appropriate) would
ultimately suffice for the client:

#!/bin/sh
TESS_DATA=/usr/share/games/valhalla
TESS_BIN=/usr/bin/tesseract_client
TESS_OPTIONS="-u${HOME}/.valhalla"

cd ${TESS_DATA}
exec ${TESS_BIN} ${TESS_OPTIONS} "$@"

A simple script for the server, which assumes a global default "data/config/server-init.cfg" in TESS_DATA,
but allows per-user overriding via the home directory, might be:

#!/bin/sh
TESS_DATA=/usr/share/games/valhalla
TESS_SERV_BIN=/usr/bin/tesseract_server
TESS_SERV_OPTIONS="-u${HOME}/.valhalla"

cd ${TESS_DATA}
exec ${TESS_SERV_BIN} ${TESS_SERV_OPTIONS} "$@"

With respect to libraries, make sure that you do not link Valhalla against any other ENet package
than the one that comes included with Valhalla, as it may be different from the official ENet
releases and might fail to compile or communicate properly.
