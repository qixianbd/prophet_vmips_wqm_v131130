SHELL = /bin/bash

srcdir = .
top_srcdir = ../..

prefix = /usr/local
exec_prefix = ${prefix}

bindir = ${exec_prefix}/bin
sbindir = ${exec_prefix}/sbin
libexecdir = ${exec_prefix}/libexec
datadir = ${prefix}/share
sysconfdir = ${prefix}/etc
sharedstatedir = NONE/com
localstatedir = ${prefix}/var
libdir = ${exec_prefix}/lib
infodir = ${prefix}/share/info
mandir = ${prefix}/share/man
includedir = ${prefix}/include
oldincludedir = /usr/include

DESTDIR =

pkgdatadir = $(datadir)/prophet_vmips
pkglibdir = $(libdir)/prophet_vmips
pkgincludedir = $(includedir)/prophet_vmips

top_builddir = ../..

INSTALL = /usr/bin/install -c
INSTALL_PROGRAM = ${INSTALL} $(AM_INSTALL_PROGRAM_FLAGS)
INSTALL_DATA = ${INSTALL} -m 644
INSTALL_SCRIPT = ${INSTALL}
mkinstalldirs = $(SHELL) $(top_srcdir)/mkinstalldirs

VMIPSTOOL = $(top_builddir)/vmipstool -n -F $(top_builddir)/vmipsrc
CC=gcc
CFLAGS=$(CPPFLAGS) -g -G 0 -Wall
HOST_CFLAGS=$(CPPFLAGS) -g -Wall
CPPFLAGS=-I$(top_srcdir) -I$(top_srcdir)/sample_code
OBJS=$(COBJS) $(AOBJS)
SETUP=../setup.o

DISTFILES= Makefile.in README boot.c coff.c coff.h \
 lib.c lib.h libtest.c libtest.input libtest.output loadtest.c \
 loadtest.script loadtest_setup.S pftest.c serial.c xmrcv.c xmrcv.h \
 boot.script boot_setup.S bootenv.c bootenv.h \
 catrom panic.c serial.c serial.h set_status.h set_status.S testbootenv.c \
 vmips16M pftest.output runtests testbootenv.output

all: boot.rom

romkernel: $(KERNEL) catrom boot.rom
	./catrom -a -n -v boot.rom $(KERNEL) boot$(KERNEL).rom

kernelrom: $(KERNEL) catrom boot.rom
	./catrom -b -n -v boot.rom $(KERNEL) boot$(KERNEL).rom

install: boot.rom
	$(mkinstalldirs) $(pkglibdir)
	$(INSTALL_DATA) boot.rom $(pkglibdir)/boot.rom

LIBGCC=$(shell $(VMIPSTOOL) --compile --print-libgcc-file-name)

loadtest: loadtest_setup.o loadtest.o lib.o serial.o $(LIBGCC)
	-$(VMIPSTOOL) --ld-script=$(srcdir)/loadtest.script --link -o $@ $+

%.rom: %.exe
	$(VMIPSTOOL) --make-rom $< $@

boot.exe: boot_setup.o boot.o coff.o lib.o xmrcv.o serial.o panic.o set_status.o bootenv.o
	$(VMIPSTOOL) --ld-script=$(srcdir)/boot.script --link -o $@ $+

%.o: %.c
	$(VMIPSTOOL) --compile $(CFLAGS) -c -o $@ $<

%.o: %.s
	$(VMIPSTOOL) --assemble -o $@ $<

%.o: %.S
	$(VMIPSTOOL) --assemble $(CFLAGS) -o $@ $<

clean:
	-rm -f *.rom *.exe *.o core loadtest libtest testbootenv pftest

distclean: clean

maintainer-clean: distclean

libtest: libtest.host.o lib.host.o
	$(CC) $(HOST_CFLAGS) -o $@ $+

testbootenv: bootenv.host.o testbootenv.host.o
	$(CC) $(HOST_CFLAGS) -o $@ $+

pftest: pftest.host.o lib.host.o
	$(CC) $(HOST_CFLAGS) -o $@ $+

%.host.o: %.c
	$(CC) $(HOST_CFLAGS) -c -o $@ $<

# Snarfed from automake and modified.
distdir: $(DISTFILES)
	-rm -rf $(distdir)
	mkdir $(distdir)
	-chmod 777 $(distdir)
	here=`cd $(top_builddir) && pwd`; \
	top_distdir=`cd $(distdir) && pwd`; \
	distdir=`cd $(distdir) && pwd`; \
	for file in $(DISTFILES); do \
	  d=$(srcdir); \
	  if test -d $$d/$$file; then \
	    cp -pr $$d/$$file $(distdir)/$$file; \
	  else \
	    test -f $(distdir)/$$file \
	    || ln $$d/$$file $(distdir)/$$file 2> /dev/null \
	    || cp -p $$d/$$file $(distdir)/$$file || :; \
	  fi; \
	done

