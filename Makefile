PREFIX          = /usr

export PKG_CONFIG_PATH := $(if $(PKG_CONFIG_PATH),$(PKG_CONFIG_PATH):$(PREFIX)/lib/pkgconfig/,$(PREFIX)/lib/pkgconfig)
export PATH := $(PATH):$(PREFIX)/bin

IGNORE_WARNINGS = -Wno-reorder -Wno-sign-compare -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-function -Wno-unknown-pragmas

MKDIR           = mkdir -p
CHK_DIR_EXISTS  = test -d
INSTALL_PROGRAM = install -m 755 -p
DEL_FILE        = rm -f
ICON_SIZES      = 16x16 32x32 64x64 128x128 256x256
SUDO            = `which sudo`

WXWIDGETS_TAG=xlights_2026.04

.NOTPARALLEL:

all: wxwidgets33 cbp2make makefile xSchedule xSMSDaemon RemoteFalcon

#############################################################################

xSchedule: FORCE
	@${MAKE} -C xSchedule -f xSchedule.cbp.mak OBJDIR_LINUX_DEBUG=".objs_debug" linux_release

xSMSDaemon: FORCE
	@${MAKE} -C xSchedule/xSMSDaemon -f xSMSDaemon.cbp.mak OBJDIR_LINUX_DEBUG=".objs_debug" linux_release

RemoteFalcon: FORCE
	@${MAKE} -C xSchedule/RemoteFalcon -f RemoteFalcon.cbp.mak OBJDIR_LINUX_DEBUG=".objs_debug" linux_release

#############################################################################

debug: makefile xSchedule_debug

xSchedule_debug:
	@${MAKE} -C xSchedule -f xSchedule.cbp.mak OBJDIR_LINUX_DEBUG=".objs_debug" linux_debug

#############################################################################

clean:
	@if test -f xSchedule/xSchedule.cbp.mak; then \
		${MAKE} -C xSchedule -f xSchedule.cbp.mak OBJDIR_LINUX_DEBUG=".objs_debug" clean; \
		$(DEL_FILE) xSchedule/xSchedule.cbp.mak xSchedule/xSchedule.cbp.mak.orig; \
	fi
	@if test -f xSchedule/xSMSDaemon/xSMSDaemon.cbp.mak; then \
		${MAKE} -C xSchedule/xSMSDaemon -f xSMSDaemon.cbp.mak OBJDIR_LINUX_DEBUG=".objs_debug" clean; \
		$(DEL_FILE) xSchedule/xSMSDaemon/xSMSDaemon.cbp.mak xSchedule/xSMSDaemon/xSMSDaemon.cbp.mak.orig; \
	fi
	@if test -f xSchedule/RemoteFalcon/RemoteFalcon.cbp.mak; then \
		${MAKE} -C xSchedule/RemoteFalcon -f RemoteFalcon.cbp.mak OBJDIR_LINUX_DEBUG=".objs_debug" clean; \
		$(DEL_FILE) xSchedule/RemoteFalcon/RemoteFalcon.cbp.mak xSchedule/RemoteFalcon/RemoteFalcon.cbp.mak.orig; \
	fi

#############################################################################

