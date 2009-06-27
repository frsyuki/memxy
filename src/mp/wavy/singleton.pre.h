//
// mp::wavy::singleton
//
// Copyright (C) 2008 FURUHASHI Sadayuki
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

#ifndef MP_WAVY_SINGLETON_H__
#define MP_WAVY_SINGLETON_H__

#include "mp/wavy/core.h"

namespace mp {
namespace wavy {


template <typename Instance>
struct singleton {

	typedef wavy::basic_handler basic_handler;
	typedef wavy::handler handler;

	static void init();

	static void step_next();

	static void add_thread(size_t num);
	static void add_thread(size_t num, void (*init)(void*), void* user);

	static void end();
	static bool is_end();

	static void join();
	static void detach();


	typedef core::finalize_t finalize_t;


	static void write(int fd, const void* buf, size_t size);


	static void write(int fd, const void* buf, size_t size,
			finalize_t fin, void* user);

	template <typename T>
	static void write(int fd, const void* buf, size_t size,
			std::auto_ptr<T>& fin);

	template <typename T>
	static void write(int fd, const void* buf, size_t size,
			mp::shared_ptr<T> fin);

	
	static void writev(int fd, const struct iovec* vec, size_t veclen,
			finalize_t fin, void* user);
	
	template <typename T>
	static void writev(int fd, const struct iovec* vec, size_t veclen,
			std::auto_ptr<T>& fin);

	template <typename T>
	static void writev(int fd, const struct iovec* vec, size_t veclen,
			mp::shared_ptr<T> fin);


	static void sendfile(int fd, int infd, uint64_t off, size_t size,
			finalize_t fin, void* user);
	
	static void hsendfile(int fd,
			const void* header, size_t header_size,
			int infd, uint64_t off, size_t size,
			finalize_t fin, void* user);
	
	static void hvsendfile(int fd,
			const struct iovec* header_vec, size_t header_veclen,
			int infd, uint64_t off, size_t size,
			finalize_t fin, void* user);

	typedef core::xfer xfer;

	static void commit(int fd, xfer* xf);


	template <typename Handler>
	static shared_ptr<Handler> add_handler();
MP_ARGS_BEGIN
	template <typename Handler, MP_ARGS_TEMPLATE>
	static shared_ptr<Handler> add_handler(MP_ARGS_PARAMS);
MP_ARGS_END

	template <typename F>
	static void submit(F f);
MP_ARGS_BEGIN
	template <typename F, MP_ARGS_TEMPLATE>
	static void submit(F f, MP_ARGS_PARAMS);
MP_ARGS_END


	typedef core::connect_callback_t connect_callback_t;

	static void connect_thread(int socket_family, int socket_type, int protocol,
			const sockaddr* addr, socklen_t addrlen,
			int timeout_msec, connect_callback_t callback);

	static void connect_event(int socket_family, int socket_type, int protocol,
			const sockaddr* addr, socklen_t addrlen,
			int timeout_msec, connect_callback_t callback);


	typedef core::listen_callback_t listen_callback_t;

	static int listen_event(int socket_family, int socket_type, int protocol,
			const sockaddr* addr, socklen_t addrlen,
			listen_callback_t callback,
			int backlog = 1024);


	typedef core::timer_callback_t timer_callback_t;

	static void timer_thread(const timespec* value, const timespec* interval,
			timer_callback_t callback);

	static void timer_event(const timespec* value, const timespec* interval,
			timer_callback_t callback);


	typedef core::signal_callback_t signal_callback_t;

	static void signal_thread(const sigset_t* set, signal_callback_t callback);

	static void signal_event(int signo, signal_callback_t callback);

private:
	static core* s_core;

