//
// mp::wavy::port
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

#ifndef MP_WAVY_PORT_H__
#define MP_WAVY_PORT_H__

#include "mp/pp.h"

#ifndef MP_WAVY_PORT
#  if   defined(HAVE_SYS_EPOLL_H)
#    define MP_WAVY_PORT epoll
#  elif defined(HAVE_SYS_EVENT_H)
#    define MP_WAVY_PORT kqueue
#  elif defined(HAVE_PORT_H)
#    define MP_WAVY_PORT eventport
#  else
#    if   defined(__linux__)
#      define MP_WAVY_PORT epoll
#    elif defined(__APPLE__) && defined(__MACH__)
#      define MP_WAVY_PORT kqueue
#    elif defined(__FreeBSD__) || defined(__NetBSD__)
#      define MP_WAVY_PORT kqueue
#    elif defined(__SunOS__)
#      define MP_WAVY_PORT eventport
#    else
#      define MP_WAVY_PORT select
#    endif
#  endif
#endif

#define MP_WAVY_PORT_HEADER(sys) \
	MP_PP_HEADER(., wavy_port_, sys, )

#ifndef MP_WAVY_PORT_BACKLOG_SIZE
#define MP_WAVY_PORT_BACKLOG_SIZE 256
#endif

#include MP_WAVY_PORT_HEADER(MP_WAVY_PORT)

#endif /* wavy_port.h */

