//
// mp::wavy::timer
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
#include <time.h>

namespace mp {
namespace wavy {


namespace {

class timer_task {
public:
	typedef core::timer_callback_t timer_callback_t;

	timer_task(core* c,
			const timespec* value,
			const timespec* interval,
			timer_callback_t callback) :
		m_interval(*interval), m_value(*value),
		m_core(c), m_callback(callback) { }

	void operator() ()
	{
		nanosleep(&m_value, NULL);
		while(!m_core->is_end()) {
			nanosleep(&m_interval, NULL);
			m_core->submit(m_callback);
		}
	}

private:
	const timespec m_interval;
	const timespec m_value;
	core* m_core;
	timer_callback_t m_callback;
	timer_task();
};

class timer_handler : public basic_handler {
public:
	typedef core::timer_callback_t timer_callback_t;

	timer_handler(int ident, timer_callback_t callback, bool periodic) :
		basic_handler(ident, this),
		m_periodic(periodic), m_callback(callback) { }

	bool operator() (const port_event* e)
	{
		port::get_timer(*e);
		m_callback();
		return m_periodic;
	}

private:
	bool m_periodic;
	timer_callback_t m_callback;
};

}  // noname namespace


void core::timer_thread(const timespec* value, const timespec* interval,
		timer_callback_t callback)
{
	//if(interval && interval->tv_sec == 0 && interval->tv_nsec == 0) {
	//	add_thread(1);
	//}
	timer_task t(this, value, interval, callback);
	submit(t);
}


void core::timer_event(const timespec* value, const timespec* interval,
		timer_callback_t callback)
{
	port& pt(ANON_impl->port_set());
	port::timer tm;

	int ident = pt.create_timer(&tm, value, interval);
	if(ident < 0) {
		throw system_error(errno, "failed to create timer");
	}

	shared_handler sh;
	if(interval && interval->tv_sec == 0 && interval->tv_nsec == 0) {
		sh = shared_handler(new timer_handler(ident, callback, true));
	} else {
		sh = shared_handler(new timer_handler(ident, callback, false));
	}
	ANON_impl->set_handler(sh);

	if(pt.add_timer(&tm) < 0) {
		ANON_impl->reset_handler(sh->ident());
		throw system_error(errno, "failed to run timer");
	}
}


}  // namespace wavy
}  // namespace mp

