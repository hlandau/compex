HOST_GCC=g++
TARGET_GCC=g++
HOST_CLANG=clang++
TARGET_CLANG=clang++
GCCPLUGINS_DIR := $(shell $(TARGET_GCC) -print-file-name=plugin)
CXXFLAGS += -g -fno-rtti -std=gnu++14 -O3
PLUGIN_CXXFLAGS=$(CXXFLAGS) -fPIC
PREFIX=/usr/local
BINPATH=$(PREFIX)/bin
INCPATH=$(PREFIX)/include
LIBPATH=$(PREFIX)/lib
DESTDIR=
BUILDDIR=build

.PHONY: clean all install dummy

all: $(BUILDDIR)/compex_gcc.so $(BUILDDIR)/compex_clang.so

install: all $(BUILDDIR)/compex-config
	install        $(BUILDDIR)/compex_gcc.so $(DESTDIR)$(LIBPATH)
	install        $(BUILDDIR)/compex_clang.so $(DESTDIR)$(LIBPATH)
	install -m 755 $(BUILDDIR)/compex-config $(DESTDIR)$(BINPATH)
	install -m 755 src/compex-convert $(DESTDIR)$(BINPATH)
	install        include/compex.h $(DESTDIR)$(INCPATH)

clean:
	rm -rf $(BUILDDIR)

dummy:

$(BUILDDIR)/compex-config: src/compex-config.in $(BUILDDIR) dummy
	sed 's#@LIBPATH@#$(LIBPATH)#g' < "$<" > "$@"

$(BUILDDIR)/compex_gcc.so: src/compex_gcc.cpp $(BUILDDIR)
	$(HOST_GCC) -shared $(PLUGIN_CXXFLAGS) -I$(GCCPLUGINS_DIR)/include $< -o $@

$(BUILDDIR)/compex_clang.so: src/compex_clang.cpp $(BUILDDIR)
	$(HOST_CLANG) -shared -s \
		$(PLUGIN_CXXFLAGS) $< -o $@ \
		-fvisibility=hidden -fvisibility-inlines-hidden -fno-exceptions

$(BUILDDIR):
	mkdir -p "$@"
