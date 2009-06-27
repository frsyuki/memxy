//
// mp::endian
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

#ifndef MP_ENDIAN_H__
#define MP_ENDIAN_H__

#include <arpa/inet.h>

namespace mp {


#if 0
inline uint32_t htonl(uint32_t x) { ::htonl(x); }

inline uint16_t htons(uint16_t x) { ::htons(x); }

inline uint32_t ntohl(uint32_t x) { ::ntohl(x); }

inline uint16_t ntohs(uint16_t x) { ::ntohs(x); }
#endif


#ifdef __LITTLE_ENDIAN__

#if defined(__bswap_64)
inline uint64_t htonll(uint64_t x) { return __bswap_64(x); }
inline uint64_t ntohll(uint64_t x) { return __bswap_64(x); }

#elif defined(__DARWIN_OSSwapInt64)
inline uint64_t htonll(uint64_t x) { return __DARWIN_OSSwapInt64(x); }
inline uint64_t ntohll(uint64_t x) { return __DARWIN_OSSwapInt64(x); }

#else
static inline uint64_t htonll(uint64_t x) {
	return	((x << 56) & 0xff00000000000000ULL ) |
			((x << 40) & 0x00ff000000000000ULL ) |
			((x << 24) & 0x0000ff0000000000ULL ) |
			((x <<  8) & 0x000000ff00000000ULL ) |
			((x >>  8) & 0x00000000ff000000ULL ) |
			((x >> 24) & 0x0000000000ff0000ULL ) |
			((x >> 40) & 0x000000000000ff00ULL ) |
			((x >> 56) & 0x00000000000000ffULL ) ;
}
static inline uint64_t ntohll(uint64_t x) {
	return	((x << 56) & 0xff00000000000000ULL ) |
			((x << 40) & 0x00ff000000000000ULL ) |
			((x << 24) & 0x0000ff0000000000ULL ) |
			((x <<  8) & 0x000000ff00000000ULL ) |
			((x >>  8) & 0x00000000ff000000ULL ) |
			((x >> 24) & 0x0000000000ff0000ULL ) |
			((x >> 40) & 0x000000000000ff00ULL ) |
			((x >> 56) & 0x00000000000000ffULL ) ;
}
#endif

#else
inline uint64_t htonll(uint64_t x) { return x; }
inline uint64_t ntohll(uint64_t x) { return x; }
#endif


}  // namespace mp

#endif /* mp/endian.h */

