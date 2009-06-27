//
// memxy::gate_memtext - memcached proxy
//
// Copyright (C) 2009 FURUHASHI Sadayuki
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
#define __STDC_LIMIT_MACROS
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include "gate_memtext_impl.h"
#include "gate_memtext_retrieval.h"

#ifndef MEMTEXT_MULTI_MAX
#define MEMTEXT_MULTI_MAX 1024
#endif

namespace memxy {
namespace memtext {


// "VALUE "+keylen+" "+uint32+" "+uint32+" "+uint64+"\r\n\0"
//          keylen     flags      vallen      cas
#define HEADER_SIZE(keylen) \
		6 +(keylen)+ 1+  10  + 1 +  10  + 1 +  20  +  3


static int request_get_single(void* user,
		memtext_command cmd,
		memtext_request_retrieval* r,
		bool require_cas)
{
	int fd = CAST_USER(user);

	memcached_return err;
	const char* const key = r->key[0];
	size_t const keylen   = r->key_len[0];
	char* val;
	size_t vallen;
	uint32_t flags;

	{
		proxy_client::ref mc( proxy_client::get() );
		val = memcached_get(*mc, key, keylen, &vallen, &flags, &err);
	}

	if(err && err != MEMCACHED_NOTFOUND) {
		send_error(fd, err);
		return 0;
	}

	if(!val) {
		send_static(fd, "END\r\n");
		return 0;
	}

	char* const header = (char*)::malloc(HEADER_SIZE(keylen));
	if(!header) { throw std::bad_alloc(); }

	char* p = header;

	memcpy(p, "VALUE ", 6);  p += 6;
	memcpy(p, key, keylen);  p += keylen;
	p += sprintf(p, " %"PRIu32" %lu", flags, vallen);

	if(require_cas) {
		// FIXME cas
		p += sprintf(p, " %"PRIu64"\r\n", (uint64_t)0);
	} else {
		p[0] = '\r'; p[1] = '\n'; p += 2;
	}

	struct iovec vec[3];
	vec[0].iov_base = header;
	vec[0].iov_len  = p - header;
	vec[1].iov_base = const_cast<char*>(val);
	vec[1].iov_len  = vallen;
	vec[2].iov_base = const_cast<char*>("\r\nEND\r\n");
	vec[2].iov_len  = 7;

	core::writev(fd, vec, 3, &::free, header);
	return 0;
}


class multi_set {
public:
	multi_set() : val(NULL), vallen(0), flags(0) { }
	~multi_set() { ::free(val); }

	char* val;
	size_t vallen;
	uint32_t flags;

private:
	multi_set(const multi_set&);
};

class multi_set_carry {
public:
	multi_set_carry(multi_set* set, size_t num, size_t buffer_size) :
		m_alloc((char*)::malloc(sizeof(char**)*num + buffer_size)),
		m_num(num)
	{
		if(!m_alloc) { throw std::bad_alloc(); }
		for(size_t i=0; i < num; ++i) {
			((char**)m_alloc)[i] = set[i].val;
			set[i].val = NULL;
		}
	}

	~multi_set_carry()
	{
		for(size_t i=0; i < m_num; ++i) {
			::free( ((char**)m_alloc)[i] );
		}
		free(m_alloc);
	}

	char* buffer() { return m_alloc + sizeof(char**)*m_num; }

	char* operator[] (size_t i) { return ((char**)m_alloc)[i]; }

private:
	char* m_alloc;
	size_t m_num;
	multi_set_carry(const multi_set&);
};

static int request_get_multi(void* user,
		memtext_command cmd,
		memtext_request_retrieval* r,
		bool require_cas)
{
	int fd = CAST_USER(user);

	if(r->key_num > MEMTEXT_MULTI_MAX) {
		send_error(fd, MEMCACHED_CLIENT_ERROR);
		return 0;
	}

	memcached_return err;
	multi_set multi[r->key_num];
	size_t found_keys = 0;
	size_t total_keylen = 0;

	{
		proxy_client::ref mc( proxy_client::get() );
		err = memcached_mget(*mc, r->key, r->key_len, r->key_num);

		if(err) {
			send_error(fd, err);
			return 0;
		}

		for(unsigned int i=0; i < r->key_num; ++i) {
			multi[i].val = memcached_fetch(*mc, r->key[i], &r->key_len[i],
					&multi[i].vallen, &multi[i].flags, &err);

			if(err) {
				if(err == MEMCACHED_NOTFOUND) { continue; }
				if(err == MEMCACHED_END) { break; }
				send_error(fd, err);
				return 0;
			}

			if(multi[i].val) {
				++found_keys;
				total_keylen += r->key_len[i];
			}
		}
	}

	if(found_keys == 0) {
		send_static(fd, "END\r\n");
		return 0;
	}

	std::auto_ptr<multi_set_carry> carry( new multi_set_carry(
				multi, sizeof(multi)/sizeof(multi_set),
				found_keys*HEADER_SIZE(0) + total_keylen) );

	char* header = carry->buffer();
	char* p = header;
	struct iovec vec[found_keys*2 + 1];  // +1: last END
	struct iovec* pv = vec;

	unsigned int i;
	for(i=0; i < r->key_num; ++i) {
		if( (*carry)[i] != NULL ) {
			break;
		}
	}

	pv->iov_base = p;
	memcpy(p, "VALUE ", 6);  p += 6;
	goto header_set;

	for(; i < r->key_num; ++i) {
		if( (*carry)[i] == NULL ) {
			continue;
		}

		pv->iov_base = p;
		memcpy(p, "\r\nEND\r\nVALUE ", 13);  p += 13;
	header_set:
		memcpy(p, r->key[i], r->key_len[i]);  p += r->key_len[i];
		p += sprintf(p, " %"PRIu32" %lu", multi[i].flags, multi[i].vallen);
		if(require_cas) {
			// FIXME cas
			p += sprintf(p, " %"PRIu64"\r\n", (uint64_t)0);
		} else {
			p[0] = '\r'; p[1] = '\n'; p += 2;
		}
		pv->iov_len = p - header;
		header = p;
		++pv;

		pv->iov_base = (*carry)[i];
		pv->iov_len  = multi[i].vallen;
		++pv;
	}

	pv->iov_base = (void*)"\r\nEND\r\n";
	pv->iov_len  = 7;
	++pv;

	core::writev(fd, vec, sizeof(vec)/sizeof(iovec), carry);

	return 0;
}


int request_get(void* user,
		memtext_command cmd,
		memtext_request_retrieval* r)
{
	if(r->key_num == 1) {
		return request_get_single(user, cmd, r, false);
	} else {
		return request_get_multi(user, cmd, r, false);
	}
}

int request_gets(void* user,
		memtext_command cmd,
		memtext_request_retrieval* r)
{
	if(r->key_num == 1) {
		return request_get_single(user, cmd, r, true);
	} else {
		return request_get_multi(user, cmd, r, true);
	}
	return 0;
}


}  // namespace memtext
}  // namespace memxy

