CLEANFILES = *~
MAINTAINERCLEANFILES = \
	Makefile.in \
	stamp-*

lib_LTLIBRARIES = libpoloka-psf.la

src_includedir = $(includedir)/poloka
src_include_HEADERS = \
	analyticpsf.h \
	imagepsf.h \
	nonlinmodel.h \
	psfstar.h

libpoloka_psf_la_SOURCES = \
	$(src_include_HEADERS) \
	analyticpsf.cc \
	imagepsf.cc \
	nonlinmodel.cc \
	psfstar.cc

libpoloka_psf_la_CPPFLAGS = @POLOKA_CORE_CFLAGS@

libpoloka_psf_la_LIBADD = @POLOKA_CORE_LIBS@
