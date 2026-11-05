/*
 * No network play in the AmigaOS 3 build: SDL_net's calls, as X-Moto makes
 * them, without SDL_net, bsdsocket.library or a TCP/IP stack. SDLNet_Init()
 * succeeds (X-Moto stops at start-up otherwise); every socket fails to
 * open, with "network play is not in the Amiga build", so the game's own
 * error paths show it: no server is started, no client connects.
 * The AmiCradle Team. GPL 2 or later, like X-Moto.
 */
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <SDL.h>
#include <SDL_net.h>

static const char nonet[] = "network play is not in the Amiga build";

static void no_network(void) { SDL_SetError("%s", nonet); }

int SDLNet_Init(void) { return 0; }
void SDLNet_Quit(void) {}

void SDLNet_SetError(const char *fmt, ...)
{
	char buf[256];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	SDL_SetError("%s", buf);
}

const char *SDLNet_GetError(void) { return SDL_GetError(); }

int SDLNet_ResolveHost(IPaddress *address, const char *host, Uint16 port)
{
	(void)host;
	if (address) {
		address->host = INADDR_NONE;
		address->port = port;
	}
	no_network();
	return -1;
}

TCPsocket SDLNet_TCP_Open(IPaddress *ip) { (void)ip; no_network(); return NULL; }
TCPsocket SDLNet_TCP_Accept(TCPsocket server) { (void)server; return NULL; }
IPaddress *SDLNet_TCP_GetPeerAddress(TCPsocket sock) { (void)sock; no_network(); return NULL; }
int SDLNet_TCP_Send(TCPsocket sock, const void *data, int len) { (void)sock; (void)data; (void)len; no_network(); return -1; }
int SDLNet_TCP_Recv(TCPsocket sock, void *data, int maxlen) { (void)sock; (void)data; (void)maxlen; no_network(); return -1; }
void SDLNet_TCP_Close(TCPsocket sock) { (void)sock; }

/* Packets are plain memory, as SDL_net makes them. */
UDPpacket *SDLNet_AllocPacket(int size)
{
	UDPpacket *p = (UDPpacket *)SDL_calloc(1, sizeof(UDPpacket));
	if (!p)
		return NULL;
	p->data = (Uint8 *)SDL_malloc(size > 0 ? size : 1);
	if (!p->data) {
		SDL_free(p);
		return NULL;
	}
	p->maxlen = size;
	p->channel = -1;
	return p;
}

void SDLNet_FreePacket(UDPpacket *packet)
{
	if (packet) {
		SDL_free(packet->data);
		SDL_free(packet);
	}
}

UDPsocket SDLNet_UDP_Open(Uint16 port) { (void)port; no_network(); return NULL; }
int SDLNet_UDP_Send(UDPsocket sock, int channel, UDPpacket *packet) { (void)sock; (void)channel; (void)packet; return 0; }
int SDLNet_UDP_Recv(UDPsocket sock, UDPpacket *packet) { (void)sock; (void)packet; return 0; }
void SDLNet_UDP_Close(UDPsocket sock) { (void)sock; }

/* A set holds nothing: no socket ever opens. */
struct _SDLNet_SocketSet { int maxsockets; };

SDLNet_SocketSet SDLNet_AllocSocketSet(int maxsockets)
{
	SDLNet_SocketSet set = (SDLNet_SocketSet)SDL_calloc(1, sizeof(struct _SDLNet_SocketSet));
	if (set)
		set->maxsockets = maxsockets;
	return set;
}

void SDLNet_FreeSocketSet(SDLNet_SocketSet set) { SDL_free(set); }
int SDLNet_AddSocket(SDLNet_SocketSet set, SDLNet_GenericSocket sock) { (void)set; (void)sock; no_network(); return -1; }
int SDLNet_DelSocket(SDLNet_SocketSet set, SDLNet_GenericSocket sock) { (void)set; (void)sock; return 0; }
int SDLNet_CheckSockets(SDLNet_SocketSet set, Uint32 timeout)
{
	(void)set;
	if (timeout)
		SDL_Delay(timeout);
	return 0;
}
