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
#include "wavy_core.h"
#include <mp/pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

namespace memxy {
namespace service {


static bool signal_handler(int signo)
{
	end();
	return true;
}

void init()
{
	core::init();

	if(signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
		throw mp::system_error(errno, "failed to set SIGPIPE handler");
	}

	core::signal_event(SIGHUP,  &signal_handler);
	core::signal_event(SIGINT,  &signal_handler);
	core::signal_event(SIGTERM, &signal_handler);
}

void daemonize(const char* pidfile, const char* stdio)
{
	pid_t pid;
	pid = fork();
	if(pid < 0) { perror("fork"); exit(1); }
	if(pid != 0) { exit(0); }
	if(setsid() == -1) { perror("setsid"); exit(1); }
	pid = fork();
	if(pid < 0) { perror("fork"); exit(1); }
	if(pid != 0) { exit(0); }
	if(pidfile) {
		FILE* f = fopen(pidfile, "w");
		if(!f) { perror("can't open pid file"); exit(1); }
		fprintf(f, "%d", getpid());
		fclose(f);
	}
	if(stdio) {
		int r = open(stdio, O_RDONLY);
		if(r < 0) { perror(stdio); exit(1); }
		int a = open(stdio, O_APPEND);
		if(a < 0) { perror(stdio); exit(1); }
		close(0);
		close(1);
		close(2);
		if(dup2(r, 0) < 0) { perror("dup2"); exit(1); }
		if(dup2(a, 1) < 0) { perror("dup2"); exit(1); }
		if(dup2(a, 2) < 0) { perror("dup2"); exit(1); }
		close(r);
		close(a);
	}
}

void start(size_t num_threads, void (*init)(void*), void* user)
{
	if(init) {
		core::add_thread(num_threads, init, user);
	} else {
		core::add_thread(num_threads);
	}
}

void step_next()
{
	core::step_next();
}

void join()
{
	core::join();
}


void end()
{
	core::end();
}

bool is_end()
{
	return core::is_end();
}


}  // namespace service
}  // namespace memxy

