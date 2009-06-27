//
// mp::wavy::connect
//
// Copyright (C) 2008-2009 FURUHASHI Sadayuki
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

#include "wavy_core.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>

namespace mp {
namespace wavy {


namespace {

class connect_task {
public:
	typedef core::connect_callback_t connect_callback_t;

	struct pack {
		int        socket_family;
		int        socket_type;
		int        protocol;
		socklen_t  addrlen;
		int        timeout_msec;
		sockaddr   addr[0];
	};

	connect_task(
			int socket_family, int socket_type, int protocol,
			const sockaddr* addr, socklen_t addrlen,
			int timeout_msec, connect_callback_t& callback) :
		m((pack*)::malloc(sizeof(pack)+addrlen)),
		m_callback(callback)
	{
		if(!m) { throw std::bad_alloc(); }
		m->socket_family = socket_family;
		m->socket_type   = socket_type;
		m->protocol      = protocol;
		m->addrlen       = addrlen;
		m->timeout_msec  = timeout_msec;
		::memcpy(m->addr, addr, addrlen);
	}

	void operator() ()
	{
		int err = 0;
		int fd = ::socket(m->socket_family, m->socket_type, m->protocol);
		if(fd < 0) {
			err = errno;
			goto out;
		}

		if(::fcntl(fd, F_SETFL, O_NONBLOCK) < 0) {
			goto errno_error;
		}

		if(::connect(fd, m->addr, m->addrlen) >= 0) {
			// connect success
			goto out;
		}

		if(errno != EINPROGRESS) {
			goto errno_error;
		}

		while(true) {
			struct pollfd pf = {fd, POLLOUT, 0};
			int ret = ::poll(&pf, 1, m->timeout_msec);
			if(ret < 0) {
				if(errno == EINTR) { continue; }
				goto errno_error;
			}

			if(ret == 0) {
				errno = ETIMEDOUT;
				goto specific_error;
			}

			{
				int value = 0;
				int len = sizeof(value);
				if(::getsockopt(fd, SOL_SOCKET, SO_ERROR,
						&value, (socklen_t*)&len) < 0) {
					goto errno_error;
				}
				if(value != 0) {
					err = value;
					goto specific_error;
				}
				goto out;
			}
		}

	errno_error:
		err = errno;

	specific_error:
		::close(fd);
		fd = -1;

	out:
		::free(m);
		m_callback(fd, err);
	}

private:
	pack* m;
	connect_callback_t m_callback;

private:
	connect_task();
};

class connect_handler : public basic_handler {
public:
	typedef core::connect_callback_t connect_callback_t;

	connect_handler(int ident, connect_callback_t callback) :
		basic_handler(ident, this),
		m_done(false), m_callback(callback) { }

	bool operator() (const port_event* e)
	{
		int fd = ident();

		if(!__sync_bool_compare_and_swap(&m_done, false, true)) {
			::close(fd);
			return false;
		}

		int err = 0;

		int value = 0;
		int len = sizeof(value);

		if(::getsockopt(fd, SOL_SOCKET, SO_ERROR,
				&value, (socklen_t*)&len) < 0) {
			goto errno_error;
		}

		if(value != 0) {
			err = value;
			goto specific_error;
		}

		goto out;

	errno_error:
		err = errno;

	specific_error:
		::close(fd);
		fd = -1;

	out:
		m_callback(fd, err);
		return false;
	}

	void timeout()
	{
		if(!__sync_bool_compare_and_swap(&m_done, false, true)) {
			return;
		}

		m_callback(-1, ETIMEDOUT);
	}

private:
	bool m_done;
	connect_callback_t m_callback;
};

class connect_timeout_handler : public basic_handler {
public:
	connect_timeout_handler(int ident, shared_ptr<connect_handler> sh) :
		basic_handler(ident, this), m_sh(sh) { }

	bool operator() (const port_event* e)
	{
		m_sh->timeout();
		return false;
	}

private:
	shared_ptr<connect_handler> m_sh;
};

}  // noname namespace


void core::connect_thread(
		int socket_family, int socket_type, int protocol,
		const sockaddr* addr, socklen_t addrlen,
		int timeout_msec, connect_callback_t callback)
{
	connect_task t(
			socket_family, socket_type, protocol,
			addr, addrlen, timeout_msec, callback);
	submit(t);
}


void core::connect_event(
		int socket_family, int socket_type, int protocol,
		const sockaddr* addr, socklen_t addrlen,
		int timeout_msec, connect_callback_t callback)
{
	shared_ptr<connect_handler> sh;

	int err = 0;
	int fd = ::socket(socket_family, socket_type, protocol);
	if(fd < 0) {
		err = errno;
		goto out;
	}

	if(::connect(fd, addr, addrlen) >= 0) {
		// connect success
		goto out;
	}

	if(errno != EINPROGRESS) {
		goto errno_error;
	}

	try {
		sh = add_handler<connect_handler>(fd, callback);
	} catch (...) {
		goto errno_error;
	}

	// timeout
	try {
		struct timespec spec;
		spec.tv_sec = timeout_msec / 1000;
		spec.tv_nsec = timeout_msec % 1000 * 1000 * 1000;
	
		port& pt(ANON_impl->port_set());
		port::timer tm;

		int ident = pt.create_timer(&tm, &spec, NULL);
		if(ident < 0) {
			throw system_error(errno, "failed to create connect timeout timer");
		}
	
		ANON_impl->set_handler(
				shared_handler(new connect_timeout_handler(ident, sh)));

		if(pt.add_timer(&tm) < 0) {
			ANON_impl->reset_handler(ident);
			throw system_error(errno, "failed to run connect timeout timer");
		}

	} catch (...) {
		// FIXME
	}

	return;

errno_error:
	err = errno;

//specific_error:
	::close(fd);
	fd = -1;

out:
	submit(callback, fd, err);
}


}  // namespace wavy
}  // namespace mp

