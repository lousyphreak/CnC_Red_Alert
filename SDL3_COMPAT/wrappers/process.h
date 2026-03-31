#ifndef RA_PROCESS_WRAPPER_H
#define RA_PROCESS_WRAPPER_H

#include "win32_compat.h"

#include <thread>
#include <unistd.h>

using _beginthread_proc_type = void (*)(void*);

inline uintptr_t _beginthread(_beginthread_proc_type start_address, unsigned, void* arglist)
{
	if (!start_address) {
		return static_cast<uintptr_t>(-1);
	}

	std::thread([start_address, arglist]() {
		start_address(arglist);
	}).detach();

	return 0;
}

#endif
