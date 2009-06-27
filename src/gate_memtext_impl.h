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
#ifndef GATE_MEMTEXT_IMPL_H__
#define GATE_MEMTEXT_IMPL_H__

#include "gate_memtext.h"
#include "proxy_client.h"
#include "wavy_core.h"

namespace memxy {
namespace memtext {


#define CAST_USER(user) ((int)(ptrdiff_t)(user))


static const char* const NOT_SUPPORTED_REPLY = "CLIENT_ERROR supported\r\n";
static const char* const GET_FAILED_REPLY    = "SERVER_ERROR get failed\r\n";
static const char* const STORE_FAILED_REPLY  = "SERVER_ERROR store failed\r\n";
static const char* const DELETE_FAILED_REPLY = "SERVER_ERROR delete failed\r\n";


static inline void send_static(int fd, const char* str)
{
	core::write(fd, str, strlen(str));
}

static inline void send_error(int fd, int err)
{
	send_static(fd, "SERVER_ERROR failed\r\n");  // FIXME
}


}  // namespace memtext
}  // namespace memxy

#endif /* gate_memtext_impl.h */

