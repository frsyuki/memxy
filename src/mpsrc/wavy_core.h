//
// mp::wavy
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

#ifndef WAVY_CORE_H__
#define WAVY_CORE_H__

#include "mp/wavy/core.h"
#include "mp/pthread.h"
#include "wavy_port.h"

namespace mp {
namespace wavy {


struct port_event : port::event { };


class out;

namespace {


class coreimpl {
public:
	coreimpl();
	~coreimpl();

	typedef shared_ptr<basic_handler> shared_handler;
	typedef function<void ()> task_t;

public:
	void add_thread(size_t num);
	void add_thread(size_t num, void (*init)(void*), void* user);

	void end();
	bool is_end() const;

	void join();
	void detach();

	void step_next();


	void set_handler(shared_handler sh)
	{
		m_state[sh->ident()] = sh;
	}

	void reset_handler(int ident)
	{
		m_state[ident].reset();
	}

	port& port_set()
	{
		return m_port;
	}


	shared_handler add_handler_impl(shared_handler sh);

	void submit_impl(task_t& f);


	//class connect_thread;

	//class listen_handler;
	//void listen(int lsock, listen_callback_t callback);

	//class timer_thread;

public:
	void operator() ();
	inline void do_task(pthread_scoped_lock& lk);
	inline void do_out(pthread_scoped_lock& lk);

private:
	volatile size_t m_off;
	volatile size_t m_num;
	volatile bool m_pollable;
//	volatile pthread_t m_poll_thread;  // FIXME signal_stop

	port::backlog m_backlog;

	shared_handler* m_state;

	port m_port;

	pthread_mutex m_mutex;
	pthread_cond m_cond;

	typedef std::queue<task_t> task_queue_t;
	task_queue_t m_task_queue;

private:
	shared_ptr<out> m_out;
	friend class wavy::core;

private:
	typedef std::vector<pthread_thread*> workers_t;
	workers_t m_workers;

	volatile bool m_end_flag;

private:
	coreimpl(const coreimpl&);
};

#define ANON_impl static_cast<coreimpl*>(m_impl)


}  // noname namespace


struct port_mixin {
	port m_port;
};

class out : protected port_mixin, public basic_handler {
public:
	out();
	~out();

	typedef core::finalize_t finalize_t;
	typedef core::xfer xfer;

	inline void commit_raw(int fd, char* xfbuf, char* xfendp);

	// optimize
	inline void commit(int fd, xfer* xf);
	inline void write(int fd, const void* buf, size_t size);

public:
	port* get_port() { return &m_port; }

	bool operator() (const port_event* e)
	{
		throw std::logic_error("out::read_event is called");
	}

	void poll_event()
	{
		int num = m_port.wait(&m_backlog, 0);
		if(num <= 0) {
			if(num == 0 || errno == EINTR || errno == EAGAIN) {
				return;
			} else {
				throw system_error(errno, "wavy out event failed");
			}
		}

		for(int i=0; i < num; ++i) {
			m_queue.push(m_backlog[i]);
		}
	}

	bool empty() const
	{
		return m_queue.empty();
	}

	port::event next()
	{
		port::event e = m_queue.front();
		m_queue.pop();
		return e;
	}

	void write_event(port::event e);

private:
	std::queue<port::event> m_queue;
	port::backlog m_backlog;

	void watch(int fd);
	void* m_fdctx;

private:
	out(const out&);
};


}  // namespace wavy
}  // namespace mp

#endif /* wavy_core.h */

