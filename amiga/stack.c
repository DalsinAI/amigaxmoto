/* A Shell gives 4 KB of stack; libnix's startup swaps to this much instead.
 * X-Moto is deep: ODE's and chipmunk's solvers, Lua level scripts, the
 * libxml2 parser and SQLite all run on the game's own task.
 * The AmiCradle Team. GPL 2 or later, like X-Moto. */
unsigned long __stack = 1024 * 1024;
static const char version[] __attribute__((used)) = "$VER: XMoto 0.6.3 Amiga (05.11.2026)";
