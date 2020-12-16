
// Copyright (C) 2014  Thomas Schmidts <Thomas.Schmidts@me.com> https://gist.github.com/13abylon/523685ef6af5bde24dc3
// Copyright (C) 2020  The DOSBox Staging team (adapted to run all tests)
// SPDX-License-Identifier: GPL-2.0-or-later

// This code benchmarks several threaded locking techniques

// g++ main.cpp --std=c++11 -lpthread -O2

#include <iostream>
#include <atomic>
#include <cassert>
#include <thread>
#include <list>
#include <mutex>
#include <time.h>
#include <condition_variable>

#include <stdio.h>

#ifndef WIN32
#include <sys/time.h>
#else
#include <WinSock2.h>

int gettimeofday(struct timeval *tv, void *tz)
{
	union {
		int64_t ns100; // since 1.1.1601 in 100ns units
		FILETIME ft;
	} now;

	GetSystemTimeAsFileTime(&now.ft);

	tv->tv_usec = (long)((now.ns100 / 10LL) % 1000000LL);
	tv->tv_sec = (long)((now.ns100 - 116444736000000000LL) / 10000000LL);

	return 0;
}

#endif
////////////////////////////////////////////////////////////////////////////////
/// @brief seconds with microsecond resolution
////////////////////////////////////////////////////////////////////////////////

double TRI_microtime()
{
	struct timeval t;

	gettimeofday(&t, 0);

	return (t.tv_sec) + (t.tv_usec / 1000000.0);
}

std::mutex theMutex;

#ifdef WIN32
SRWLOCK srwlock;
#else
pthread_rwlock_t rwlock = PTHREAD_RWLOCK_INITIALIZER;
#endif
std::atomic<uint64_t> atomic_uint;

uint64_t flat_uint = 1;

long count;

std::mutex m;
std::condition_variable halt;
bool ready = false;

std::list<std::thread> threads;

void start_wait(void)
{
	std::unique_lock<std::mutex> lk(m);
	halt.wait(lk, [] { return ready; });
}

void threadfunc_unlocked()
{
	start_wait();
	// Wait until main() sends data
	long i;
	for (i = 1; i < count; i++) {
		flat_uint++;
	}
}

void threadfunc_unlocked_read()
{
	start_wait();
	// Wait until main() sends data
	long i, j;
	uint64_t current;
	j = 0;
	for (i = 1; i <= count; i++) {
		current = flat_uint;
		if (current > 0) {
			j++;
		} else {
			std::cout << "WTF?";
		}
	}
	if (j != count) {
		std::cout << "Not matched count j=" << j << "count=" << count << "\n";
	}
}

void threadfunc_mutex()
{
	start_wait();
	long i;
	for (i = 1; i <= count; i++) {
		theMutex.lock();
		flat_uint++;
		theMutex.unlock();
	}
}

void threadfunc_mutex_read()
{
	start_wait();
	uint64_t current;
	long i, j;
	j = 0;
	for (i = 1; i <= count; i++) {
		theMutex.lock();
		current = flat_uint;
		theMutex.unlock();
		if (current > 0) {
			j++;
		} else {
			std::cout << "WTF?";
		}
	}
	if (j != count) {
		std::cout << "Not matched count j=" << j << "count=" << count << "\n";
	}
}

void threadfunc_rwlock()
{
	start_wait();
	long i;
	for (i = 0; i <= count; i++) {
#ifdef WIN32
		AcquireSRWLockExclusive(&srwlock);
		flat_uint++;
		ReleaseSRWLockExclusive(&srwlock);
#else
		pthread_rwlock_wrlock(&rwlock);
		flat_uint++;
		pthread_rwlock_unlock(&rwlock);
#endif
	}
}

void threadfunc_rwlock_read_wr()
{
	start_wait();
	long i, j;
	uint64_t current;
	j = 0;
	for (i = 1; i <= count; i++) {
#ifdef WIN32
		AcquireSRWLockExclusive(&srwlock);
		current = flat_uint;
		ReleaseSRWLockExclusive(&srwlock);
#else
		pthread_rwlock_wrlock(&rwlock);
		current = flat_uint;
		pthread_rwlock_unlock(&rwlock);
#endif
		if (current > 0) {
			j++;
		} else {
			std::cout << "WTF?";
		}
	}
	if (j != count) {
		std::cout << "Not matched count j=" << j << "count=" << count << "\n";
	}
}

void threadfunc_rwlock_read_rd()
{
	start_wait();
	long i, j;
	uint64_t current;
	j = 0;
	for (i = 1; i <= count; i++) {
#ifdef WIN32
		AcquireSRWLockShared(&srwlock);
		current = flat_uint;
		ReleaseSRWLockShared(&srwlock);
#else
		pthread_rwlock_rdlock(&rwlock);
		current = flat_uint;
		pthread_rwlock_unlock(&rwlock);
#endif
		if (current > 0) {
			j++;
		} else {
			std::cout << "WTF?";
		}
	}
	if (j != count) {
		std::cout << "Not matched count j=" << j << "count=" << count << "\n";
	}
}

void threadfunc_atomic()
{
	start_wait();
	// Wait until main() sends data
	long i;
	for (i = 1; i <= count; i++) {
		atomic_uint++;
	}
}

void threadfunc_atomic_read()
{
	start_wait();
	// Wait until main() sends data
	long i, j;
	uint64_t current;
	j = 0;
	for (i = 1; i <= count; i++) {
		current = atomic_uint;
		if (current > 0) {
			j++;
		} else {
			std::cout << "WTF?";
		}
	}
	if (j != count) {
		std::cout << "Not matched count j=" << j << "count=" << count << "\n";
	}
}

