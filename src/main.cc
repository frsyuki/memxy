//
// memxy - memcached proxy
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
#include "proxy_client.h"
#include "gate_memtext.h"
#include "gate_control.h"
#include <cclog/cclog.h>
#include <cclog/cclog_tty.h>
#include <cclog/cclog_ostream.h>
#include <stdio.h>
#include <getopt.h>
#include <fstream>

static const char* s_progname;
static unsigned short s_text_port = 11211;
static unsigned short s_ctl_port  = 11001;
static const char* s_init_servers = NULL;

static const char* s_pidfile = NULL;
static const char* s_logfile = NULL;

static cclog::level s_loglevel = cclog::WARN;

static void usage(const char* msg)
{
	printf("Usage: %s [options]  [initial server list]\n"
		" -t PORT=11211      : proxy port (text)\n"
		" -c PORT=11001      : control port\n"
		" -h                 : print this help message\n"
		" -o <path.log>      : log file\n"
		" -d <path.pid>      : daemonize and output pid into the file\n"
		" -v                 : verbose mode\n"
		, s_progname);
	if(msg) { printf("error: %s\n", msg); }
	exit(1);
}

static void parse_argv(int argc, char* argv[])
{
	int c;
	s_progname = argv[0];
	while((c = getopt(argc, argv, "t:c:o:d:vh")) != -1) {
		switch(c) {
		case 't':
			s_text_port = atoi(optarg);
			if(s_text_port == 0) { usage("-t: invalid port number"); }
			break;

		case 'c':
			s_ctl_port = atoi(optarg);
			if(s_ctl_port == 0) { usage("-c: invalid port number"); }
			break;

		case 'o':
			s_logfile = optarg;
			break;

		case 'd':
			s_pidfile = optarg;
			break;

		case 'v':
			s_loglevel = cclog::TRACE;
			break;

		case 'h':
		case '?':
		default:
			usage(NULL);
		}
	}

	argc -= optind;

	switch(argc) {
	case 1:
		s_init_servers = argv[optind];
		break;

	case 0:
		break;

	default:
		usage(NULL);
	}

}

int main(int argc, char* argv[])
{
	parse_argv(argc, argv);

	if(s_logfile) {
		if(strcmp(s_logfile, "-") == 0) {
			cclog::reset(new cclog_ostream(s_loglevel, std::cout));
		} else {
			std::ostream* logstream = new std::ofstream(s_logfile, std::ios::app);
			cclog::reset(new cclog_ostream(s_loglevel, *logstream));
		}
	} else if(s_pidfile) {
		cclog::reset(new cclog_ostream(s_loglevel, std::cout));
	} else {
		cclog::reset(new cclog_tty(s_loglevel, std::cout));
	}

	using namespace memxy;

	service::init();
	proxy_client::init();

	gate_memtext memtext;
	gate_control control;

	{
		struct sockaddr_in addr;
		memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = htonl(INADDR_ANY);
		addr.sin_port = htons(s_text_port);
	
		memtext.listen(AF_INET, SOCK_STREAM, 0,
				(struct sockaddr*)&addr, sizeof(addr));
	}

	{
		struct sockaddr_in addr;
		memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = htonl(INADDR_ANY);
		addr.sin_port = htons(s_ctl_port);
	
		control.listen(AF_INET, SOCK_STREAM, 0,
				(struct sockaddr*)&addr, sizeof(addr));
	}

	if(s_pidfile) {
		service::daemonize(s_pidfile);
	}

	service::start(8, &proxy_client::thread_init_func, NULL);

	if(s_init_servers) {
		proxy_client::set_servers(s_init_servers);
	}

	service::join();
}

