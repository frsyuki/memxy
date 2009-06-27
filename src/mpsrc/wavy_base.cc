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

#include "wavy_core.h"
#include <sys/types.h>
#include <sys/resource.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#ifndef MP_WAVY_TASK_QUEUE_LIMIT
#define MP_WAVY_TASK_QUEUE_LIMIT 16
#endif

namespace mp {
namespace wavy {


namespace {


coreimpl::coreimpl() :
	m_off(0),
	m_num(0),
	m_pollable(true),
	m_end_flag(false)
{
	struct rlimit rbuf;
	if(::getrlimit(RLIMIT_NOFILE, &rbuf) < 0) {
		throw system_error(errno, "getrlimit() failed");
	}
	m_state = new shared_handler[rbuf.rlim_cur];

	// add out handler
	{
		m_out = shared_ptr<out>(new out);
		set_handler(m_out);
		port_set().add_port(m_out->get_port());
	}
}

coreimpl::~coreimpl()
{
	end();
	{
		pthread_scoped_lock lk(m_mutex);
		m_cond.broadcast();
	}
	for(workers_t::iterator it(m_workers.begin());
			it != m_workers.end(); ++it) {
		delete *it;
	}
	delete[] m_state;
}

void coreimpl::end()
{
	m_end_flag = true;
	{
		pthread_scoped_lock lk(m_mutex);
		m_cond.broadcast();
//		if(m_poll_thread) {  // FIXME signal_stop
//			pthread_kill(m_poll_thread, SIGALRM);
//		}
	}
}

bool coreimpl::is_end() const
{
	return m_end_flag;
}


void coreimpl::join()
{
	for(workers_t::iterator it(m_workers.begin());
			it != m_workers.end(); ++it) {
		(*it)->join();
	}
}

void coreimpl::detach()
{
	for(workers_t::iterator it(m_workers.begin());
			it != m_workers.end(); ++it) {
		(*it)->detach();
	}
}


namespace {
class thread_init {
private:
	struct init_pack {
		void (*func)(void*);
		void* user;
		volatile coreimpl* impl;
		volatile bool started;
		pthread_mutex start_mutex;
		pthread_cond  start_cond;
	};

	void started()
	{
		pthread_scoped_lock lk(m_init->start_mutex);
		m_init->started = true;
		m_init->start_cond.broadcast();
	}

public:
	thread_init() { }

	void set(coreimpl* impl, void (*func)(void*), void* user)
	{
		m_init.reset(new init_pack);
		m_init->impl = impl;
		m_init->func = func;
		m_init->user = user;
		m_init->started = false;
	}

	void operator() ()
	{
		if(m_init->func) try {
			(*m_init->func)(m_init->user);
		} catch (...) {
			started();
			throw;
		}
		coreimpl* impl = const_cast<coreimpl*>(m_init->impl);
		started();
		impl->operator()();
	}

