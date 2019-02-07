-include ../config.mk

VPATH = ..
REGEX_SRC ?= text-regex.c

SRC = array.c buffer.c libutf.c main.c map.c \
	sam.c text.c text-motions.c text-objects.c text-util.c \
	ui-terminal.c view.c vis.c vis-lua.c vis-modes.c vis-motions.c \
	vis-operators.c vis-registers.c vis-marks.c vis-prompt.c vis-text-objects.c $(REGEX_SRC)

ELF = vis vis-menu vis-digraph
EXECUTABLES = $(ELF) vis-clipboard vis-complete vis-open

MANUALS = $(EXECUTABLES:=.1)

DOCUMENTATION = LICENSE README.md

VERSION = $(shell git describe --always --dirty 2>/dev/null || echo "v0.6-git")

CONFIG_HELP ?= 1
CONFIG_CURSES ?= 1
CONFIG_LUA ?= 1
CONFIG_LPEG ?= 0
CONFIG_TRE ?= 0
CONFIG_ACL ?= 0
CONFIG_SELINUX ?= 0

CFLAGS_STD ?= -std=c99 -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -DNDEBUG
CFLAGS_STD += -DVERSION="${VERSION}"
LDFLAGS_STD ?= -lc

CFLAGS_LIBC ?= -DHAVE_MEMRCHR=0

CFLAGS_VIS = $(CFLAGS_AUTO) $(CFLAGS_TERMKEY) $(CFLAGS_CURSES) $(CFLAGS_ACL) \
	$(CFLAGS_SELINUX) $(CFLAGS_TRE) $(CFLAGS_LUA) $(CFLAGS_LPEG) $(CFLAGS_STD) \
	$(CFLAGS_LIBC)

CFLAGS_VIS += -DVIS_PATH="${SHAREPREFIX}/vis"
CFLAGS_VIS += -DCONFIG_HELP=${CONFIG_HELP}
CFLAGS_VIS += -DCONFIG_CURSES=${CONFIG_CURSES}
CFLAGS_VIS += -DCONFIG_LUA=${CONFIG_LUA}
CFLAGS_VIS += -DCONFIG_LPEG=${CONFIG_LPEG}
CFLAGS_VIS += -DCONFIG_TRE=${CONFIG_TRE}
CFLAGS_VIS += -DCONFIG_SELINUX=${CONFIG_SELINUX}
CFLAGS_VIS += -DCONFIG_ACL=${CONFIG_ACL}

LDFLAGS_VIS = $(LDFLAGS_AUTO) $(LDFLAGS_TERMKEY) $(LDFLAGS_CURSES) $(LDFLAGS_ACL) \
	$(LDFLAGS_SELINUX) $(LDFLAGS_LUA) $(LDFLAGS_LPEG) $(LDFLAGS_STD) $(LDFLAGS_TRE)

STRIP?=strip
TAR?=tar
DOCKER?=docker

all: $(ELF)

../config.h: config.def.h
	@echo CP $@; cp $< $@

config.mk:
	@touch $@

${SRC:%.c=%.o}: config.mk

VISCC := ${CC}
CPPFLAGS = -MD

%.o: CC = @echo CC $@; ${VISCC}
main.o: config.h
${SRC:%.c=%.o}: CFLAGS += ${CFLAGS_VIS} ${CFLAGS_EXTRA}

vis: CC = @echo LD $@; ${VISCC}
vis: LDFLAGS += ${LDFLAGS_VIS}
vis: ${SRC:%.c=%.o}

vis-menu.o: CFLAGS += ${CFLAGS_AUTO} ${CFLAGS_STD} ${CFLAGS_EXTRA}
vis-menu: CC = @echo LD $@; ${VISCC}
vis-menu: LDFLAGS += ${LDFLAGS_STD} ${LDFLAGS_AUTO}
vis-menu: vis-menu.o

vis-digraph.o: CFLAGS += ${CFLAGS_AUTO} ${CFLAGS_STD} ${CFLAGS_EXTRA}
vis-digraph: CC = @echo LD $@; ${VISCC}
vis-digraph: LDFLAGS += ${LDFLAGS_STD} ${LDFLAGS_AUTO}
vis-digraph: vis-digraph.o

