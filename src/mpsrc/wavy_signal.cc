
//
// mp::wavy::signal
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
#include "mp/memory.h"
#include <signal.h>

namespace mp {
namespace wavy {


namespace {

class signal_task {
public:
	typedef core::signal_callback_t signal_callback_t;

	signal_task(core* c,
			const sigset_t* set,
			signal_callback_t callback) :
		m_sigmask(new scoped_sigprocmask(set)),
		m_core(c), m_callback(callback) { }

	void operator() ()
	{
		int signo;
		while(!m_core->is_end()) {
			if(sigwait(m_sigmask->get(), &signo) != 0) {
				throw system_error(errno, "sigwait failed");
			}
			if(!m_callback(signo)) {
				break;
			}
		}
	}

private:
	struct scoped_sigprocmask {
		scoped_sigprocmask(const sigset_t* set) :
			m_set(*set)
		{
			if(sigprocmask(SIG_BLOCK, set, NULL) < 0) {
				throw pthread_error(errno, "failed to set sigprocmask");
			}
		}

		~scoped_sigprocmask()
		{
			sigprocmask(SIG_UNBLOCK, &m_set, NULL);
		}

		const sigset_t* get() const
		{
			return &m_set;
		}

	private:
		sigset_t m_set;
		scoped_sigprocmask();
		scoped_sigprocmask(const scoped_sigprocmask&);
	};

	shared_ptr<scoped_sigprocmask> m_sigmask;
	core* m_core;
	signal_callback_t m_callback;
};

class signal_handler : public basic_handler {
public:
	typedef core::signal_callback_t signal_callback_t;

	signal_handler(int ident, int signo, signal_callback_t callback) :
		basic_handler(ident, this),
		m_signo(signo), m_callback(callback)
	{
		sigset_t mask;
		sigemptyset(&mask);
		sigaddset(&mask, m_signo);
		if(sigprocmask(SIG_BLOCK, &mask, NULL) < 0) {
			throw system_error(errno, "failed to sigprocmask");
		}
	}

	~signal_handler()
	{
		sigset_t mask;
		sigemptyset(&mask);
		sigaddset(&mask, m_signo);
		if(sigprocmask(SIG_UNBLOCK, &mask, NULL) < 0) {
			throw system_error(errno, "failed to sigprocmask");
		}
	}

	bool operator() (const port_event* e)
	{
		port::get_signal(*e);
		return m_callback(m_signo);
	}

private:
	int m_signo;
	signal_callback_t m_callback;
};

}  // noname namespace


void core::signal_thread(const sigset_t* set, signal_callback_t callback)
{
	signal_task t(this, set, callback);
	submit(t);
}

void core::signal_event(int signo, signal_callback_t callback)
{
	port& pt(ANON_impl->port_set());
	port::signal sg;

	int ident = pt.create_signal(&sg, signo);
	if(ident < 0) {
		throw system_error(errno, "failed to create signal");
	}

	shared_handler sh(new signal_handler(ident, signo, callback));
	ANON_impl->set_handler(sh);

	if(pt.add_signal(&sg) < 0) {
		ANON_impl->reset_handler(sh->ident());
		throw system_error(errno, "failed to run signal");
	}
}


}  // namespace wavy
}  // namespace mp

