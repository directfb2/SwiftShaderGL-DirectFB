// Copyright 2016 The SwiftShader Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef sw_MutexLock_hpp
#define sw_MutexLock_hpp

#include <pthread.h>

namespace sw {

class MutexLock
{
public:
	MutexLock()
	{
		pthread_mutex_init(&mutex, NULL);
	}

	~MutexLock()
	{
		pthread_mutex_destroy(&mutex);
	}

	bool attemptLock()
	{
		return pthread_mutex_trylock(&mutex) == 0;
	}

	void lock()
	{
		pthread_mutex_lock(&mutex);
	}

	void unlock()
	{
		pthread_mutex_unlock(&mutex);
	}

private:
	pthread_mutex_t mutex;
};

}

class LockGuard
{
public:
	explicit LockGuard(sw::MutexLock &mutex) : mutex(&mutex)
	{
		mutex.lock();
	}

	explicit LockGuard(sw::MutexLock *mutex) : mutex(mutex)
	{
		if(mutex) mutex->lock();
	}

	~LockGuard()
	{
		if(mutex) mutex->unlock();
	}

protected:
	sw::MutexLock *mutex;
};

#endif
