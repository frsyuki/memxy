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
#include "gate_memtext.h"
#include "gate_memtext_retrieval.h"
#include "gate_memtext_storage.h"
#include "gate_memtext_delete.h"
#include "wavy_core.h"
#include "exception.h"
#include "memproto/memtext.h"
#include <cclog/cclog.h>
#include <mp/pthread.h>
#include <mp/stream_buffer.h>
#include <stdexcept>
#include <string.h>
#include <errno.h>
#include <netinet/tcp.h>
#include <sys/uio.h>
#include <unistd.h>
#include <memory>

#ifndef MEMTEXT_INITIAL_ALLOCATION_SIZE
#define MEMTEXT_INITIAL_ALLOCATION_SIZE (32*1024)
#endif

#ifndef MEMTEXT_RESERVE_SIZE
#define MEMTEXT_RESERVE_SIZE (4*1024)
#endif


namespace memxy {
namespace memtext {
namespace {


class handler : public core::handler {
public:
	handler(int fd);
	~handler();

public:
	void read_event();

public:
	mp::stream_buffer m_buffer;
	memtext_parser m_parser;
	size_t m_off;
};


handler::handler(int fd) :
	core::handler(fd),
	m_buffer(MEMTEXT_INITIAL_ALLOCATION_SIZE),
	m_off(0)
{
	memtext_callback cb = {
		request_get,    // get
		request_gets,   // gets
		request_set,    // set
		NULL,           // add
		NULL,           // replace
		NULL,           // append
		NULL,           // prepend
		NULL,           // cas
		request_delete, // delete
		NULL,           // incr
		NULL,           // decr
	};

	memtext_init(&m_parser, &cb, (void*)(ptrdiff_t)fd);
}

handler::~handler() { }


void handler::read_event()
try {
	m_buffer.reserve_buffer(MEMTEXT_RESERVE_SIZE);

	ssize_t rl = ::read(fd(), m_buffer.buffer(), m_buffer.buffer_capacity());
	if(rl <= 0) {
		if(rl == 0) { throw connection_closed_error(); }
		if(errno == EAGAIN || errno == EINTR) { return; }
		else { throw connection_broken_error(); }
	}

	m_buffer.buffer_consumed(rl);

	do {
		int ret = memtext_execute(&m_parser,
				(char*)m_buffer.data(), m_buffer.data_size(), &m_off);
		if(ret < 0) {
			throw std::runtime_error("parse error");
		} else if(ret == 0) {
			return;
		}
		m_buffer.data_used(m_off);
		m_off = 0;
	} while(m_buffer.data_size() > 0);

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
	LOG_DEBUG("accept memtext gate fd=",fd);
	core::add_handler<memtext::handler>(fd);
}


}  // noname namespace
}  // namespace memtext


gate_memtext::gate_memtext() { }

gate_memtext::~gate_memtext() { }

void gate_memtext::listen(
		int socket_family, int socket_type, int protocol,
		const sockaddr* addr, socklen_t addrlen,
		int backlog)
{
	using namespace mp::placeholders;
	core::listen_event(
			socket_family, socket_type, protocol,
			addr, addrlen,
			&memtext::accepted,
			backlog);
}


}  // namespace memxy