install:
	@$(CHK_DIR_EXISTS) $(DESTDIR)/${PREFIX}/bin || $(MKDIR) $(DESTDIR)/${PREFIX}/bin
	-$(INSTALL_PROGRAM) -D bin/xSchedule $(DESTDIR)/${PREFIX}/bin/xSchedule
	-$(INSTALL_PROGRAM) -D bin/xSMSDaemon.so $(DESTDIR)/${PREFIX}/bin/xSMSDaemon.so
	-$(INSTALL_PROGRAM) -D bin/RemoteFalcon.so $(DESTDIR)/${PREFIX}/bin/RemoteFalcon.so
	-$(INSTALL_PROGRAM) -D bin/xschedule.desktop $(DESTDIR)/${PREFIX}/share/applications/xschedule.desktop
	-$(INSTALL_PROGRAM) -D bin/xsmsdaemon.desktop $(DESTDIR)/${PREFIX}/share/applications/xsmsdaemon.desktop
	install -d -m 755 $(DESTDIR)/${PREFIX}/share/xSchedule/xScheduleWeb
	cp -r bin/xScheduleWeb/* $(DESTDIR)/${PREFIX}/share/xSchedule/xScheduleWeb
	$(foreach size, $(ICON_SIZES), install -D -m 644 images/icons/$(size).png $(DESTDIR)/${PREFIX}/share/icons/hicolor/$(size)/apps/xschedule.png ; )

uninstall:
	-$(DEL_FILE) $(DESTDIR)/${PREFIX}/bin/xSchedule
	-$(DEL_FILE) $(DESTDIR)/${PREFIX}/bin/xSMSDaemon.so
	-$(DEL_FILE) $(DESTDIR)/${PREFIX}/bin/RemoteFalcon.so
	-$(DEL_FILE) $(DESTDIR)/${PREFIX}/share/applications/xschedule.desktop
	-$(DEL_FILE) $(DESTDIR)/${PREFIX}/share/applications/xsmsdaemon.desktop
	$(foreach size, $(ICON_SIZES), $(DEL_FILE) $(DESTDIR)/${PREFIX}/share/icons/hicolor/$(size)/apps/xschedule.png ; )

#############################################################################

wxwidgets33: FORCE
	@printf "Checking wxwidgets\n"
	@if test -n "`wx-config --version 2>/dev/null`"; then \
		echo "wxWidgets already installed: `wx-config --version`"; \
	elif test ! -d wxWidgets-$(WXWIDGETS_TAG); then \
		echo Downloading wxwidgets; \
		git clone --depth=1 --shallow-submodules --recurse-submodules -b $(WXWIDGETS_TAG) https://github.com/xLightsSequencer/wxWidgets wxWidgets-$(WXWIDGETS_TAG); \
		cd wxWidgets-$(WXWIDGETS_TAG); \
		./configure --enable-cxx11 --with-cxx=17 --enable-std_containers --enable-std_string_conv_in_wxstring --enable-backtrace --enable-exceptions --enable-mediactrl --enable-graphics_ctx --enable-monolithic --disable-sdltest --with-gtk=3 --disable-pcx --disable-iff --without-libtiff --enable-utf8 --enable-utf8only --prefix=$(PREFIX); \
		echo Building wxwidgets; \
		${MAKE} -j 4 -s; \
		echo Installing wxwidgets; \
		$(SUDO) ${MAKE} install DESTDIR=$(DESTDIR); \
		echo Completed build/install of wxwidgets; \
	fi

cbp2make:
	@if test -n "`cbp2make --version`"; then \
		$(DEL_FILE) xSchedule/xSchedule.cbp.mak; \
		$(DEL_FILE) xSchedule/xSMSDaemon/xSMSDaemon.cbp.mak; \
		$(DEL_FILE) xSchedule/RemoteFalcon/RemoteFalcon.cbp.mak; \
	fi

makefile: xSchedule/xSchedule.cbp.mak xSchedule/xSMSDaemon/xSMSDaemon.cbp.mak xSchedule/RemoteFalcon/RemoteFalcon.cbp.mak

xSchedule/xSchedule.cbp.mak: xSchedule/xSchedule.cbp
	@cbp2make -in xSchedule/xSchedule.cbp -cfg xlights/cbp2make.cfg -out xSchedule/xSchedule.cbp.mak \
			--with-deps --keep-outdir --keep-objdir
	@cp xSchedule/xSchedule.cbp.mak xSchedule/xSchedule.cbp.mak.orig
	@cat xSchedule/xSchedule.cbp.mak.orig \
		| sed \
			-e "s/CFLAGS_LINUX_RELEASE = \(.*\)/CFLAGS_LINUX_RELEASE = \1 $(IGNORE_WARNINGS)/" \
			-e "s/OBJDIR_LINUX_DEBUG = \(.*\)/OBJDIR_LINUX_DEBUG = .objs_debug/" \
		> xSchedule/xSchedule.cbp.mak

xSchedule/xSMSDaemon/xSMSDaemon.cbp.mak: xSchedule/xSMSDaemon/xSMSDaemon.cbp
	@cbp2make -in xSchedule/xSMSDaemon/xSMSDaemon.cbp -cfg xlights/cbp2make.cfg -out xSchedule/xSMSDaemon/xSMSDaemon.cbp.mak \
			--with-deps --keep-outdir --keep-objdir
	@cp xSchedule/xSMSDaemon/xSMSDaemon.cbp.mak xSchedule/xSMSDaemon/xSMSDaemon.cbp.mak.orig
	@cat xSchedule/xSMSDaemon/xSMSDaemon.cbp.mak.orig \
		| sed \
			-e "s/CFLAGS_LINUX_RELEASE = \(.*\)/CFLAGS_LINUX_RELEASE = \1 $(IGNORE_WARNINGS)/" \
			-e "s/OBJDIR_LINUX_DEBUG = \(.*\)/OBJDIR_LINUX_DEBUG = .objs_debug/" \
		> xSchedule/xSMSDaemon/xSMSDaemon.cbp.mak

xSchedule/RemoteFalcon/RemoteFalcon.cbp.mak: xSchedule/RemoteFalcon/RemoteFalcon.cbp
	@cbp2make -in xSchedule/RemoteFalcon/RemoteFalcon.cbp -cfg xlights/cbp2make.cfg -out xSchedule/RemoteFalcon/RemoteFalcon.cbp.mak \
			--with-deps --keep-outdir --keep-objdir
	@cp xSchedule/RemoteFalcon/RemoteFalcon.cbp.mak xSchedule/RemoteFalcon/RemoteFalcon.cbp.mak.orig
	@cat xSchedule/RemoteFalcon/RemoteFalcon.cbp.mak.orig \
		| sed \
			-e "s/CFLAGS_LINUX_RELEASE = \(.*\)/CFLAGS_LINUX_RELEASE = \1 $(IGNORE_WARNINGS)/" \
			-e "s/OBJDIR_LINUX_DEBUG = \(.*\)/OBJDIR_LINUX_DEBUG = .objs_debug/" \
		> xSchedule/RemoteFalcon/RemoteFalcon.cbp.mak

#############################################################################

FORCE:
