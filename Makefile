HOST_GCC=g++
TARGET_GCC=g++
GCCPLUGINS_DIR := $(shell $(TARGET_GCC) -print-file-name=plugin)
CXXFLAGS += -g -fno-rtti -O3
PLUGIN_CXXFLAGS=$(CXXFLAGS) -I$(GCCPLUGINS_DIR)/include -fPIC
PREFIX=/usr/local
BINPATH=$(PREFIX)/bin
INCPATH=$(PREFIX)/include
LIBPATH=$(PREFIX)/lib
DESTDIR=
BUILDDIR=build

.PHONY: clean all install dummy

all: $(BUILDDIR)/compex_gcc.so

install: all $(BUILDDIR)/compex-config
	install        $(BUILDDIR)/compex_gcc.so $(DESTDIR)$(LIBPATH)
	install -m 755 $(BUILDDIR)/compex-config $(DESTDIR)$(BINPATH)
	install -m 755 src/compex-convert $(DESTDIR)$(BINPATH)
	install        include/compex.h $(DESTDIR)$(INCPATH)

clean:
	rm -rf $(BUILDDIR)

dummy:

$(BUILDDIR)/compex-config: src/compex-config.in $(BUILDDIR) dummy
	sed 's#@LIBPATH@#$(LIBPATH)#g' < "$<" > "$@"

$(BUILDDIR)/compex_gcc.so: src/compex_gcc.cpp $(BUILDDIR)
	$(HOST_GCC) -shared $(PLUGIN_CXXFLAGS) $< -o $@

$(BUILDDIR):
	mkdir -p "$@"