	singleton();
};

template <typename Instance>
core* singleton<Instance>::s_core;

template <typename Instance>
inline void singleton<Instance>::init()
{
	s_core = new core();
}

template <typename Instance>
inline void singleton<Instance>::step_next()
	{ s_core->step_next(); }

template <typename Instance>
inline void singleton<Instance>::add_thread(size_t num)
	{ s_core->add_thread(num); }

template <typename Instance>
inline void singleton<Instance>::add_thread(size_t num, void (*init)(void*), void* user)
	{ s_core->add_thread(num, init, user); }

template <typename Instance>
inline void singleton<Instance>::end()
{
	s_core->end();
}

template <typename Instance>
bool singleton<Instance>::is_end()
{
	return s_core->is_end();
}

template <typename Instance>
inline void singleton<Instance>::join()
{
	s_core->join();
}

template <typename Instance>
inline void singleton<Instance>::detach()
{
	s_core->detach();
}

template <typename Instance>
inline void singleton<Instance>::write(int fd, const void* buf, size_t size)
	{ s_core->write(fd, buf, size); }

template <typename Instance>
inline void singleton<Instance>::write(int fd, const void* buf, size_t size,
		finalize_t fin, void* user)
	{ s_core->write(fd, buf, size, fin, user); }

template <typename Instance>
template <typename T>
inline void singleton<Instance>::write(int fd, const void* buf, size_t size,
		std::auto_ptr<T>& fin)
	{ s_core->write(fd, buf, fin); }

template <typename Instance>
template <typename T>
inline void singleton<Instance>::write(int fd, const void* buf, size_t size,
		mp::shared_ptr<T> fin)
	{ s_core->write(fd, buf, fin); }

template <typename Instance>
inline void singleton<Instance>::writev(int fd, const struct iovec* vec, size_t veclen,
		finalize_t fin, void* user)
	{ s_core->writev(fd, vec, veclen, fin, user); }

template <typename Instance>
template <typename T>
inline void singleton<Instance>::writev(int fd, const struct iovec* vec, size_t veclen,
		std::auto_ptr<T>& fin)
	{ s_core->writev(fd, vec, veclen, fin); }

template <typename Instance>
template <typename T>
inline void singleton<Instance>::writev(int fd, const struct iovec* vec, size_t veclen,
		mp::shared_ptr<T> fin)
	{ s_core->writev(fd, vec, veclen, fin); }

template <typename Instance>
inline void singleton<Instance>::sendfile(int fd, int infd, uint64_t off, size_t size,
		finalize_t fin, void* user)
	{ s_core->sendfile(fd, infd, off, size, fin, user); }

template <typename Instance>
inline void singleton<Instance>::hsendfile(int fd,
		const void* header, size_t header_size,
		int infd, uint64_t off, size_t size,
		finalize_t fin, void* user)
	{ s_core->hsendfile(fd, header, header_size, infd, off, size, fin, user); }

template <typename Instance>
inline void singleton<Instance>::hvsendfile(int fd,
		const struct iovec* header_vec, size_t header_veclen,
		int infd, uint64_t off, size_t size,
		finalize_t fin, void* user)
	{ s_core->hsendfile(fd, header_vec, header_veclen, infd, off, size, fin, user); }

template <typename Instance>
inline void singleton<Instance>::commit(int fd, xfer* xf)
	{ s_core->commit(fd, xf); }


template <typename Instance>
template <typename Handler>
inline shared_ptr<Handler> singleton<Instance>::add_handler()
	{ return s_core->add_handler<Handler>(); }
MP_ARGS_BEGIN
template <typename Instance>
template <typename Handler, MP_ARGS_TEMPLATE>
inline shared_ptr<Handler> singleton<Instance>::add_handler(MP_ARGS_PARAMS)
	{ return s_core->add_handler<Handler, MP_ARGS_TYPES>(MP_ARGS_FUNC); }
MP_ARGS_END

template <typename Instance>
template <typename F>
inline void singleton<Instance>::submit(F f)
	{ s_core->submit<F>(f); }
MP_ARGS_BEGIN
template <typename Instance>
template <typename F, MP_ARGS_TEMPLATE>
inline void singleton<Instance>::submit(F f, MP_ARGS_PARAMS)
	{ s_core->submit<F, MP_ARGS_TYPES>(f, MP_ARGS_FUNC); }
MP_ARGS_END


template <typename Instance>
inline void singleton<Instance>::connect_thread(
			int socket_family, int socket_type, int protocol,
			const sockaddr* addr, socklen_t addrlen,
			int timeout_msec, connect_callback_t callback)
	{ s_core->connect_thread(socket_family, socket_type, protocol,
			addr, addrlen, timeout_msec, callback); }

template <typename Instance>
inline void singleton<Instance>::connect_event(
			int socket_family, int socket_type, int protocol,
			const sockaddr* addr, socklen_t addrlen,
			int timeout_msec, connect_callback_t callback)
	{ s_core->connect_event(socket_family, socket_type, protocol,
			addr, addrlen, timeout_msec, callback); }


template <typename Instance>
inline int singleton<Instance>::listen_event(
			int socket_family, int socket_type, int protocol,
			const sockaddr* addr, socklen_t addrlen,
			listen_callback_t callback,
			int backlog)
	{ return s_core->listen_event(socket_family, socket_type, protocol,
			addr, addrlen, callback, backlog); }


template <typename Instance>
inline void singleton<Instance>::timer_thread(
		const timespec* value, const timespec* interval,
		timer_callback_t callback)
	{ s_core->timer_thread(value, interval, callback); }

template <typename Instance>
inline void singleton<Instance>::timer_event(
		const timespec* value, const timespec* interval,
		timer_callback_t callback)
	{ s_core->timer_event(value, interval, callback); }


template <typename Instance>
inline void singleton<Instance>::signal_thread(
		const sigset_t* set, signal_callback_t callback)
	{ s_core->signal_thread(set, callback); } 

template <typename Instance>
inline void singleton<Instance>::signal_event(
		int signo, signal_callback_t callback)
	{ s_core->signal_event(signo, callback); } 


}  // namespace wavy
}  // namespace mp

#endif /* mp/wavy/singleton.h */

