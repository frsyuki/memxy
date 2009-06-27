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

#ifndef MP_WAVY_PORT_KQUEUE_H__
#define MP_WAVY_PORT_KQUEUE_H__

#include "mp/exception.h"
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <sys/resource.h>

#ifndef MP_WAVY_PORT_KQUEUE_XVEC_MAX
#define MP_WAVY_PORT_KQUEUE_XVEC_MAX 256
#endif

namespace mp {
namespace wavy {


static const short EVPORT_READ  = EVFILT_READ;
static const short EVPORT_WRITE = EVFILT_WRITE;


class port {
public:
	port() : m_kq(kqueue())
	{
		struct rlimit rbuf;
		if(::getrlimit(RLIMIT_NOFILE, &rbuf) < 0) {
			::close(m_kq);
			throw system_error(errno, "getrlimit() failed");
		}
		m_fdmax = rbuf.rlim_cur;

		if(m_kq < 0) {
			throw system_error(errno, "failed to initialize kqueue");
		}

		memset(m_xvec, 0, sizeof(m_xvec));
		m_xindex = false;
	}

	~port()
	{
		::close(m_kq);
	}

	size_t max() const
	{
		return m_fdmax + MP_WAVY_PORT_KQUEUE_XVEC_MAX;
	}


	struct event {
	public:
		explicit event(struct kevent kev_) : kev(kev_) { }
		~event() { }

		int ident() const { return kev.ident; }

	private:
		struct kevent kev;
		friend class port;
	};


	int add_fd(int fd, short events)
	{
		struct kevent kev;
		EV_SET(&kev, fd, events, EV_ADD|EV_ONESHOT, 0, 0, NULL);
		return kevent(m_kq, &kev, 1, NULL, 0, NULL);
	}


	struct timer {
	public:
		timer() : ident(-1) { }
		~timer() { if(ident >= 0) free_xvec(xv); }
	private:
		int ident;
		unsigned long init;
		unsigned long interval;
		bool* xv;
		friend class port;
		timer(const timer&);
	};

	int create_timer(timer* tm, const timespec* value, const timespec* interval)
	{
		tm->ident = alloc_xvec_ident();
		if(tm->ident < 0) {
			return -1;
		}
		tm->xv = get_xvec(tm->ident);
		tm->init = value->tv_sec*1000 + value->tv_nsec/1000/1000;
		if(interval) {
			tm->interval = interval->tv_sec*1000 + interval->tv_nsec/1000/1000;
		} else {
			tm->interval = 0;
		}
		return tm->ident;
	}

	int add_timer(timer* tm)
	{
		struct kevent kev;
		EV_SET(&kev, tm->ident, EVFILT_TIMER, EV_ADD|EV_ONESHOT, 0,
				tm->init, (void*)tm->interval);
		if(kevent(m_kq, &kev, 1, NULL, 0, NULL) < 0) {
			return -1;
		}
		tm->ident = -1;
		return 0;
	}

	static int get_timer(event e)
	{
		return e.kev.data;
	}


	struct signal {
	public:
		signal() : ident(-1) { }
		~signal() { if(ident >= 0) free_xvec(xv); }
	private:
		int ident;
		int signo;
		bool* xv;
		friend class port;
		signal(const signal&);
	};

	int create_signal(signal* sg, int signo)
	{
		sg->ident = alloc_xvec_ident();
		if(sg->ident < 0) {
			return -1;
		}
		sg->xv = get_xvec(sg->ident);
		sg->signo = signo;
		return sg->ident;
	}

	int add_signal(signal* sg)
	{
		struct kevent kev;
		EV_SET(&kev, sg->ident, EVFILT_SIGNAL, EV_ADD|EV_ONESHOT, 0,
				sg->signo, (void*)sg->signo);
		if(kevent(m_kq, &kev, 1, NULL, 0, NULL) < 0) {
			return -1;
		}
		sg->ident = -1;
		return 0;
	}

	static int get_signal(event e)
	{
		return e.kev.data;
	}


	int add_port(port* pt)
	{
		if(add_fd(pt->m_kq, EVPORT_READ) < 0) {
			return -1;
		}
		return pt->m_kq;
	}


	int shot_reactivate(event e)
	{
		switch(e.kev.filter) {
		case EVFILT_READ:
			return add_fd(e.ident(), EVFILT_READ);

		case EVFILT_WRITE:
			return add_fd(e.ident(), EVFILT_WRITE);

		case EVFILT_TIMER: {
				timer tm;
				tm.ident = e.ident();
				tm.msec  = (unsigned long)e.kev.udata;
				return add_timer(&tm);
			}

		case EVFILT_SIGNAL: {
				signal sg;
				sg.ident  = e.ident();
				sg.signo = (int)e.kev.udata;
				return add_signal(&sg);
			}

		default:
			return -1;
		}
	}

	int shot_remove(event e)
	{
		switch(e.kev.filter) {
		case EVFILT_READ:
		case EVFILT_WRITE:
			return 0;

		case EVFILT_TIMER:
		case EVFILT_SIGNAL:
			return free_xvec( get_xvec(e.ident()) );

		default:
			return -1;
		}
	}


	int remove_fd(int fd, short events)
	{
		struct kevent kev;
		EV_SET(&kev, fd, events, EV_DELETE, 0, 0, NULL);
		return kevent(m_kq, &kev, 1, NULL, 0, NULL);
	}


	struct backlog {
		backlog()
		{
			buf = (struct kevent*)::calloc(
					sizeof(struct kevent),
					MP_WAVY_PORT_BACKLOG_SIZE);
			if(!buf) { throw std::bad_alloc(); }
		}

		~backlog()
		{
			::free(buf);
		}

		event operator[] (int n) const
		{
			return event(buf[n]);
		}

	private:
		struct kevent* buf;
		friend class port;
		backlog(const backlog&);
	};

	int wait(backlog* result)
	{
		return kevent(m_kq, NULL, 0, result->buf,
				MP_WAVY_PORT_BACKLOG_SIZE, NULL);
	}

	int wait(backlog* result, int timeout_msec)
	{
		struct timespec ts;
		ts.tv_sec  = timeout_msec / 1000;
		ts.tv_nsec = (timeout_msec % 1000) * 1000000;
		return kevent(m_kq, NULL, 0, result->buf,
				MP_WAVY_PORT_BACKLOG_SIZE, &ts);
	}


private:
	int alloc_xvec_ident()
	{
		for(unsigned int i=0; i < MP_WAVY_PORT_KQUEUE_XVEC_MAX*2; ++i) {
			unsigned int ident = __sync_fetch_and_add(&m_xindex, 1) % MP_WAVY_PORT_KQUEUE_XVEC_MAX;
			if(__sync_bool_compare_and_swap(&m_xvec[ident], false, true)) {
				return ident;
			}
		}
		errno = EMFILE;
		return -1;
	}

	bool* get_xvec(int ident)
	{
		return m_xvec + ident;
	}

	static int free_xvec(bool* xv)
	{
		if(*xv) {
			*xv = false;
			return true;
		}
		return false;
	}

	friend class timer;
	friend class signal;

private:
	int m_kq;
	size_t m_fdmax;

	bool m_xvec[MP_WAVY_PORT_KQUEUE_XVEC_MAX];
	unsigned int m_xindex;

	port(const port&);
};


}  // namespace wavy
}  // namespace mp

#endif /* wavy_port_kqueue.h */

