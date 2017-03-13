# st - simple terminal
# See LICENSE file for copyright and license details.

SHELL = /bin/sh
include config.mk

DEBUG ?= 1
ifeq ($(DEBUG),1)
	CPPFLAGS += -g -O0
else
	CPPFLAGS += -DNDEBUG -O3
	CPPFLAGS := $(filter-out -g,$(CPPFLAGS))
endif

TARBALL ?= st-$(VERSION)

all : st

config.h : config.def.h
	clang-format -style=file -i config.def.h
	cp config.def.h config.h

st.o : st.c config.h
	clang-format -style=file -i st.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

st : st.o
	$(CC) $(LDFLAGS) $< -o $@ $(LOADLIBES) $(LDLIBS)

clean :
	rm -f st st.bc st.i st.o st.s $(TARBALL).tar.gz

dist : clean
	mkdir -p $(TARBALL)
	cp -R LICENSE Makefile README.md config.def.h config.mk st.1 st.c st.info $(TARBALL)
	tar -c $(TARBALL) | gzip > $(TARBALL).tar.gz
	rm -rf $(TARBALL)

distclean : clean
	rm -f config.h

format :
	clang-format -style=file -i config.def.h
	clang-format -style=file -i st.c

install : all
	@echo Copying executable file to $(DESTDIR)$(PREFIX)/bin
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp -f st $(DESTDIR)$(PREFIX)/bin
	chmod 755 $(DESTDIR)$(PREFIX)/bin/st
	@echo Copying manual page to $(DESTDIR)$(MANPREFIX)/man1
	mkdir -p $(DESTDIR)$(MANPREFIX)/man1
	sed "s/VERSION/$(VERSION)/g" < st.1 > $(DESTDIR)$(MANPREFIX)/man1/st.1
	chmod 644 $(DESTDIR)$(MANPREFIX)/man1/st.1

terminfo : st.info
	@echo Please see the README file regarding the terminfo entry of st.
	tic -sx st.info

tidy :
	clang-format -style=file -i st.c
	$(CLANG_TIDY_EXECUTABLE) st.c -checks=$(CLANG_TIDY_CHECKS) -fix -- \
	$(CPPFLAGS) $(CFLAGS) $(CLANG_TIDY_FLAGS)

uninstall :
	@echo removing executable file from $(DESTDIR)$(PREFIX)/bin
	rm -f $(DESTDIR)$(PREFIX)/bin/st
	@echo removing manual page from $(DESTDIR)$(MANPREFIX)/man1
	rm -f $(DESTDIR)$(MANPREFIX)/man1/st.1

.PHONY : all clean dist distclean format install terminfo tidy uninstall
