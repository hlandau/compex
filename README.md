compex
======

compex provides plugins for GCC and clang for extracting type information from
C++.

The plugin itself dumps information about structures, fields and methods in
YAML format, allowing you to easily build reflection facilities with C++.

A Python 3 helper script is included to convert to JSON if preferred.

You can also annotate structures, fields and methods with arbitrary tags.

Because compex is a compiler plugin, type information can be dumped as you
compile files, eliminating the need to parse your source code twice and
increase compile times.

Because compex uses the C++ compiler's parsing facilities, it understands all
of C++, unlike tools such as Qt's moc, which can only parse limited subsets of
C++.

compex is a very small codebase which can be easily customized or adapted. It
may also serve as interesting reading for those interested in writing compiler
plugins.

compex is entirely unrelated to C++ RTTI (`dynamic_cast`) and does not require
it in any way.

Building
--------
Run `make`. If you only have one of gcc and clang installed, edit the Makefile
and remove `compex_gcc.so` or `compex_clang.so` (as appropriate) from the `all`
target.

You may need to ensure that the header packages for GCC/clang are installed.

Run `make install` to install. The following are the default installation
parameters, which you may override by passing them to make:

    PREFIX=/usr/local
    BINPATH=$(PREFIX)/bin
    INCPATH=$(PREFIX)/include
    LIBPATH=$(PREFIX)/lib
    DESTDIR=                    # sandbox installation path

You can then run `g++` or `clang++` using the plugin directly:

    g++ -c -fplugin=/usr/local/lib/compex_gcc.so file.cpp
    clang++ -c -Xclang -load -Xclang /usr/local/lib/compex_clang.so \
      -Xclang -plugin -Xclang compex_clang

or, after installation, using `compex-config` (recommended):

    g++ -c `compex-config --gcc` file.cpp

For clang:

    clang++ -c `compex-config --clang` file.cpp

compex output is sent to stdout by default. To redirect it to a file, pass `-o
filename` to `compex-config`:

    g++ -c `compex-config --gcc -o file.info` file.cpp

You can alternatively pass `-fplugin-arg-compex_gcc-o=filename` directly to g++
or `-Xclang -plugin-arg-compex_clang -Xclang -o=filename` to clang++.

If you want to run compex without producing normal object code output, pass
`-S -o /dev/null` to the compiler.

Example Input Programs; Example Output
--------------------------------------
See the `doc/examples` directory for example input programs and their
corresponding output.

Usage
-----
The header file `<compex.h>` provides macros for the use of the compex
attributes. See `compex.h` for details.

Specify `COMPEX_TAG()` on any structure you want metadata to be dumped for. For
example:

    #include <compex.h>

    struct COMPEX_TAG() my_struct {
      int x, y, z;
    };

Metadata will be output for any structure with at least one `COMPEX_TAG()` tag.
The `COMPEX_TAG()` macro expands to nothing when compex is not used.

You can also specify metadata to be attached to a construct using `COMPEX_TAG()`;
simply pass a number of items to `COMPEX_TAG()`. Each should be an integer or a
string literal. For example:

    struct COMPEX_TAG("theAnswer", 42) my_struct {
      // ...
    };

The arguments passed to `COMPEX_TAG()` become entries in a list in the output.
If you specify `COMPEX_TAG()` multiple times for the same element, the
arguments to each such tag are separate lists. A list is not emitted if no
arguments are specified.

For example, if you specified
`COMPEX_TAG() COMPEX_TAG("a", "b", 42) COMPEX_TAG(1, 2, "c")`,
the 'tags' member of the output for the structure would look something like
this:

    tags:
      - - a
        - b
        - 42
      - - 1
        - 2
        - c

You can also use the GCC attributes directly, but this is not recommended:

    struct __attribute__((compex_tag("foo"))) my_struct {
      // ...
    };

compex currently uses the GCC attribute syntax instead of the C++11 attribute
syntax due to what appears to be a bug in GCC, but this is all smoothed over by
`compex.h` anyway.

Extras
------
The barebones script `src/compex-convert` can be used to convert the YAML
output into JSON, if desired, or alternately into a format intended to be
amenable to processing with the C preprocessor.

Colophon
--------
© 2014 Hugo Landau <hlandau@devever.net>

File licenses vary due to the different licenses used by GCC/clang. See the end
of each file for license information.

(Please note that licenses do not affect the output of compex, which can be
used without restriction.)

Send bug reports or patches to <hlandau@devever.net>.
