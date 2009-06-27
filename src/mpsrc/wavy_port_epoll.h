//
// mp::wavy::port
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

#ifndef MP_WAVY_PORT_EPOLL_H__
#define MP_WAVY_PORT_EPOLL_H__

#include "mp/exception.h"
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/epoll.h>
#include <sys/resource.h>
#include <sys/timerfd.h>

// work around for glibc header signalfd.h error:expected initializer before ‘throw
#if __GLIBC__ < 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ <= 8)
#undef __THROW
#define __THROW
#endif
#include <sys/signalfd.h>

namespace mp {
namespace wavy {


static const short EVPORT_READ  = EPOLLIN;
static const short EVPORT_WRITE = EPOLLOUT;


class port {
public:
	port() : m_ep(epoll_create(MP_WAVY_PORT_BACKLOG_SIZE))
	{
		if(m_ep < 0) {
			throw system_error(errno, "failed to initialize epoll");
		}
	}

	~port()
	{
		::close(m_ep);
	}

	size_t max() const
	{
		struct rlimit rbuf;
		if(::getrlimit(RLIMIT_NOFILE, &rbuf) < 0) {
			throw system_error(errno, "getrlimit() failed");
		}
		return rbuf.rlim_cur;
	}


	struct event {
	public:
		explicit event(uint64_t data) : m_data(data) { }
		~event() { }

		int ident() const { return m_data & 0xffffffff; }

	private:
		uint64_t m_data;

		uint64_t data()   const { return m_data; }
		uint32_t events() const { return m_data >> 32; }
		friend class port;
	};


	int add_fd(int fd, short events)
	{
		struct epoll_event ev;
		::memset(&ev, 0, sizeof(ev));  // FIXME valgrind
		ev.events = events | EPOLLONESHOT;
		ev.data.u64 = ((uint64_t)fd) | ((uint64_t)ev.events << 32);
		return epoll_ctl(m_ep, EPOLL_CTL_ADD, fd, &ev);
	}


	struct timer {
	public:
		timer() : fd(-1) { }
		~timer() { if(fd >= 0) ::close(fd); }
	private:
		int fd;
		friend class port;
		timer(const timer&);
	};

	int create_timer(timer* tm, const timespec* value, const timespec* interval)
	{
		tm->fd = timerfd_create(CLOCK_REALTIME, 0);
		if(tm->fd < 0) {
			return -1;
		}

		if(::fcntl(tm->fd, F_SETFL, O_NONBLOCK) < 0) {
			return -1;
		}

		struct itimerspec itimer;
		::memset(&itimer, 0, sizeof(itimer));

		itimer.it_value = *value;
		if(interval) {
			itimer.it_interval = *interval;
		}

		if(timerfd_settime(tm->fd, 0, &itimer, NULL) < 0) {
			return -1;
		}

		return tm->fd;
	}

	int add_timer(timer* tm)
	{
		if(add_fd(tm->fd, EVPORT_READ) < 0) {
			return -1;
		}
		tm->fd = -1;
		return 0;
	}

	static int get_timer(event e)
	{
		uint64_t exp;
		if(read(e.ident(), &exp, sizeof(uint64_t)) <= 0) {
			return -1;
		}
		return exp & 0x7fffffff;
	}


	struct signal {
	public:
		signal() : fd(-1) { }
		~signal() { if(fd >= 0) ::close(fd); }
	private:
		int fd;
		friend class port;
		signal(const signal&);
	};

	int create_signal(signal* sg, int signo)
	{
		sigset_t mask;
		sigemptyset(&mask);
		sigaddset(&mask, signo);

		sg->fd = signalfd(-1, &mask, 0);
		if(sg->fd < 0) {
			return -1;
		}

		if(::fcntl(sg->fd, F_SETFL, O_NONBLOCK) < 0) {
			return -1;
		}

		return sg->fd;
	}

	int add_signal(signal* sg)
	{
		if(add_fd(sg->fd, EVPORT_READ) < 0) {
			return -1;
		}
		sg->fd = -1;
		return 0;
	}

	static int get_signal(event e)
	{
		signalfd_siginfo info;
		if(read(e.ident(), &info, sizeof(info)) <= 0) {
			return -1;
		}
		return 0;
	}


	int add_port(port* pt)
	{
		if(add_fd(pt->m_ep, EVPORT_READ) < 0) {
			return -1;
		}
		return pt->m_ep;
	}

	int ident() const
	{
		return m_ep;
	}


	int shot_reactivate(event e)
	{
		struct epoll_event ev;
		::memset(&ev, 0, sizeof(ev));  // FIXME valgrind
		ev.events = e.events();
		ev.data.u64 = e.data();
		return epoll_ctl(m_ep, EPOLL_CTL_MOD, e.ident(), &ev);
	}

	int shot_remove(event e)
	{
		return epoll_ctl(m_ep, EPOLL_CTL_DEL, e.ident(), NULL);
	}


	int remove_fd(int fd, short events)
	{
		return epoll_ctl(m_ep, EPOLL_CTL_DEL, fd, NULL);
	}


	struct backlog {
		backlog()
		{
			buf = (struct epoll_event*)::calloc(
					sizeof(struct epoll_event),
					MP_WAVY_PORT_BACKLOG_SIZE);
			if(!buf) { throw std::bad_alloc(); }
		}

		~backlog()
		{
			::free(buf);
		}

		event operator[] (int n)
		{
			return event(buf[n].data.u64);
		}

	private:
		struct epoll_event* buf;
		friend class port;
		backlog(const backlog&);
	};


	int wait(backlog* result)
	{
		return wait(result, -1);
	}

	int wait(backlog* result, int timeout_msec)
	{
		return epoll_wait(m_ep, result->buf,
				MP_WAVY_PORT_BACKLOG_SIZE, timeout_msec);
	}


private:
	int m_ep;

private:
	port(const port&);
};


}  // namespace wavy
}  // namespace mp

#endif /* wavy_port_epoll.h */

