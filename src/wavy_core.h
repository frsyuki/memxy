//
// memxy::wavy_core - memcached proxy
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
#ifndef MEMXY_WAVY_CORE_H__
#define MEMXY_WAVY_CORE_H__

#include <mp/wavy/singleton.h>

namespace memxy {
namespace service {


void init();

void daemonize(const char* pidfile = NULL, const char* stdio = "/dev/null");

void start(size_t num_threads,
		void (*init)(void*) = NULL, void* user = NULL);

void step_next();

void join();

void end();

bool is_end();


}  // namespace service


struct memxy_wavy;

typedef mp::wavy::singleton<memxy_wavy> core;


}  // namespace memxy

#endif /* wavy_core.h */

