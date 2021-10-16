//----------------------------------------------------------------------------
//  System Networking Basics
//----------------------------------------------------------------------------
// 
//  Copyright (c) 1999-2011  The EDGE Team.
// 
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//----------------------------------------------------------------------------
//
//  FIXME acknowledge SDL_net
//  
//----------------------------------------------------------------------------

#ifndef __I_NETWORK_H__
#define __I_NETWORK_H__

/* Include normal headers */
#include <errno.h>

/* Include system network headers */
#ifdef WIN32
#include <winsock.h>
#else
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#endif

/* System-dependent definitions */
#ifndef WIN32
#define closesocket	 close
#define SOCKET	int
#define INVALID_SOCKET	-1
#define SOCKET_ERROR	-1
#endif

#ifdef WIN32
typedef int socklen_t;
#endif


class net_address_c
{
public:
	byte addr[4];

	int port;

public:
	net_address_c() : port(0)
	{
		addr[0] = addr[1] = addr[2] = addr[3] = 0;
	}

	net_address_c(const byte *_ip, int _pt = 0);
	net_address_c(const net_address_c& rhs);

	~net_address_c() { }

public:
	void FromSockAddr(const struct sockaddr_in *inaddr);

	void ToSockAddr(struct sockaddr_in *inaddr) const;

	const char *TempString(bool with_port = true) const;
	// returns a string representation of the address.
	// the result is a static buffer, hence is only valid
	// temporarily (until the next call).

	bool FromString(const char *str);
	// parse the dotted notation (##.##.##.##) with an optional
	// port number after a colon (':').  Returns false if the
	// string was not a valid address.

	void GuessBroadcast(void);
	// modify this address to produce (a guess of) the
	// broadcast address.
};


/* Variables */
 
extern bool nonet;

extern net_address_c n_local_addr;  // IP address of this machine
extern net_address_c n_broadcast_send;
extern net_address_c n_broadcast_listen;


/* Functions */

void I_StartupNetwork(void);
void I_ShutdownNetwork(void);

#ifdef LINUX  // TO BE REPLACED or REMOVED
const char * I_LocalIPAddrString(const char *eth_name);
// LINUX ONLY: determine IP address from an ethernet adaptor.
// The given string is "eth0" or "eth1".  Returns NULL if something
// went wrong.
#endif

void I_SetNonBlock (SOCKET sock, bool enable);
void I_SetNoDelay  (SOCKET sock, bool enable);
void I_SetBroadcast(SOCKET sock, bool enable);


#endif /* __I_NETWORK_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