void threadfunc_atomic_set_release()
{
	start_wait();
	// Wait until main() sends data
	long i;
	for (i = 1; i <= count; i++) {
		atomic_uint.store(i, std::memory_order_release);
	}
}

void threadfunc_atomic_set_cst()
{
	start_wait();
	// Wait until main() sends data
	long i;
	for (i = 1; i <= count; i++) {
		atomic_uint.store(i, std::memory_order_seq_cst);
	}
}

void threadfunc_atomic_set_relaxed()
{
	start_wait();
	// Wait until main() sends data
	long i;
	for (i = 1; i <= count; i++) {
		atomic_uint.store(i, std::memory_order_relaxed);
	}
}

void threadfunc_atomic_read_consume()
{
	start_wait();
	// Wait until main() sends data
	long i, j;
	uint64_t current;
	j = 0;
	for (i = 1; i <= count; i++) {
		current = atomic_uint.load(std::memory_order_consume);
		if (current > 0) {
			j++;
		} else {
			std::cout << "WTF?" << errno << " " << i << "\n";
		}
	}
	if (j != count) {
		std::cout << "Not matched count j=" << j << "count=" << count << "\n";
	}
}

void threadfunc_atomic_read_acquire()
{
	start_wait();
	// Wait until main() sends data
	long i, j;
	uint64_t current;
	j = 0;
	for (i = 1; i <= count; i++) {
		current = atomic_uint.load(std::memory_order_acquire);
		if (current > 0) {
			j++;
		} else {
			std::cout << "WTF?";
		}
	}
	if (j != count) {
		std::cout << "Not matched count j=" << j << "count=" << count << "\n";
	}
}

void threadfunc_atomic_weak()
{
	start_wait();
	// Wait until main() sends data
	long i;
	uint64_t next;
	for (i = 1; i <= count; i++) {
		do {
			next = atomic_uint.load(std::memory_order_relaxed);
		} while (!atomic_uint.compare_exchange_weak(next, next + 1,
		                                            std::memory_order_release,
		                                            std::memory_order_relaxed));
	}
}

typedef void (*ThreadFunc)(void);

typedef struct __jobs {
	ThreadFunc Write;
	const char *WriteStr;
	ThreadFunc Read;
	const char *ReadStr;
} jobs;

#define FN(NAME) threadfunc_##NAME

#define JOB(READFUNC, WRITEFUNC) \
	{ \
		FN(READFUNC), #READFUNC, FN(WRITEFUNC), #WRITEFUNC \
	}
void run_benchmark(int type, int numthreads, int factor)
{
	int readerCount = 0;
	int writerCount = 0;
	double ticStart;

	jobs Jobs[] = {JOB(unlocked, unlocked_read),
	               JOB(mutex, mutex_read),
	               JOB(rwlock, rwlock_read_wr),
	               JOB(rwlock, rwlock_read_rd),
	               JOB(atomic, atomic_read),
	               JOB(atomic_set_release, atomic_read_consume),
	               JOB(atomic_set_release, atomic_read_acquire),
	               JOB(atomic_set_cst, atomic_read_consume),
	               JOB(atomic_set_cst, atomic_read_acquire),
	               JOB(atomic_set_relaxed, atomic_read_consume),
	               JOB(atomic_set_relaxed, atomic_read_acquire),
	               JOB(atomic_weak, atomic_read_consume),
	               JOB(atomic_weak, atomic_read_acquire)};

#ifdef WIN32
	InitializeSRWLock(&srwlock);
#endif

	atomic_uint = 1;
	if (type >= sizeof(Jobs) / sizeof(jobs)) {
		std::cout << "type not supported.\n";
		exit(1);
	}

	for (int i = 0; i < numthreads; i++) {
		if (i % factor == 0) {
			writerCount++;
			threads.push_back(std::thread(Jobs[type].Write));
		} else {
			readerCount++;
			threads.push_back(std::thread(Jobs[type].Read));
		}
	}

	// release the threads for processing
	{
		std::lock_guard<std::mutex> lk(m);
		ready = true;
	}
	ticStart = TRI_microtime();
	halt.notify_all();

	// join all the threads
	while (threads.size()) {
		auto t = threads.begin();
		while (t != threads.end()) {
			if (t->joinable()) {
				t->join();
				t = threads.erase(t); // t assigned to the next
				                      // item, which can be ::end
			}
		}
	}
	const auto duration_ms = 1000 * (TRI_microtime() - ticStart);

	printf("%13.3f %5d    %-20s  %-20s\n", duration_ms, type,
	       Jobs[type].ReadStr, Jobs[type].WriteStr);
}

void reset_state()
{
	ready = false;
	theMutex.unlock();
	m.unlock();
	assert(threads.empty());
	flat_uint = 1;
	atomic_uint = 0;
}

// extern void UI_Init(void);
int main(int argc, char *argv[])
{
	if (argc != 4) {
		printf("Usage: NUM-THREADS ITERATIONS READS-to-WRITES-RATIO\n");
		return -1;
	}
	const auto numthreads = atoi(argv[1]);
	count = atol(argv[2]);
	const auto factor = atoi(argv[3]);

	printf("Testing with %d threads over %ld iterations using a %d R/W-ratio ...\n\n",
	       numthreads, count, factor);

	printf("Duration (ms)   Type   Read Mechanism        Write Mechanism\n"
	       "=============   ====   ===================   ==================\n");

	int type = -1;
	while (++type < 13) {
		run_benchmark(type, numthreads, factor);
		reset_state();
	}
	return 0;
}
