//----------------------------------------------------------------------------
//  EDGE Networking : Broadcast Links
//----------------------------------------------------------------------------
// 
//  Copyright (c) 1999-2009  The EDGE Team.
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
//  FIXME  acknowledge SDL_net code
//
//  FIXME  acknowledge SDLNetx code
//
//----------------------------------------------------------------------------

#if 0  // DISABLED

#include "i_defs.h"
#include "i_net.h"

#ifdef LINUX
#include <linux/if.h>
#include <linux/sockios.h>
#endif

#include "epi/endianess.h"

#include "n_bcast.h"


static SOCKET host_broadcast_sock = INVALID_SOCKET;

static int host_broadcast_port = -1;


bool N_StartupBroadcastLink(int port)
{
	if (nonet) ///!!!! || nobroadcast)
		return false;

	SYS_ASSERT(port > 0);

	host_broadcast_port = port;
	host_broadcast_sock = socket(PF_INET, SOCK_DGRAM, 0);

	if (host_broadcast_sock == INVALID_SOCKET)
	{
		I_Debugf("N_StartupBroadcastLink: couldn't create socket!\n");
		return false;
	}

	n_broadcast_send.port   = port;
	n_broadcast_listen.port = port;


	struct sockaddr_in sock_addr;

	n_broadcast_listen.ToSockAddr(&sock_addr);

	// bind the socket for listening
	if (bind(host_broadcast_sock, (struct sockaddr *)&sock_addr,
			 sizeof(sock_addr)) == SOCKET_ERROR)
	{
		I_Printf("N_StartupBroadcastLink: Couldn't bind to local port\n");
		N_ShutdownBroadcastLink();
		return false;
	}

	// get the channel host address
	net_address_c my_addr;

	my_addr.FromSockAddr(&sock_addr);
// I_Printf(">>> my_addr (bc) : %s\n", my_addr.TempString());

	I_SetBroadcast(host_broadcast_sock, 1);

	I_Printf("N_StartupBroadcastLink: OK\n");

	return true;
}

void N_ShutdownBroadcastLink(void)
{
	if (host_broadcast_sock != INVALID_SOCKET)
	{
		closesocket(host_broadcast_sock);

		host_broadcast_sock = INVALID_SOCKET;
		host_broadcast_port = -1;
	}
}



bool N_BroadcastSend(const byte *data, int len)
{
	struct sockaddr_in sock_addr;

	n_broadcast_send.ToSockAddr(&sock_addr);

	int actual = sendto(host_broadcast_sock,
			(const char *)data, len, 0,
			(struct sockaddr *)&sock_addr, sizeof(sock_addr));

	if (actual < 0) // error occurred
	{
		return false;
	}

	return true;
}


int N_BroadcastRecv(net_address_c *remote, byte *buffer, int max_len)
{
	struct sockaddr_in sock_addr;

	memset(&sock_addr, 0, sizeof(sock_addr));

	socklen_t len_var = sizeof(sock_addr);

	// clear global 'errno' var before the call
	errno = 0;
	
	int actual = recvfrom(host_broadcast_sock,
			(char *)buffer, max_len, 0 /* flags */,
			(struct sockaddr *)&sock_addr,
#ifdef USE_GUSI_SOCKETS
			(unsigned int *)&len_var);
#else
			&len_var);
#endif

	if (actual < 0) // error occurred
	{
		if (errno == EAGAIN)
			return 0;

		L_WriteDebug("N_BroadcastRecv: error %d\n", errno);
		return -1;
	}

	// set remote addr/port (output parameters)
	remote->FromSockAddr(&sock_addr);

	return actual; //OK
}

#endif  // DISABLED

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
