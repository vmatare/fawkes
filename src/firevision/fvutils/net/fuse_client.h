
/***************************************************************************
 *  fuse_client.h - FUSE network transport client
 *
 *  Created: Mon Jan 09 15:33:59 2006
 *  Copyright  2005-2007  Tim Niemueller [www.niemueller.de]
 *
 *  $Id$
 *
 ****************************************************************************/

/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __FIREVISION_FVUTILS_NET_FUSE_CLIENT_H_
#define __FIREVISION_FVUTILS_NET_FUSE_CLIENT_H_

#include <fvutils/net/fuse.h>
#include <core/threading/thread.h>
#include <sys/types.h>

class StreamSocket;
class WaitCondition;
class Mutex;
class FuseNetworkMessageQueue;
class FuseNetworkMessage;
class FuseClientHandler;

class FuseClient : public Thread {
 public:
  FuseClient(const char *hostname, unsigned short int port,
	     FuseClientHandler *handler);
  virtual ~FuseClient();

  void connect();
  void disconnect();

  void enqueue(FuseNetworkMessage *m);
  void enqueue(FUSE_message_type_t type, void *payload, size_t payload_size);
  void enqueue(FUSE_message_type_t type);
  void wait();

  virtual void loop();

 private:
  void send();
  void recv();
  void sleep();

  char *__hostname;
  unsigned short int __port;

  StreamSocket *__socket;
  unsigned int __wait_timeout;

  Mutex *__mutex;

  FuseNetworkMessageQueue *  __inbound_msgq;
  FuseNetworkMessageQueue *  __outbound_msgq;

  FuseClientHandler       *__handler;
  WaitCondition           *__waitcond;

  bool __alive;
};


#endif
