//
// mp::wavy::core
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

#ifndef MP_WAVY_CORE_H__
#define MP_WAVY_CORE_H__

#include "mp/functional.h"
#include "mp/memory.h"
#include "mp/pthread.h"
#include "mp/object_callback.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <stdlib.h>
#include <memory>
#include <queue>

namespace mp {
namespace wavy {


class basic_handler;
class handler;


class core {
public:
	core();
	~core();

	void step_next();

	void add_thread(size_t num);
	void add_thread(size_t num, void (*init)(void*), void* user);

	void end();
	bool is_end() const;

	void join();
	void detach();


	typedef void (*finalize_t)(void* user);


	void write(int fd, const void* buf, size_t size);


	void write(int fd, const void* buf, size_t size,
			finalize_t fin, void* user);

	template <typename T>
	void write(int fd, const void* buf, size_t size,
			std::auto_ptr<T>& fin);

	template <typename T>
	void write(int fd, const void* buf, size_t size,
			mp::shared_ptr<T> fin);
	

	void writev(int fd, const struct iovec* vec, size_t veclen,
			finalize_t fin, void* user);

	template <typename T>
	void writev(int fd, const struct iovec* vec, size_t veclen,
			std::auto_ptr<T>& fin);

	template <typename T>
	void writev(int fd, const struct iovec* vec, size_t veclen,
			mp::shared_ptr<T> fin);


	void sendfile(int fd, int infd, uint64_t off, size_t size,
			finalize_t fin, void* user);
	
	void hsendfile(int fd,
			const void* header, size_t header_size,
			int infd, uint64_t off, size_t size,
			finalize_t fin, void* user);
	
	void hvsendfile(int fd,
			const struct iovec* header_vec, size_t header_veclen,
			int infd, uint64_t off, size_t size,
			finalize_t fin, void* user);

	struct xfer;

	void commit(int fd, xfer* xf);


	template <typename Handler>
	shared_ptr<Handler> add_handler();
MP_ARGS_BEGIN
	template <typename Handler, MP_ARGS_TEMPLATE>
	shared_ptr<Handler> add_handler(MP_ARGS_PARAMS);
MP_ARGS_END

	template <typename F>
	void submit(F f);
MP_ARGS_BEGIN
	template <typename F, MP_ARGS_TEMPLATE>
	void submit(F f, MP_ARGS_PARAMS);
MP_ARGS_END


	typedef function<void (int fd, int err)> connect_callback_t;

	void connect_thread(int socket_family, int socket_type, int protocol,
			const sockaddr* addr, socklen_t addrlen,
			int timeout_msec, connect_callback_t callback);

	void connect_event(int socket_family, int socket_type, int protocol,
			const sockaddr* addr, socklen_t addrlen,
			int timeout_msec, connect_callback_t callback);


	typedef function<void (int fd, int err)> listen_callback_t;

	int listen_event(int socket_family, int socket_type, int protocol,
			const sockaddr* addr, socklen_t addrlen,
			listen_callback_t callback,
			int backlog = 1024);


	typedef function<void ()> timer_callback_t;

	void timer_thread(const timespec* value, const timespec* interval,
			timer_callback_t callback);

	void timer_event(const timespec* value, const timespec* interval,
			timer_callback_t callback);


	typedef function<bool (int signo)> signal_callback_t;

	void signal_thread(const sigset_t* set, signal_callback_t callback);

	void signal_event(int signo, signal_callback_t callback);

private:
	typedef shared_ptr<basic_handler> shared_handler;
	shared_handler add_handler_impl(shared_handler sh);

	typedef function<void ()> task_t;
	void submit_impl(task_t f);

private:
	void* m_impl;

	core(const core&);
};


struct port_event;


struct basic_handler {
public:
	typedef bool (*callback_t)(basic_handler*, const port_event*);

	template <typename IMPL>
	basic_handler(int ident, IMPL* self) :
		m_ident(ident), m_callback(&static_callback<IMPL>) { }

	basic_handler(int ident, callback_t callback) :
		m_ident(ident), m_callback(callback) { }

	virtual ~basic_handler() { }

	int ident() const { return m_ident; }

	bool operator() (const port_event* e);

private:
	int m_ident;

	callback_t m_callback;

private:
	template <typename IMPL>
	static bool static_callback(basic_handler* self, const port_event* e)
	{
		return (*static_cast<IMPL*>(self))(e);
	}

	basic_handler();
	basic_handler(const basic_handler&);
};


struct handler : public mp::enable_shared_from_this<handler>, public basic_handler {
public:
	handler(int fd) : basic_handler(fd, &read_event_callback) { }

