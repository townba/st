# st - simple terminal
# See LICENSE file for copyright and license details.

SHELL = /bin/sh
include config.mk

all: debug

config.h : config.def.h
	cp config.def.h config.h

st.o : config.h

clean:
	rm -f st st.bc st.i st.o st.s st-${VERSION}.tar.gz

debug: CPPFLAGS += -g -O0
debug: LDFLAGS += -g
debug: st

dist: clean
	@echo creating dist tarball
	mkdir -p st-${VERSION}
	cp -R LICENSE Makefile README.md config.mk config.def.h st.info st.1 st.c st-${VERSION}
	tar -cf st-${VERSION}.tar st-${VERSION}
	gzip st-${VERSION}.tar
	rm -rf st-${VERSION}

format:
	clang-format -style=file -i st.c

install: all terminfo
	@echo installing executable file to ${DESTDIR}${PREFIX}/bin
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -f st ${DESTDIR}${PREFIX}/bin
	chmod 755 ${DESTDIR}${PREFIX}/bin/st
	@echo installing manual page to ${DESTDIR}${MANPREFIX}/man1
	mkdir -p ${DESTDIR}${MANPREFIX}/man1
	sed "s/VERSION/${VERSION}/g" < st.1 > ${DESTDIR}${MANPREFIX}/man1/st.1
	chmod 644 ${DESTDIR}${MANPREFIX}/man1/st.1
	@echo Please see the README file regarding the terminfo entry of st.
	tic -sx st.info

release: CPPFLAGS += -DNDEBUG -O3
release: CPPFLAGS := $(filter-out -g,$(CPPFLAGS))
release: st

terminfo: st.info
	tic -sx st.info

tidy:
	${CLANG_TIDY_EXECUTABLE} st.c -checks=${CLANG_TIDY_CHECKS} -fix -- \
	${CPPFLAGS} ${CFLAGS} ${CLANG_TIDY_FLAGS}

uninstall:
	@echo removing executable file from ${DESTDIR}${PREFIX}/bin
	rm -f ${DESTDIR}${PREFIX}/bin/st
	@echo removing manual page from ${DESTDIR}${MANPREFIX}/man1
	rm -f ${DESTDIR}${MANPREFIX}/man1/st.1

.PHONY: all clean debug dist format install release tidy uninstall
