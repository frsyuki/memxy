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
#include "gate_memtext_impl.h"
#include "gate_memtext_storage.h"

namespace memxy {
namespace memtext {


int request_set(void* user,
		memtext_command cmd,
		memtext_request_storage* r)
{
	int fd = CAST_USER(user);

	memcached_return err;

	{
		proxy_client::ref mc( proxy_client::get() );
		err = memcached_set(*mc, r->key, r->key_len,
				r->data, r->data_len, r->exptime, r->flags);
	}

	if(r->noreply) { return 0; }

	if(err) {
		send_error(fd, err);
		return 0;
	}

	send_static(fd, "STORED\r\n");

	return 0;
}


}  // namespace memtext
}  // namespace memxy

