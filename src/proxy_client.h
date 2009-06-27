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
#ifndef MEMXY_PROXY_CLIENT_H__
#define MEMXY_PROXY_CLIENT_H__

#include <mp/exclusive.h>
#include <vector>
#include <libmemcached/memcached.h>

namespace memxy {
namespace proxy_client {


typedef mp::exclusive<memcached_st*> exclusive_t;
typedef exclusive_t::ref ref;

void thread_init_func(void* null);

void init();
void destroy();

void set_servers(const char* server_list);

exclusive_t& get();


}  // namespace proxy_client
}  // namespace memxy

#endif /* proxy_client.h */

