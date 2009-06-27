//
// memxy::gate_memtext - memcached proxy
//
// Copyright (C) 2009 FURUHASHI Sadayuki
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//
//    Unless required by applicable law or agreed to in writing, software
//    distributed under the License is distributed on an "AS IS" BASIS,
//    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//    See the License for the specific language governing permissions and
//    limitations under the License.
//
#include "gate_memtext_impl.h"

namespace memxy {
namespace memtext {


void send_error(int fd, int err)
{
	switch(err) {
	case MEMCACHED_NOTSTORED:
		send_static(fd, "NOT_STORED\r\n");
		break;
	case MEMCACHED_DELETED:
		send_static(fd, "DELETED\r\n");
		break;
	case MEMCACHED_NOTFOUND:
		send_static(fd, "NOT_FOUND\r\n");
		break;
	case MEMCACHED_CLIENT_ERROR:
		send_static(fd, "CLIENT_ERROR\r\n");
		break;
	case MEMCACHED_NO_SERVERS:
		send_static(fd, "SERVER_ERROR no server\r\n");
		break;
	case MEMCACHED_SERVER_ERROR:
		send_static(fd, "SERVER_ERROR\r\n");
		break;
	default:
		send_static(fd, "SERVER_ERROR unknown error\r\n");
		break;
	}
}


}  // namespace memtext
}  // namespace memxy