	void wait_init()
	{
		{
			pthread_scoped_lock lk(m_init->start_mutex);
			while(!m_init->started) {
				m_init->start_cond.wait(m_init->start_mutex);
			}
		}
		m_init.reset();
	}

private:
	std::auto_ptr<init_pack> m_init;
};
}  // noname namespace

void coreimpl::add_thread(size_t num)
{
	for(size_t i=0; i < num; ++i) {
		m_workers.push_back(NULL);
		try {
			m_workers.back() = new pthread_thread(this);
		} catch (...) {
			m_workers.pop_back();
			throw;
		}
		m_workers.back()->run();
	}
}

void coreimpl::add_thread(size_t num, void (*init)(void*), void* user)
{
	thread_init inits[num];
	for(size_t i=0; i < num; ++i) {
		m_workers.push_back(NULL);
		try {
			inits[i].set(this, init, user);
			m_workers.back() = new pthread_thread(&inits[i]);
		} catch (...) {
			m_workers.pop_back();
			throw;
		}
		m_workers.back()->run();
	}
	for(size_t i=0; i < num; ++i) {
		inits[i].wait_init();
	}
}

void coreimpl::submit_impl(task_t& f)
{
	pthread_scoped_lock lk(m_mutex);
	m_task_queue.push(f);
	m_cond.signal();
}


shared_ptr<basic_handler> coreimpl::add_handler_impl(shared_ptr<basic_handler> sh)
{
	int fd = sh->ident();
	if(::fcntl(fd, F_SETFL, O_NONBLOCK) < 0) {
		throw system_error(errno, "failed to set nonblock flag");
	}

	set_handler(sh);
	port_set().add_fd(fd, EVPORT_READ);

	return sh;
}


void coreimpl::do_task(pthread_scoped_lock& lk)
{
	task_t ev = m_task_queue.front();
	m_task_queue.pop();
	if(!m_task_queue.empty()) { m_cond.signal(); }

	lk.unlock();

	try {
		ev();
	} catch (...) { }
}

void coreimpl::do_out(pthread_scoped_lock& lk)
{
	port::event e = m_out->next();

	lk.unlock();

	m_out->write_event(e);
}

void coreimpl::operator() ()
{
	retry:
	while(true) {
		pthread_scoped_lock lk(m_mutex);

		retry_task:
		if(m_end_flag) { return; }

		if(!m_pollable) {
			if(!m_out->empty()) {
				do_out(lk);
				goto retry;
			} else if(!m_task_queue.empty()) {
				do_task(lk);
				goto retry;
			} else {
				m_cond.wait(m_mutex);
				goto retry_task;
			}
		} else if(m_task_queue.size() > MP_WAVY_TASK_QUEUE_LIMIT) {
			do_task(lk);
			goto retry;
		}

		if(m_num == m_off) {
			m_pollable = false;
//m_poll_thread = pthread_self();  // FIXME signal_stop
			lk.unlock();

			retry_poll:
			int num = m_port.wait(&m_backlog, 1000);

			if(num <= 0) {
				if(num == 0 || errno == EINTR || errno == EAGAIN) {
					if(m_end_flag) {
						m_pollable = true;
						return;
					}
					goto retry_poll;
				} else {
					throw system_error(errno, "wavy core event failed");
				}
			}

			lk.relock(m_mutex);
			m_off = 0;
			m_num = num;

//m_poll_thread = 0;  // FIXME signal_stop
			m_pollable = true;
			m_cond.signal();
		}

		port::event e = m_backlog[m_off++];
		int ident = e.ident();

		if(ident == m_out->ident()) {
			m_out->poll_event();
			lk.unlock();

			m_port.shot_reactivate(e);

		} else {
			lk.unlock();

			bool cont;
			try {
				cont = (*m_state[ident])(static_cast<const port_event*>(&e));
			} catch (...) {
				cont = false;
			}

			if(!cont) {
				m_port.shot_remove(e);
				reset_handler(ident);
				goto retry;
			}

			m_port.shot_reactivate(e);
		}

	}  // while(true)
}


void coreimpl::step_next()
{
	pthread_scoped_lock lk(m_mutex);

	if(m_end_flag) { return; }

	if(!m_pollable) {
		if(!m_out->empty()) {
			do_out(lk);
		} else if(!m_task_queue.empty()) {
			do_task(lk);
		} else {
			m_cond.wait(m_mutex);
		}
		return;
	} else if(!m_task_queue.empty()) {
		do_task(lk);
		return;
	}

	if(m_num == m_off) {
		m_pollable = false;
		lk.unlock();

		int num = m_port.wait(&m_backlog, 1000);
		if(num <= 0) {
			if(num == 0 || errno == EINTR || errno == EAGAIN) {
				m_pollable = true;
				return;
			} else {
				throw system_error(errno, "wavy core event failed");
			}
		}

		lk.relock(m_mutex);
		m_off = 0;
		m_num = num;

		m_pollable = true;
		m_cond.signal();
	}

	port::event e = m_backlog[m_off++];
	int ident = e.ident();

	if(ident == m_out->ident()) {
		m_out->poll_event();
		lk.unlock();

		m_port.shot_reactivate(e);

	} else {
		lk.unlock();

		bool cont;
		try {
			cont = (*m_state[ident])(static_cast<const port_event*>(&e));
		} catch (...) {
			cont = false;
		}

		if(!cont) {
			m_port.shot_remove(e);
			reset_handler(ident);
			return;
		}

		m_port.shot_reactivate(e);
	}
}


}  // noname namespace


core::core() : m_impl(new coreimpl()) { }

core::~core() { delete ANON_impl; }

void core::end()
	{ ANON_impl->end(); }

bool core::is_end() const
	{ return ANON_impl->is_end(); }

void core::join()
	{ ANON_impl->join(); }

void core::detach()
	{ ANON_impl->detach(); }

void core::add_thread(size_t num)
	{ ANON_impl->add_thread(num); }

void core::add_thread(size_t num, void (*init)(void*), void* user)
	{ ANON_impl->add_thread(num, init, user); }

void core::submit_impl(task_t f)
	{ ANON_impl->submit_impl(f); }

core::shared_handler core::add_handler_impl(shared_handler newh)
	{ return ANON_impl->add_handler_impl(newh); }

void core::step_next()
	{ ANON_impl->step_next(); }


}  // namespace wavy
}  // namespace mp

