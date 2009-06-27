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
#include "gate_control.h"
#include "proxy_client.h"
#include "wavy_core.h"
#include "exception.h"
#include <mp/endian.h>
#include <cclog/cclog.h>
#include <mp/stream_buffer.h>
#include <stdexcept>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <netinet/tcp.h>
#include <sys/uio.h>
#include <unistd.h>

#ifndef CONTROL_INITIAL_ALLOCATION_SIZE
#define CONTROL_INITIAL_ALLOCATION_SIZE (32*1024)
#endif

#ifndef CONTROL_RESERVE_SIZE
#define CONTROL_RESERVE_SIZE (4*1024)
#endif

namespace memxy {
namespace control {
namespace {


class handler : public core::handler {
public:
	handler(int fd);
	~handler();

public:
	void read_event();
	void process_body(char* data, size_t size);

public:
	mp::stream_buffer m_buffer;
	size_t m_off;
};

handler::handler(int fd) :
	core::handler(fd),
	m_buffer(CONTROL_INITIAL_ALLOCATION_SIZE),
	m_off(0)
{ }

handler::~handler() { }

void handler::read_event()
try {

	m_buffer.reserve_buffer(CONTROL_RESERVE_SIZE);

	ssize_t rl = ::read(fd(), m_buffer.buffer(), m_buffer.buffer_capacity());
	if(rl <= 0) {
		if(rl == 0) { throw connection_closed_error(); }
		if(errno == EAGAIN || errno == EINTR) { return; }
		else { throw connection_broken_error(); }
	}

	m_buffer.buffer_consumed(rl);

	while(m_buffer.data_size() > sizeof(uint32_t)) {
		uint32_t sz = ntohl(*(uint32_t*)m_buffer.data());

		char* const data = (char*)m_buffer.data() + sizeof(uint32_t);
		const size_t data_size = m_buffer.data_size() - sizeof(uint32_t);

		if(data_size < sz) { break; }

		process_body(data, sz);

		m_buffer.data_used(sizeof(uint32_t) + sz);
	}

} catch(connection_error& e) {
	LOG_DEBUG(e.what());
	throw;
} catch (std::exception& e) {
	LOG_DEBUG("memcached text protocol error: ",e.what());
	throw;
} catch (...) {
	LOG_DEBUG("memcached text protocol error: unknown error");
	throw;
}

void handler::process_body(char* data, size_t size)
{
	char* str = (char*)::malloc(size + 1);
	if(!str) { throw std::bad_alloc(); }

	memcpy(str, data, size);
	str[size] = '\0';

	try {
		LOG_INFO("set server list: ",str);
		proxy_client::set_servers(str);
	} catch(...) {
		::free(str);
		throw;
	}
	free(str);
}


void accepted(int fd, int err)
{
	if(fd < 0) {
		LOG_FATAL("accept failed: ",strerror(errno));
		service::end();
		return;
	}
#ifndef NO_TCP_NODELAY
	int on = 1;
	::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));  // ignore error
#endif
#ifndef NO_SO_LINGER
	struct linger opt = {0, 0};
	::setsockopt(fd, SOL_SOCKET, SO_LINGER, (void *)&opt, sizeof(opt));  // ignore error
#endif
	LOG_DEBUG("accept control gate fd=",fd);
	core::add_handler<control::handler>(fd);
}


}  // noname namespace
}  // namespace control


gate_control::gate_control() { }

gate_control::~gate_control() { }

void gate_control::listen(
		int socket_family, int socket_type, int protocol,
		const sockaddr* addr, socklen_t addrlen,
		int backlog)
{
	using namespace mp::placeholders;
	core::listen_event(
			socket_family, socket_type, protocol,
			addr, addrlen,
			&control::accepted,
			backlog);
}


}  // namespace memxy

