//
// mpio exclusive
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

#ifndef MP_EXCLUSIVE_H__
#define MP_EXCLUSIVE_H__

#include "mp/pthread.h"

namespace mp {


template <typename T>
class exclusive {
public:
	exclusive() : m_obj() { }
MP_ARGS_BEGIN
	template <MP_ARGS_TEMPLATE>
	exclusive(MP_ARGS_PARAMS) : m_obj(MP_ARGS_FUNC) { }
MP_ARGS_END

	~exclusive() { }

	T& unsafe_ref() { return m_obj; }
	const T& unsafe_ref() const { return m_obj; }

	class ref {
	public:
		ref(exclusive<T>& obj) : m_ref(NULL)
		{
			obj.m_mutex.lock();
			m_ref = &obj;
		}
	
		ref() : m_ref(NULL) { }
	
		~ref() { reset(); }
	
		void reset()
		{
			if(m_ref) {
				m_ref->m_mutex.unlock();
				m_ref = NULL;
			}
		}
	
		void reset(exclusive<T>& obj)
		{
			reset();
			obj.m_mutex.lock();
			m_ref = &obj;
		}
	
		T& operator*() { return m_ref->m_obj; }
		T* operator->() { return &operator*(); }
		const T& operator*() const { return m_ref->m_obj; }
		const T* operator->() const { return &operator*(); }
	
	private:
		exclusive<T>* m_ref;
	
	private:
		ref(const ref&);
	};

private:
	T m_obj;
	pthread_mutex m_mutex;
	friend class ref;

private:
	exclusive(const exclusive&);
};


}  // namespace mp

#endif /* mp/exclusive.h */

