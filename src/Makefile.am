export RUBY
export RAGEL

SUBDIRS = mp mpsrc cclog

bin_PROGRAMS = memxy
bin_SCRIPTS = memxyctl

memxy_SOURCES = \
		memproto/memproto.c \
		memproto/memtext.c \
		gate_control.cc \
		gate_memtext.cc \
		gate_memtext_impl.cc \
		gate_memtext_retrieval.cc \
		gate_memtext_storage.cc \
		gate_memtext_delete.cc \
		proxy_client.cc \
		wavy_core.cc \
		main.cc

noinst_HEADERS = \
		memproto/memproto.h \
		memproto/memtext.h \
		gate_control.h \
		gate_memtext.h \
		gate_memtext_impl.h \
		gate_memtext_retrieval.h \
		gate_memtext_storage.h \
		gate_memtext_delete.h \
		proxy_client.h \
		wavy_core.h

memxy_LDADD = \
		cclog/libcclog.a \
		mpsrc/libmpio.a

memproto/memtext.c: memproto/memtext.rl
	$(RAGEL) -C $< -o $@.tmp
	mv $@.tmp $@

EXTRA_DIST = \
		memproto/memtext.rl

MOSTLYCLEANFILES = \
		memproto/memtext.c

