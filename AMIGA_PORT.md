X-Moto for AmigaOS 3.x
======================

X-Moto 0.6.3 (upstream https://github.com/xmoto/xmoto, master at commit
539984a84e8b1ad6f1ab7f90f92e5af9c9878e1c, "Merge pull request #213 from
xmoto/fix-msys2", 27 August 2025; GPL 2 or later), the 2D motocross
platform game, for AmigaOS 3.2 on a 68040 or 68060 with an FPU, on
OpenGPU's SDL 2 and OpenGL.

Status: lab-tested (Instance-45, an emulated 68040 with an FPU, 5 November
2026): the first start builds the level database, a new profile is made,
the tutorial level plays (the bike, the physics, the dirt), and the game
quits cleanly from the pause menu and from the main menu. The game's
clock ran at about a tenth of real time there. The first run leaves about
21 MB of Fast RAM in use (the OpenGPU modules stay loaded); a second run
gives back all but 80 bytes. Pictures: docs/screenshots/.


What you need
-------------

- AmigaOS 3.2 (or 3.2.3 with GFOpenUp), a 68040/68060 with an FPU.
- OpenGPU 0.6 or later, which OpenUp installs: opengpu.library and
  LIBS:OpenGPU/SDL2.module (version 3, for SDL_GL), SDL2_mixer.module and
  GL.module. X-Moto draws only with OpenGL: on AmigaChrome GL runs on the
  GPU (virgl); elsewhere it is Mesa's softpipe on the 68k, which will be
  very slow at 800x600.
- An RTG screen that SDL 2 can open a GL window on (the Workbench's, in a
  window, by default).
- Fast RAM: X-Moto keeps its textures, the level being played, the level
  database and SQLite's cache in memory; 64 MB or more is the safe side.
- About 70 MB of disk: the game's data comes with it.


Using it
--------

Open the XMoto icon, or run it from a Shell (it finds everything through
PROGDIR:, so from any directory). The game opens an 800x600 window on the
Workbench (upstream's default); Options can make it full screen. It keeps
its profiles, settings, the level database (xm.db), replays, its log and
the level cache in the userdata drawer beside the program, made the first
time if it is not there.

The first start builds the level database from the 1000-odd levels in
data/xmoto.bin: on a 68k that takes a while, with a progress bar.

Single player, local multiplayer on one keyboard, replays and ghosts are
all there. Not there (see "What was cut"): anything online.



Standard keys
-------------

Every AmiCradle game has the same keys on Right Amiga (GFOpenUp's
SDL2.module, after 0.8.26):

- Right Amiga + F: window or full screen, remembered for next time
- Right Amiga + P: pause (Right Amiga + P again goes on)
- Right Amiga + Q: quit
- Right Amiga + M: free the mouse pointer, or let the game take it again
- Right Amiga + S: a screenshot in the drawer's Screenshots
- Help: this game's keys (the drawer's Controls file)
- Right Amiga + F1, F2: F11, F12; Right Amiga + cursor keys: Page Up,
  Page Down, Home, End

The FULLSCREEN tooltype on the game's icon (take the brackets off) starts
it full screen.


How it is built: the route
--------------------------

X-Moto 0.6 has two renderers on paper: OpenGL and SDL_gfx. The SDL_gfx one
is SDL 1.2's (SDL_gfxPrimitives.h, SDL_rotozoom.h) and upstream's own
CMake says it cannot be built any more ("Can't disable yet"). The OpenGPU
kit has OpenGL (libGL.a, GL.module, a compatibility context through
SDL_GL), so the port uses upstream's OpenGL renderer unchanged: glad loads
GL 2.1 through SDL_GL_GetProcAddress, as on every other system. Sound,
input and the window are SDL 2's, unchanged.

- Built in from vendor/, as upstream does on Windows: ODE, libccd,
  chipmunk, Lua, bzip2, md5sum, glad and the XDG library (linked, unused).
- From the OpenGPU kit: libSDL2.a, libSDL2_mixer.a, libSDL2_ttf.a and
  libGL.a.
- Cross-built static libraries in /mnt/amigachrome/team2/deps/m68k-amigaos
  (sh amiga/deps/build-m68k-libs.sh zlib|png|jpeg|sqlite|xml2; copy
  amiga/deps/sqlite/*.c into the SQLite 3.46.1 amalgamation's folder first,
  one at a time, -j2): zlib 1.3.1, libpng 1.6.43, IJG jpeg 9f, SQLite
  3.46.1 and libxml2 2.12.9 (parser and tree only). SQLite has its own
  file system on dos.library (amiga_vfs.c: Open/Read/Write/Seek/
  SetFileSize, with Seek()'s old-position answer and SetFileSize()'s
  new-size answer taken as they are, zeros written where a file grows,
  SQLite's file locks kept in memory for all of the program's
  connections, temporary files in T:) and thread-safe mutexes on Exec
  signal semaphores (sqlite3_amiga.c), because X-Moto writes its
  statistics and counts level packs from SDL threads with their own
  connections. The first version, on libnix's POSIX calls with no locks,
  left xm.db as 24 bytes of garbage ("file is not a database").
- dist/SQLiteTest (amiga/tests/sqlite_test.c) tests that SQLite on its
  own: `SQLiteTest [DIR]` (T: by default) creates a database, inserts,
  reads back through a second connection, checks the header, a BUSY
  write, integrity_check, VACUUM and reopening; it ends PASS or FAIL.
- No GLU in the kit: amiga/amiga_glu.c has the two GLU calls X-Moto makes
  (gluScaleImage, nearest pixel; gluBuild2DMipmaps, box-filtered levels),
  amiga/include/GL/glu.h declares them.
- libstdc++ asks for POSIX threads: amiga/pthread_stubs.c (as the Seven
  Kingdoms and Aleph One ports).
- The toolchain's <pthread.h> turns read(), write() and friends into
  macros when <unistd.h> came first, and then <istream> does not compile:
  amiga/include/amiga_prelude.h includes it first in every C++ file (and
  <time.h>, which glibc brings in by itself and libnix does not).
- xmoto.bin, X-Moto's data package, is made by `xmoto --pack` on other
  systems, which needs a host build: amiga/tools/pack_xmoto_bin.py writes
  the same file (src/common/Packager.cpp's format) from bin/package.lst.
- amiga/stack.c: a 1 MB stack (ODE and chipmunk's solvers, Lua, the XML
  parser and SQLite all run on the game's task; libGL.a takes its own).
- amiga/amiga_screens.c, from the Seven Kingdoms port: SDL2.module leaves
  empty screens behind a full-screen window; after SDL_Quit() X-Moto
  closes the ones it left (or sends them behind the Workbench).

Build (GFOpenAmigaGCC 16.2.0-open4 or later; the defaults are CradleLab's
paths):

    make -f amiga/Makefile.amigaos -j2 \
         STOVE=$HOME/CradleLab/toolchain/openamigagcc-16.2.0-open4-x86_64-linux \
         NDK=$HOME/CradleLab/gfopensdk/OpenNDK/Include_H \
         SDL2KIT=/mnt/amigachrome/team2/deps/opengpu-sdk \
         DEPS=/mnt/amigachrome/team2/deps/m68k-amigaos

Output: dist/XMoto/ (XMoto, ReadMe, COPYING.txt, data/xmoto.bin,
data/Textures/Musics and Fonts, an empty userdata, an icon for each) and
dist/XMoto.info; build/amigaos/XMoto.debug is the unstripped program.
Flags: -noixemul -m68040 -m68881 -O2 -std=gnu++11 -fpermissive (as
upstream) and GCC 16's -fno-tree-loop-distribute-patterns. The program
and every object pass fpcr_check.py (GCC 16's float-to-int store).

Icons: amiga/icons/icons.json, from our own GlowIcon art (amiga/icons/art/xmoto.svg):

    python3 AMICRADLE/scripts/port_icons.py amiga/icons/icons.json --out amiga/icons


What was cut
------------

- curl: amiga/include/curl/curl.h stands in for it, and every transfer
  fails at once ("online features are not in the Amiga build"). Web
  levels, themes, highscores, rooms, votes, reports, replay downloads and
  uploads and the database sync are off, and off by default here
  (src/common/XMSession_default.h: no web, no web forms, no checking for
  levels or highscores at start-up, no web-configuration question at the
  first start).
- SDL_net: amiga/sdlnet_stub.c gives its calls without SDL_net,
  bsdsocket.library or a TCP/IP stack; SDLNet_Init() succeeds (X-Moto stops
  otherwise) and every socket fails to open, so there is no network play
  and no server (--server is also without its signal handlers).
- gettext: English only.
- The SDL_gfx renderer (see above).
- Reading /proc for the memory report (getMemoryInfo(): "No available
  information", as on Windows and macOS).
- Smaller defaults for a 68k: the frame rate is capped at 30 (upstream 50)
  and sound is 22050 Hz (upstream 44100); both are in Options.


Byte order
----------

Upstream already runs on big-endian machines: its files go through
XMFS::read/write*_LE and SwapEndian (replays, the level cache, xmoto.bin's
sizes, DBuffer), and the pixel masks follow SDL_BYTEORDER. Every raw read
outside src/common/VFileIO.cpp was checked: they are byte strings
(compressed replay data, Lua sources, Ogg data) or go through the _LE
calls.

One structure is kept raw: replays store SerializedBikeState as memory,
byte-swapped to little-endian, at the PC's layout (44 bytes, the floats
at 4-byte offsets). The 68k aligns a float to 2 bytes, which made it 42
bytes: every PC replay (credits.rpl, highscore replays) would have been
misread and overrun the state it is copied into. On AmigaOS its first
float is now aligned as on the PC, and static_asserts check its size and
offsets (src/xmscene/BasicSceneStructs.h).


Changes to upstream
-------------------

All under #if defined(__amigaos__) (the Makefile defines it):

- src/common/VFileIO.cpp: PROGDIR:data and PROGDIR:userdata (upstream's
  own MorphOS/AmigaOS 4 branch, plus making userdata), no XDG, no
  migration from $HOME, AmigaDOS names with a colon are absolute and
  "PROGDIR:x"'s parent is "PROGDIR:", getcwd() for PWD, libnix's strlwr().
- src/common/XMSession_default.h: the defaults above.
- src/xmscene/BasicSceneStructs.h: the replay state's layout.
- src/xmoto/Game.cpp, GameInit.cpp: amiga/amiga_screens.c's calls; no
  sigaction for --server.
- src/net/extSDL_net.cpp: no raw socket send.
- src/image/tim.cpp, tim_jpeg.cpp: libnix's stricmp().
- src/helpers/System.cpp: no /proc.
- vendor/glad/src/glad.c: no dlopen (GL is linked in; SDL loads it).


Known limits, and what a tester should check
--------------------------------------------

1. It starts: the GL window opens, the main menu draws with its textures
   and fonts, the menu music plays.
2. SQLiteTest PASSes (in T: and in the drawer: SQLiteTest PROGDIR:).
   The first start's level database build finishes (SQLite on the Amiga
   VFS, libxml2), and a second start is quick (xm.db and LCache reused).
3. A level plays: the bike moves, the physics (ODE/chipmunk) hold up,
   the frame rate (softpipe will be slow; AmigaChrome's virgl should not).
4. The credits replay and a saved replay play back (the replay layout
   fix); a ghost of your best time shows.
5. Quitting, from a window and from full screen, leaves no grey screen in
   front of the Workbench.
6. Threads: X-Moto writes its statistics after each death or finish and
   counts the level packs from SDL threads. SQLite is thread-safe here,
   but libstdc++ sees one thread (pthread_stubs.c): look for crashes right
   after dying or finishing, or on the level pack list.
7. Font rendering of the menus (SDL_ttf with DejaVu) and the Options
   screen's resolution list (SDL2.module's display modes).


Licence
-------

GPL 2 or later (COPYING.txt), the code and the data alike, as upstream
says. The port's own files in amiga/ are GPL 2 or later, like the game;
the curl and SDL_net stand-ins are not those libraries' code.
