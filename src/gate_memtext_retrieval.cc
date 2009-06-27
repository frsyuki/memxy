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
	size_t vallen;
	uint32_t flags;
	char* val;

	{
		proxy_client::ref mc( proxy_client::get() );
		val = memcached_get(*mc, key, keylen, &vallen, &flags, &err);
	}

	if(err) {
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

	struct iovec vb[3];
	vb[0].iov_base = header;
	vb[0].iov_len  = p - header;
	vb[1].iov_base = const_cast<char*>(val);
	vb[1].iov_len  = vallen;
	vb[2].iov_base = const_cast<char*>("\r\nEND\r\n");
	vb[2].iov_len  = 7;

	core::writev(fd, vb, 3, &::free, header);
	return 0;
}

static int request_get_multi(void* user,
		memtext_command cmd,
		memtext_request_retrieval* r,
		bool require_cas)
{
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