	~handler() { ::close(fd()); }

	int fd() const { return ident(); }

	virtual void read_event() = 0;

public:
	template <typename IMPL>
	shared_ptr<IMPL> shared_self()
	{
		return static_pointer_cast<IMPL>(enable_shared_from_this<handler>::shared_from_this());
	}

	template <typename IMPL>
	shared_ptr<IMPL const> shared_self() const
	{
		return static_pointer_cast<IMPL>(enable_shared_from_this<handler>::shared_from_this());
	}

private:
	static inline bool read_event_callback(basic_handler* self, const port_event* e)
	{
		static_cast<handler*>(self)->read_event();
		return true;
	}
	friend class basic_handler;
};


inline bool basic_handler::operator() (const port_event* e)
{
	if(m_callback == handler::read_event_callback) {
		return handler::read_event_callback(this, e);
	} else {
		return m_callback(this, e);
	}
}


struct core::xfer {
public:
	xfer();
	~xfer();

	typedef core::finalize_t finalize_t;

	void push_write(const void* buf, size_t size);

	void push_writev(const struct iovec* vec, size_t veclen);

	void push_sendfile(int infd, uint64_t off, size_t len);

	void push_finalize(finalize_t fin, void* user);

	template <typename T>
	void push_finalize(std::auto_ptr<T> fin);

	template <typename T>
	void push_finalize(mp::shared_ptr<T> fin);

	bool empty() const;

	void reset();

	void migrate(xfer* to);

protected:
	char* m_head;
	char* m_tail;
	size_t m_free;

	void reserve(size_t reqsz);

private:
	xfer(const xfer&);
};



template <typename Handler>
shared_ptr<Handler> core::add_handler()
	{ return static_pointer_cast<Handler>(add_handler_impl(
			shared_ptr<Handler>(new Handler())) ); }
MP_ARGS_BEGIN
template <typename Handler, MP_ARGS_TEMPLATE>
shared_ptr<Handler> core::add_handler(MP_ARGS_PARAMS)
	{ return static_pointer_cast<Handler>(add_handler_impl(
			shared_ptr<Handler>(new Handler(MP_ARGS_FUNC))) ); }
MP_ARGS_END

template <typename F>
inline void core::submit(F f)
	{ submit_impl(task_t(f)); }
MP_ARGS_BEGIN
template <typename F, MP_ARGS_TEMPLATE>
inline void core::submit(F f, MP_ARGS_PARAMS)
	{ submit_impl(bind(f, MP_ARGS_FUNC)); }
MP_ARGS_END


inline core::xfer::xfer() :
	m_head(NULL), m_tail(NULL), m_free(0) { }

inline core::xfer::~xfer()
{
	if(m_head) {
		reset();
		::free(m_head);
	}
}

inline bool core::xfer::empty() const
{
	return m_head == m_tail;
}

template <typename T>
inline void core::xfer::push_finalize(std::auto_ptr<T> fin)
{
	push_finalize(&mp::object_delete<T>, reinterpret_cast<void*>(fin.get()));
	fin.release();
}

template <typename T>
inline void core::xfer::push_finalize(mp::shared_ptr<T> fin)
{
	std::auto_ptr<mp::shared_ptr<T> > afin(new mp::shared_ptr<T>(fin));
	push_finalize(afin);
}

template <typename T>
void write(int fd, const void* buf, size_t size,
		std::auto_ptr<T>& fin)
{
	write(fd, buf, size, &mp::object_delete<T>, fin.get());
	fin.release();
}

template <typename T>
void write(int fd, const void* buf, size_t size,
		mp::shared_ptr<T> fin)
{
	std::auto_ptr<mp::shared_ptr<T> > afin(new mp::shared_ptr<T>(fin));
	write(fd, buf, size, afin);
}


template <typename T>
inline void core::writev(int fd, const struct iovec* vec, size_t veclen,
		std::auto_ptr<T>& fin)
{
	writev(fd, vec, veclen, &mp::object_delete<T>, fin.get());
	fin.release();
}

template <typename T>
inline void core::writev(int fd, const struct iovec* vec, size_t veclen,
		mp::shared_ptr<T> fin)
{
	std::auto_ptr<mp::shared_ptr<T> > afin(new mp::shared_ptr<T>(fin));
	writev(fd, vec, veclen, afin);
}


}  // namespace wavy
}  // namespace mp

#endif /* mp/wavy/core.h */