vis-single-payload.inc: $(EXECUTABLES) lua/*
	for e in $(ELF); do \
		${STRIP} "$$e"; \
	done
	echo '#ifndef VIS_SINGLE_PAYLOAD_H' > $@
	echo '#define VIS_SINGLE_PAYLOAD_H' >> $@
	echo 'static unsigned char vis_single_payload[] = {' >> $@
	$(TAR) --mtime='2014-07-15 01:23Z' --owner=0 --group=0 --numeric-owner --mode='a+rX-w' -c \
		$(EXECUTABLES) $$(find lua -name '*.lua' | LC_ALL=C sort) | xz -T 1 | \
		od -t x1 -v | sed -e 's/^[0-9a-fA-F]\{1,\}//g' -e 's/\([0-9a-f]\{2\}\)/0x\1,/g' >> $@
	echo '};' >> $@
	echo '#endif' >> $@

vis-single: vis-single.c vis-single-payload.inc
	${CC} ${CFLAGS} ${CFLAGS_AUTO} ${CFLAGS_STD} ${CFLAGS_EXTRA} $< ${LDFLAGS} ${LDFLAGS_STD} ${LDFLAGS_AUTO} -luntar -llzma -o $@
	${STRIP} $@

docker-kill:
	-$(DOCKER) kill vis && $(DOCKER) wait vis

docker: docker-kill clean
	$(DOCKER) build -t vis .
	$(DOCKER) run --rm -d --name vis vis tail -f /dev/null
	$(DOCKER) exec vis apk update
	$(DOCKER) exec vis apk upgrade
	$(DOCKER) cp . vis:/build/vis
	$(DOCKER) exec -w /build/vis vis ./configure CC='cc --static' \
		--enable-acl \
		--enable-lua \
		--enable-lpeg-static
	$(DOCKER) exec -w /build/vis vis make VERSION="$(VERSION)" clean vis-single
	$(DOCKER) cp vis:/build/vis/vis-single vis
	$(DOCKER) kill vis

docker-clean: docker-kill clean
	-$(DOCKER) image rm vis

debug: clean
	@$(MAKE) CFLAGS_EXTRA='${CFLAGS_EXTRA} ${CFLAGS_DEBUG}'

profile: clean
	@$(MAKE) CFLAGS_AUTO='' LDFLAGS_AUTO='' CFLAGS_EXTRA='-pg -O2'

coverage: clean
	@$(MAKE) CFLAGS_EXTRA='--coverage'

test-update:
	git submodule init
	git submodule update --remote --rebase

test:
	[ -e test/Makefile ] || $(MAKE) test-update
	@$(MAKE) -C test

testclean:
	@echo cleaning the test artifacts
	[ ! -e test/Makefile ] || $(MAKE) -C test clean

clean:
	@echo cleaning
	@rm -f $(ELF) ${ELF:%=%.o} ${ELF:%=%.d} ${SRC:%.c=%.o} ${SRC:%.c=%.d} vis-single vis-single-payload.inc vis-*.tar.gz *.gcov *.gcda *.gcno

distclean: clean testclean
	@echo cleaning build configuration
	@rm -f config.h config.mk

dist: distclean
	@echo creating dist tarball
	@git archive --prefix=vis-${VERSION}/ -o vis-${VERSION}.tar.gz HEAD

html: ${MANUALS:%=man/%.html}

${MANUALS:%=man/%.html}: man/%.html: man/%
	sed -e "s/VERSION/${VERSION}/" "$<" | mandoc -W warning -T utf8 -T html -O man=%N.%S.html -O style=mandoc.css 1> "$@" || true

luadoc:
	@cd lua/doc && ldoc . && sed -e "s/RELEASE/${VERSION}/" -i index.html

luadoc-all:
	@cd lua/doc && ldoc -a . && sed -e "s/RELEASE/${VERSION}/" -i index.html

luacheck:
	@luacheck --config .luacheckrc lua test/lua | less -RFX

install: ${EXECUTABLES:%=${DESTDIR}${PREFIX}/bin/%} ${MANUALS:%=${DESTDIR}${MANPREFIX}/man1/%} ${DOCUMENTATION:%=${DESTDIR}${DOCPREFIX}/vis/%}

ifneq (${CONFIG_LUA}, 0)

LUA_FILES := $(wildcard ../lua/*.lua ../lua/*/*.lua)
install: ${LUA_FILES:../lua/%=${DESTDIR}${SHAREPREFIX}/vis/%}

${LUA_FILES:../lua/%=${DESTDIR}${SHAREPREFIX}/vis/%}: ${DESTDIR}${SHAREPREFIX}/vis/%: ../lua/% | ${DESTDIR}${SHAREPREFIX}/vis
	install -D -m 0644 "$<" "$@"

endif

${DESTDIR}${BINDIR} ${DESTDIR}${MANPREFIX}/man1 ${DESTDIR}${DOCPREFIX}/vis ${DESTDIR}${SHAREPREFIX}/vis:
	mkdir -p "$@"

${EXECUTABLES:%=${DESTDIR}${BINDIR}/%}: ${DESTDIR}${BINDIR}/%: % | ${DESTDIR}${BINDIR}
	install "$<" "${DESTDIR}${BINDIR}"

${MANUALS:%=${DESTDIR}${MANPREFIX}/man1/%}: ${DESTDIR}${MANPREFIX}/man1/%: man/% | ${DESTDIR}${MANPREFIX}/man1
	sed -e "s/VERSION/${VERSION}/" < "$<" > "$@"

${DOCUMENTATION:%=${DESTDIR}${DOCPREFIX}/vis/%}: ${DESTDIR}${DOCPREFIX}/vis/%: % | ${DESTDIR}${DOCPREFIX}/vis
	install -m 0644 "$<" "$@"

install-strip: install
	@echo stripping executables
	@for e in $(ELF); do \
		${STRIP} ${DESTDIR}${PREFIX}/bin/"$$e"; \
	done

uninstall:
	@echo uninstalling
	@rm -f ${EXECUTABLES:%=${DESTDIR}${PREFIX}/bin/%} ${MANUALS:%=${DESTDIR}${MANPREFIX}/man1/%} ${DOCUMENTATION:%=${DESTDIR}${DOCPREFIX}/vis/%} ${LUA_FILES:../lua/%=${DESTDIR}${SHAREPREFIX}/vis/%}

.PHONY: all clean testclean dist distclean install install-strip uninstall debug profile coverage test test-update luadoc luadoc-all luacheck html docker-kill docker docker-clean

-include ${SRC:%.c=%.d}
