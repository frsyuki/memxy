//
// memxy::proxy_client - memcached proxy
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
#include "proxy_client.h"
#include <pthread.h>
#include <string.h>
#include <stdlib.h>

namespace memxy {
namespace proxy_client {


typedef std::vector<exclusive_t*> thread_list_t;
typedef mp::exclusive<thread_list_t> exclusive_thread_list_t;
typedef exclusive_thread_list_t::ref thread_list_ref;

static exclusive_thread_list_t* s_thread_list = NULL;

static __thread exclusive_t* tls = NULL;


void thread_init_func(void*)
{
	memcached_st* mc = memcached_create(NULL);
	tls = new exclusive_t(mc);

	thread_list_ref ls(*s_thread_list);
	ls->push_back(tls);
}

void init()
{
	s_thread_list = new exclusive_thread_list_t();
}

void destroy()
{
	thread_list_ref ls(*s_thread_list);
	for(thread_list_t::iterator it(ls->begin()), it_end(ls->end());
			it != it_end; ++it) {
		ref r(**it);
		memcached_free(*r);
		delete *it;
	}
	delete s_thread_list;
	s_thread_list = NULL;
}

void set_servers(const char* server_list)
{
	memcached_server_st* servers;
	{
		char* server_list_dup = ::strdup(server_list);
		if(!server_list_dup) {
			throw std::bad_alloc();
		}
	
		servers = memcached_servers_parse(server_list_dup);
		::free(server_list_dup);
	
		if(servers == NULL) {
			throw std::runtime_error("invalid server list");
		}
	}

	thread_list_ref ls(*s_thread_list);
	for(thread_list_t::iterator it(ls->begin()), it_end(ls->end());
			it != it_end; ++it) {

		memcached_st* mc = memcached_create(NULL);
		if(!mc) {
			throw std::bad_alloc();
		}

		if(memcached_server_push(mc, servers) != MEMCACHED_SUCCESS) {
			throw std::bad_alloc();
		}

		ref r(**it);
		memcached_free(*r);
		*r = mc;
	}
}

exclusive_t& get()
{
	return *tls;
}


}  // namespace proxy_client
}  // namespace memxy

