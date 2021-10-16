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

#ifndef __N_RELIABLE_H__
#define __N_RELIABLE_H__

#include "i_net.h"

class net_node_c
{
public:
	SOCKET sock;

	net_address_c remote;

public:
	 net_node_c();
	~net_node_c();
};


bool N_StartupReliableLink(int port);
// (HOST ONLY)
// Create the link (socket) which allows clients to connect to us.
// Returns true if successful, otherwise false.

void N_ShutdownReliableLink(void);
// (HOST ONLY)
// close the reliable link and release all resources associated
// with it.  This will invalid any existing client connections
// (they should all be closed before calling this), and any
// buffered data will be discarded.

net_node_c * N_AcceptReliableConn(void);
// (HOST ONLY)
// Check to see if any clients have (requested to) connect, and
// allow them to connect.  This function should be called
// regularly to prevent the accept queue from overflowing.
//
// When successful, returns a new node.
// Returns NULL if not successful.

net_node_c * N_OpenReliableLink(const net_address_c *remote);
// (CLIENT ONLY)
// Open a link to the Host at the given address and port.
//
// When successful, returns a new node.
// Returns NULL if an error occurred.

void N_CloseReliableLink(net_node_c *node);
// Close the link to a Client or Host.

bool N_ReliableSend(net_node_c *node, const byte *data, int len);
// Send the data to the specified node.
// Returns true if successful, false on error.
// This call is non-blocking.

int N_ReliableRecv(net_node_c *node, byte *buffer, int max_len);
// Receive upto 'max_len' of data from the specified node.
// Returns the number of bytes read, 0 for none, or -1 if
// an error occurred.  This call is non-blocking.

// ===== NODE MANAGEMENT ===========

void N_ClearNodes(void);
void N_AddNode(net_node_c *nd);
void N_RemoveNode(net_node_c *nd);

#endif /* __N_RELIABLE_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
