//----------------------------------------------------------------------------
//  EDGE Networking : Reliable Links
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
//----------------------------------------------------------------------------

// FIXME: EWOULDBLOCK may/may not be synonymous with EAGAIN

#include "i_defs.h"
#include "i_net.h"

#include <vector>

#include "n_reliable.h"
#include "n_bcast.h"


net_node_c::net_node_c() : sock(INVALID_SOCKET), remote()
{ }

net_node_c::~net_node_c()
{ }


#if 0  // DISABLED

//----------------------------------------------------------------------------

static SOCKET host_conn_sock = INVALID_SOCKET;

static int host_conn_port = -1;


bool N_StartupReliableLink(int port)
{
	if (nonet)
		return false;

	host_conn_port = port;
	host_conn_sock = socket(PF_INET, SOCK_STREAM, 0);

	if (host_conn_sock == INVALID_SOCKET)
	{
		L_WriteDebug("N_StartupReliableLink: couldn't create socket!\n");
		return false;
	}

	struct sockaddr_in sock_addr;

	memset(&sock_addr, 0, sizeof(sock_addr));

	sock_addr.sin_family = AF_INET;
	sock_addr.sin_addr.s_addr = INADDR_ANY;
	sock_addr.sin_port = htons(port);

/*
 * Windows gets bad mojo with SO_REUSEADDR:
 * http://www.devolution.com/pipermail/sdl/2005-September/070491.html
 *   --ryan.
 */
#ifndef WIN32
	// allow local address reuse
	{
		int enabled = 1;

		setsockopt(host_conn_sock, SOL_SOCKET, SO_REUSEADDR,
				   (char*)&enabled, sizeof(enabled));
	}
#endif

	// bind the socket for listening
	if (bind(host_conn_sock, (struct sockaddr *)&sock_addr,
			 sizeof(sock_addr)) == SOCKET_ERROR)
	{
		I_Printf("ReliableLink: Couldn't bind to local port!\n");
		N_ShutdownReliableLink();
		return false;
	}

	if (listen(host_conn_sock, 5) == SOCKET_ERROR)
	{
		I_Printf("ReliableLink: Couldn't listen to local port!\n");
		N_ShutdownReliableLink();
		return false;
	}

	// set the socket to non-blocking mode for accept()
	I_SetNonBlock(host_conn_sock, true);

	I_Printf("N_StartupReliableLink: OK\n");

	return true; //OK
}

void N_ShutdownReliableLink(void)
{
	if (host_conn_sock != INVALID_SOCKET)
	{
		closesocket(host_conn_sock);

		host_conn_sock = INVALID_SOCKET;
		host_conn_port = -1;
	}
}

net_node_c * N_AcceptReliableConn(void)
{
	net_node_c *node = new net_node_c();

	// accept a new TCP connection on a server socket
	struct sockaddr_in sock_addr;

	memset(&sock_addr, 0, sizeof(sock_addr));

	socklen_t len_var = sizeof(sock_addr);

	node->sock = accept(host_conn_sock, (struct sockaddr *)&sock_addr,
#ifdef USE_GUSI_SOCKETS
			(unsigned int *)&len_var);
#else
			&len_var);
#endif

	if (node->sock == INVALID_SOCKET)
	{
		I_Printf("N_AcceptReliableConn: failed");

		delete node;
		return NULL;
	}

	node->remote.FromSockAddr(&sock_addr);

	// we want non-blocking read/writes
	I_SetNonBlock(node->sock, true);

	// set the nodelay TCP option for real-time games
	I_SetNoDelay(node->sock, true);

	return node;
}

net_node_c * N_OpenReliableLink(const net_address_c *remote)
{
	net_node_c *node = new net_node_c();

	node->sock = socket(PF_INET, SOCK_STREAM, 0);

	if (node->sock == INVALID_SOCKET)
	{
		L_WriteDebug("N_OpenReliableLink: couldn't create socket!\n");

		delete node;
		return NULL;
	}

	struct sockaddr_in sock_addr;

	remote->ToSockAddr(&sock_addr);

	// connect to the remote host
	if (connect(node->sock, (struct sockaddr *)&sock_addr,
				sizeof(sock_addr)) == SOCKET_ERROR)
	{
		I_Printf("Couldn't connect to remote host!\n");
		N_CloseReliableLink(node);

		delete node;
		return NULL;
	}

	node->remote.FromSockAddr(&sock_addr);

	// we want non-blocking read/writes
	I_SetNonBlock(node->sock, true);

	// set the nodelay TCP option for real-time games
	I_SetNoDelay(node->sock, true);

	return NULL;
}

void N_CloseReliableLink(net_node_c *node)
{
	if (node->sock != INVALID_SOCKET)
	{
		closesocket(node->sock);

		node->sock = INVALID_SOCKET;
	}
}

bool N_ReliableSend(net_node_c *node, const byte *data, int len)
{
	SYS_ASSERT(len > 0);

	// keep sending data until it's sent or an error occurs
	while (len > 0)
	{
		// clear global 'errno' variable before the call
		errno = 0;

// SHIT: may block !!! FIXME
			
		int actual = send(node->sock, (const char*)data, len, 0);

		if (actual <= 0) // error occurred
		{
			if (errno == EINTR)
				continue;

			if (errno != EAGAIN)
				L_WriteDebug("N_ReliableSend: error %d\n", errno);

			return false;
		}

		SYS_ASSERT(actual <= len);

		len  -= actual;
		data += actual;
	}

	return true; //OK
}

int N_ReliableRecv(net_node_c *node, byte *buffer, int max_len)
{
	int actual;

	do
	{
		// clear global 'errno' variable before the call
		errno = 0;

		actual = recv(node->sock, (char*)buffer, max_len, 0);
	}
	while (errno == EINTR);

	if (actual < 0) // error occurred
	{
		if (errno != EAGAIN)
			L_WriteDebug("N_ReliableRecv: error %d\n", errno);

		return 0;
	}

	SYS_ASSERT(actual <= max_len);

	return actual;
}


//----------------------------------------------------------------------------
//  NODE MANAGEMENT
//----------------------------------------------------------------------------

std::vector<net_node_c *> net_nodes;

void N_ClearNodes(void)
{
	for (unsigned int i=0; i < net_nodes.size(); i++)
	{
		if (net_nodes[i])
		{
			delete net_nodes[i];
			net_nodes[i] = NULL;
		}
	}

	net_nodes.clear();
}

void N_AddNode(net_node_c *nd)
{
	net_nodes.push_back(nd);
}

void N_RemoveNode(net_node_c *nd)
{
	for (unsigned int i=0; i < net_nodes.size(); i++)
	{
		if (net_nodes[i] == nd)
		{
			net_nodes[i] = NULL;
			return;
		}
	}

	I_Error("INTERNAL ERROR N_RemoveNode: no such node '%p'\n", nd);
}

#endif  // DISABLED

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
