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

#ifndef sw_Thread_hpp
#define sw_Thread_hpp

#include <atomic>
#include <cstdlib>
#include <pthread.h>
#include <unistd.h>

#define TLS_OUT_OF_INDEXES (pthread_key_t)(~0)

namespace sw {

class Event;

class Thread
{
public:
	Thread(void (*threadFunction)(void *parameters), void *parameters);

	~Thread();

	void join();

	static void yield();
	static void sleep(int milliseconds);

	typedef pthread_key_t LocalStorageKey;

	static LocalStorageKey allocateLocalStorageKey(void (*destructor)(void *storage) = free);
	static void freeLocalStorageKey(LocalStorageKey key);
	static void *allocateLocalStorage(LocalStorageKey key, size_t size);
	static void *getLocalStorage(LocalStorageKey key);
	static void freeLocalStorage(LocalStorageKey key);

private:
	struct Entry
	{
		void (*const threadFunction)(void *parameters);
		void *threadParameters;
		Event *init;
	};

	static void *startFunction(void *parameters);
	pthread_t handle;

	bool hasJoined = false;
};

class Event
{
	friend class Thread;

public:
	Event();

	~Event();

	void signal();
	void wait();

private:
	pthread_cond_t handle;
	pthread_mutex_t mutex;
	volatile bool signaled;
};

int atomicIncrement(int volatile *value);
int atomicDecrement(int volatile *value);

inline void Thread::yield()
{
	sched_yield();
}

inline void Thread::sleep(int milliseconds)
{
	usleep(1000 * milliseconds);
}

inline Thread::LocalStorageKey Thread::allocateLocalStorageKey(void (*destructor)(void *storage))
{
	LocalStorageKey key;
	pthread_key_create(&key, destructor);
	return key;
}

inline void Thread::freeLocalStorageKey(LocalStorageKey key)
{
	pthread_key_delete(key);
}

inline void *Thread::allocateLocalStorage(LocalStorageKey key, size_t size)
{
	if(key == TLS_OUT_OF_INDEXES)
	{
		return nullptr;
	}

	freeLocalStorage(key);

	void *storage = malloc(size);

	pthread_setspecific(key, storage);

	return storage;
}

inline void *Thread::getLocalStorage(LocalStorageKey key)
{
	if(key == TLS_OUT_OF_INDEXES)
	{
		return nullptr;
	}

	return pthread_getspecific(key);
}

inline void Thread::freeLocalStorage(LocalStorageKey key)
{
	free(getLocalStorage(key));

	pthread_setspecific(key, nullptr);
}

inline void Event::signal()
{
	pthread_mutex_lock(&mutex);
	signaled = true;
	pthread_cond_signal(&handle);
	pthread_mutex_unlock(&mutex);
}

inline void Event::wait()
{
	pthread_mutex_lock(&mutex);
	while(!signaled) pthread_cond_wait(&handle, &mutex);
	signaled = false;
	pthread_mutex_unlock(&mutex);
}

inline int atomicIncrement(volatile int *value)
{
	return __sync_add_and_fetch(value, 1);
}

inline int atomicDecrement(volatile int *value)
{
	return __sync_sub_and_fetch(value, 1);
}

class AtomicInt
{
public:
	AtomicInt() : ai() {}
	AtomicInt(int i) : ai(i) {}

	inline operator int() const { return ai.load(std::memory_order_acquire); }
	inline void operator=(const AtomicInt& i) { ai.store(i.ai.load(std::memory_order_acquire), std::memory_order_release); }
	inline void operator=(int i) { ai.store(i, std::memory_order_release); }
	inline void operator--() { ai.fetch_sub(1, std::memory_order_acq_rel); }
	inline void operator++() { ai.fetch_add(1, std::memory_order_acq_rel); }
	inline int operator--(int) { return ai.fetch_sub(1, std::memory_order_acq_rel) - 1; }
	inline int operator++(int) { return ai.fetch_add(1, std::memory_order_acq_rel) + 1; }
	inline void operator-=(int i) { ai.fetch_sub(i, std::memory_order_acq_rel); }
	inline void operator+=(int i) { ai.fetch_add(i, std::memory_order_acq_rel); }
private:
	std::atomic<int> ai;
};

}

#endif
