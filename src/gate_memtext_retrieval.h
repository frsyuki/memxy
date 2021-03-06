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
#ifndef GATE_MEMTEXT_RETRIEVAL_H__
#define GATE_MEMTEXT_RETRIEVAL_H__

#include "memproto/memtext.h"

namespace memxy {
namespace memtext {


int request_get(void* user,
		memtext_command cmd,
		memtext_request_retrieval* r);

int request_gets(void* user,
		memtext_command cmd,
		memtext_request_retrieval* r);


}  // namespace memtext
}  // namespace memxy

#endif /* gate_memtext_retrieval.h */
