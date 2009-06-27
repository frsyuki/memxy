
nodist_noinst_DATA = cclog.h
dist_noinst_DATA = cclog.h.erb

noinst_LIBRARIES = libcclog.a

libcclog_a_SOURCES = \
		cclog.cc \
		cclog_null.cc \
		cclog_ostream.cc \
		cclog_syslog.cc \
		cclog_tty.cc

# FIXME GNU make extension
%.h: %.h.erb
	$(ERB) < $< > $@.tmp
	mv $@.tmp $@

MOSTLYCLEANFILES = cclog.h

noinst_HEADERS = \
		cclog.h \
		cclog_null.h \
		cclog_ostream.h \
		cclog_syslog.h \
		cclog_tty.h

$(libcclog_a_SOURCES): cclog.h

