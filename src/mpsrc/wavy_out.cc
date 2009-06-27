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
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#ifdef __linux__
#include <sys/sendfile.h>
#endif

namespace mp {
namespace wavy {


namespace {

class xferimpl : public core::xfer {
public:
	xferimpl() { }
	~xferimpl() { }

	bool try_write(int fd);

	void push_xfraw(char* buf, size_t size);

	static size_t sizeof_mem();
	static size_t sizeof_iovec(size_t veclen);
	static size_t sizeof_sendfile();
	static size_t sizeof_finalize();

	static char* fill_mem(char* from, const void* buf, size_t size);
	static char* fill_iovec(char* from, const struct iovec* vec, size_t veclen);
	static char* fill_sendfile(char* from, int infd, uint64_t off, size_t len);
	static char* fill_finalize(char* from, finalize_t fin, void* user);

	static bool execute(int fd, char* head, char** tail);

public:
	pthread_mutex& mutex() { return m_mutex; }

private:
	pthread_mutex m_mutex;

private:
	xferimpl(const xferimpl&);
};

}  // noname namespace


#define ANON_fdctx reinterpret_cast<xferimpl*>(m_fdctx)

out::out() : basic_handler(m_port.ident(), this)
{
	struct rlimit rbuf;
	if(::getrlimit(RLIMIT_NOFILE, &rbuf) < 0) {
		throw system_error(errno, "getrlimit() failed");
	}
	m_fdctx = new xferimpl[rbuf.rlim_cur];
}

out::~out()
{
	delete[] ANON_fdctx;
}


namespace {

typedef unsigned int xfer_type;

static const xfer_type XF_IOVEC    = 0;
static const xfer_type XF_SENDFILE = 1;
static const xfer_type XF_FINALIZE = 3;
//static const xfer_type             = 5;
//static const xfer_type             = 7;

struct xfer_sendfile {
	int infd;
	uint64_t off;
	size_t len;
};

struct xfer_finalize {
	void (*finalize)(void*);
	void* user;
};


inline size_t xferimpl::sizeof_mem()
{
	return sizeof(xfer_type) + sizeof(struct iovec)*1;
}

inline size_t xferimpl::sizeof_iovec(size_t veclen)
{
	return sizeof(xfer_type) + sizeof(iovec) * veclen;
}

inline size_t xferimpl::sizeof_sendfile()
{
	return sizeof(xfer_type) + sizeof(xfer_sendfile);
}

inline size_t xferimpl::sizeof_finalize()
{
	return sizeof(xfer_type) + sizeof(xfer_finalize);
}

inline char* xferimpl::fill_mem(char* from, const void* buf, size_t size)
{
	*(xfer_type*)from = 1 << 1;
	from += sizeof(xfer_type);

	((struct iovec*)from)->iov_base = const_cast<void*>(buf);
	((struct iovec*)from)->iov_len  = size;
	from += sizeof(struct iovec);

	return from;
}

inline char* xferimpl::fill_iovec(char* from, const struct iovec* vec, size_t veclen)
{
	*(xfer_type*)from = veclen << 1;
	from += sizeof(xfer_type);

	const size_t iovbufsz = sizeof(struct iovec) * veclen;
	memcpy(from, vec, iovbufsz);
	from += iovbufsz;

	return from;
}

inline char* xferimpl::fill_sendfile(char* from, int infd, uint64_t off, size_t len)
{
	*(xfer_type*)from = XF_SENDFILE;
	from += sizeof(xfer_type);

	((xfer_sendfile*)from)->infd = infd;
	((xfer_sendfile*)from)->off = off;
	((xfer_sendfile*)from)->len = len;
	from += sizeof(xfer_sendfile);

	return from;
}

inline char* xferimpl::fill_finalize(char* from, finalize_t fin, void* user)
{
	*(xfer_type*)from = XF_FINALIZE;
	from += sizeof(xfer_type);

	((xfer_finalize*)from)->finalize = fin;
	((xfer_finalize*)from)->user = user;
	from += sizeof(xfer_finalize);

	return from;
}

void xferimpl::push_xfraw(char* buf, size_t size)
{
	if(m_free < size) { reserve(size); }
	memcpy(m_tail, buf, size);
	m_tail += size;
	m_free -= size;
}

}  // noname namespace


void core::xfer::reserve(size_t reqsz)
{
	size_t used = m_tail - m_head;
	reqsz += used;
	size_t nsize = (used + m_free) * 2 + 72;  // used + m_free may be 0

	while(nsize < reqsz) { nsize *= 2; }

	char* tmp = (char*)::realloc(m_head, nsize);
	if(!tmp) { throw std::bad_alloc(); }

	m_head = tmp;
	m_tail = tmp + used;
	m_free = nsize - used;
}


void core::xfer::push_write(const void* buf, size_t size)
{
	size_t sz = xferimpl::sizeof_mem();
	if(m_free < sz) { reserve(sz); }
	m_tail = xferimpl::fill_mem(m_tail, buf, size);
	m_free -= sz;
}

void core::xfer::push_writev(const struct iovec* vec, size_t veclen)
{
	size_t sz = xferimpl::sizeof_iovec(veclen);
	if(m_free < sz) { reserve(sz); }
	m_tail = xferimpl::fill_iovec(m_tail, vec, veclen);
	m_free -= sz;
}

void core::xfer::push_sendfile(int infd, uint64_t off, size_t len)
{
	size_t sz = xferimpl::sizeof_sendfile();
	if(m_free < sz) { reserve(sz); }
	m_tail = xferimpl::fill_sendfile(m_tail, infd, off, len);
	m_free -= sz;
}

void core::xfer::push_finalize(finalize_t fin, void* user)
{
	size_t sz = xferimpl::sizeof_finalize();
	if(m_free < sz) { reserve(sz); }
	m_tail = xferimpl::fill_finalize(m_tail, fin, user);
	m_free -= sz;
}

void core::xfer::migrate(xfer* to)
{
	if(to->m_head == NULL) {
		// swap
		to->m_head = m_head;
		to->m_tail = m_tail;
		to->m_free = m_free;
		m_tail = m_head = NULL;
		m_free = 0;
		return;
	}

	size_t reqsz = m_tail - m_head;
	if(to->m_free < reqsz) { to->reserve(reqsz); }
	
	memcpy(to->m_tail, m_head, reqsz);
	to->m_tail += reqsz;
	to->m_free -= reqsz;
	
	m_free += reqsz;
	m_tail = m_head;
}

void core::xfer::reset()
{
	for(char* p = m_head; p < m_tail; ) {
		switch(*(xfer_type*)p) {
		case XF_SENDFILE:
			p += xferimpl::sizeof_sendfile();
			break;

		case XF_FINALIZE: {
			xfer_finalize* x = (xfer_finalize*)(p + sizeof(xfer_type));
			if(x->finalize) try {
				x->finalize(x->user);
			} catch (...) { }

			p += xferimpl::sizeof_finalize();
			break; }

		default:  // XF_IOVEC
			p += xferimpl::sizeof_iovec( (*(xfer_type*)p) >> 1 );
			break;
		}
	}

	//::free(m_head);
	//m_tail = m_head = NULL;
	//m_free = 0;
	m_free += m_tail - m_head;
	m_tail = m_head;
}


namespace {

#define MP_WAVY_XFER_CONSUMED \
	do { \
		size_t left = endp - p; \
		::memmove(head, p, left); \
		*tail = head + left; \
	} while(0)

bool xferimpl::execute(int fd, char* head, char** tail)
{
	char* p = head;
	char* const endp = *tail;
	while(p < endp) {
		switch(*(xfer_type*)p) {
		case XF_SENDFILE: {
			xfer_sendfile* x = (xfer_sendfile*)(p + sizeof(xfer_type));
#ifdef __linux__
			off_t off = x->off;
			ssize_t wl = ::sendfile(fd, x->infd, &off, x->len);
			if(wl <= 0) {
				MP_WAVY_XFER_CONSUMED;
				if(wl < 0 && (errno == EAGAIN || errno == EINTR)) {
					return true;
				} else {
					return false;
				}
			}
#else
			off_t wl = x->size;
			if(::sendfile(x->infd, fd, x->off, &wl, NULL, 0) < 0) {
				MP_WAVY_XFER_CONSUMED;
				if(errno == EAGAIN || errno == EINTR) {
					return true;
				} else {
					return false;
				}
			}
#endif

			if(static_cast<size_t>(wl) < x->len) {
				x->off += wl;
				x->len -= wl;
				MP_WAVY_XFER_CONSUMED;
				return true;
			}

			p += sizeof_sendfile();
			break; }

		case XF_FINALIZE: {
			xfer_finalize* x = (xfer_finalize*)(p + sizeof(xfer_type));
			if(x->finalize) try {
				x->finalize(x->user);
			} catch (...) { }

			p += xferimpl::sizeof_finalize();
			break; }

		default: {  // XF_IOVEC
			size_t veclen = (*(xfer_type*)p) >> 1;
			struct iovec* vec = (struct iovec*)(p + sizeof(xfer_type));

			ssize_t wl = ::writev(fd, vec, veclen);
			if(wl <= 0) {
				MP_WAVY_XFER_CONSUMED;
				if(wl < 0 && (errno == EAGAIN || errno == EINTR)) {
					return true;
				} else {
					return false;
				}
			}

			for(size_t i=0; i < veclen; ++i) {
				if(static_cast<size_t>(wl) >= vec[i].iov_len) {
					wl -= vec[i].iov_len;
				} else {
					vec[i].iov_base = (void*)(((char*)vec[i].iov_base) + wl);
					vec[i].iov_len -= wl;

					if(i == 0) {
						MP_WAVY_XFER_CONSUMED;
					} else {
						p += sizeof_iovec(veclen);
						size_t left = endp - p;
						char* filltail = fill_iovec(head, vec+i, veclen-i);
						::memmove(filltail, p, left);
						*tail = filltail + left;
					}

					return true;
				}
			}

			p += sizeof_iovec(veclen);

			break; }
		}
	}

	*tail = head;
	return false;
}


bool xferimpl::try_write(int fd)
{
	char* const alloc_end = m_tail + m_free;
	bool cont = execute(fd, m_head, &m_tail);
	m_free = alloc_end - m_tail;

	if(!cont && !empty()) {
		// error occured
		::shutdown(fd, SHUT_RD);
	}
	return cont;
}

}  // noname namespace


void out::write_event(port::event e)
{
	int ident = e.ident();

	xferimpl& ctx(ANON_fdctx[ident]);
	pthread_scoped_lock lk(ctx.mutex());

	bool cont;
	try {
		cont = ctx.try_write(ident);
	} catch (...) {
		cont = false;
	}
	
	if(!cont) {
		m_port.shot_remove(e);
		ctx.reset();
	} else {
		m_port.shot_reactivate(e);
	}
}

inline void out::watch(int fd)
{
	m_port.add_fd(fd, EVPORT_WRITE);
}


void out::commit_raw(int fd, char* xfbuf, char* xfendp)
{
	xferimpl& ctx(ANON_fdctx[fd]);
	pthread_scoped_lock lk(ctx.mutex());

	if(!ctx.empty()) {
		ctx.push_xfraw(xfbuf, xfendp - xfbuf);
		return;
	}

	if(xferimpl::execute(fd, xfbuf, &xfendp)) {
		ctx.push_xfraw(xfbuf, xfendp - xfbuf);  // FIXME exception
		watch(fd);  // FIXME exception
	}
}

void out::commit(int fd, xfer* xf)
{
	xferimpl& ctx(ANON_fdctx[fd]);
	pthread_scoped_lock lk(ctx.mutex());

	if(!ctx.empty()) {
		xf->migrate(&ctx);
		return;
	}

	if(static_cast<xferimpl*>(xf)->try_write(fd)) {
		xf->migrate(&ctx);  // FIXME exception
		watch(fd);  // FIXME exception
	}
}

void out::write(int fd, const void* buf, size_t size)
{
	xferimpl& ctx(ANON_fdctx[fd]);
	pthread_scoped_lock lk(ctx.mutex());

	if(ctx.empty()) {
		ssize_t wl = ::write(fd, buf, size);
		if(wl <= 0) {
			if(wl == 0 || (errno != EINTR && errno != EAGAIN)) {
				::shutdown(fd, SHUT_RD);
				return;
			}
		} else if(static_cast<size_t>(wl) >= size) {
			return;
		} else {
			buf  = ((const char*)buf) + wl;
			size -= wl;
		}

		ctx.push_write(buf, size);
		watch(fd);

	} else {
		ctx.push_write(buf, size);
	}
}


#define ANON_out static_cast<coreimpl*>(m_impl)->m_out

void core::commit(int fd, xfer* xf)
	{ ANON_out->commit(fd, xf); }

void core::write(int fd, const void* buf, size_t size)
	{ ANON_out->write(fd, buf, size); }

void core::write(int fd,
		const void* buf, size_t size,
		finalize_t fin, void* user)
{
	char xfbuf[ xferimpl::sizeof_mem() + xferimpl::sizeof_finalize() ];
	char* p = xfbuf;
	p = xferimpl::fill_mem(p, buf, size);
	p = xferimpl::fill_finalize(p, fin, user);
	ANON_out->commit_raw(fd, xfbuf, p);
}

void core::writev(int fd,
		const struct iovec* vec, size_t veclen,
		finalize_t fin, void* user)
{
	char xfbuf[ xferimpl::sizeof_iovec(veclen) + xferimpl::sizeof_finalize() ];
	char* p = xfbuf;
	p = xferimpl::fill_iovec(p, vec, veclen);
	p = xferimpl::fill_finalize(p, fin, user);
	ANON_out->commit_raw(fd, xfbuf, p);
}

void core::sendfile(int fd,
		int infd, uint64_t off, size_t size,
		finalize_t fin, void* user)
{
	char xfbuf[ xferimpl::sizeof_sendfile() + xferimpl::sizeof_finalize() ];
	char* p = xfbuf;
	p = xferimpl::fill_sendfile(p, infd, off, size);
	p = xferimpl::fill_finalize(p, fin, user);
	ANON_out->commit_raw(fd, xfbuf, p);
}

void core::hsendfile(int fd,
		const void* header, size_t header_size,
		int infd, uint64_t off, size_t size,
		finalize_t fin, void* user)
{
	char xfbuf[ xferimpl::sizeof_mem()
		+ xferimpl::sizeof_sendfile() + xferimpl::sizeof_finalize() ];
	char* p = xfbuf;
	p = xferimpl::fill_mem(p, header, header_size);
	p = xferimpl::fill_sendfile(p, infd, off, size);
	p = xferimpl::fill_finalize(p, fin, user);
	ANON_out->commit_raw(fd, xfbuf, p);
}

void core::hvsendfile(int fd,
		const struct iovec* header_vec, size_t header_veclen,
		int infd, uint64_t off, size_t size,
		finalize_t fin, void* user)
{
	char xfbuf[ xferimpl::sizeof_iovec(header_veclen)
		+ xferimpl::sizeof_sendfile() + xferimpl::sizeof_finalize() ];
	char* p = xfbuf;
	p = xferimpl::fill_iovec(p, header_vec, header_veclen);
	p = xferimpl::fill_sendfile(p, infd, off, size);
	p = xferimpl::fill_finalize(p, fin, user);
	ANON_out->commit_raw(fd, xfbuf, p);
}


}  // namespace wavy
}  // namespace mp

