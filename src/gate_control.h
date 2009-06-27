//
// memxy::gate_control - memcached proxy
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
#ifndef MEMXY_GATE_CONTROL_H__
#define MEMXY_GATE_CONTROL_H__

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

namespace memxy {


class gate_control {
public:
	gate_control();
	~gate_control();

	void listen(
			int socket_family, int socket_type, int protocol,
			const sockaddr* addr, socklen_t addrlen,
			int backlog = 8);

private:
	gate_control(const gate_control&);
};


}  // namespace memxy

#endif /* gate_control.h */

