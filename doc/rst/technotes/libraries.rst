.. _readme-libraries:

========================================
Calling Chapel Code from Other Languages
========================================

.. note::

   The features described in this document are still under development.
   If you encounter a bug or limitation not yet documented as a `Github
   issue <https://github.com/chapel-lang/chapel/issues>`_, consider filing
   an issue as described in :ref:`readme-bugs`.

To build a Chapel program as a library, compile with the ``--library`` flag.
Without this flag, Chapel assumes that you are building a main program and
produces a main routine, whether one is explicitly defined or not.

.. contents::

Static and Dynamic Libraries
============================

The type of library produced can be specified through the ``--static`` and
``--dynamic`` flags.  If neither ``--static`` nor ``--dynamic`` is specified, a
platform-dependent default library type is produced.

Some platforms support linking against both static and dynamic versions of
the same library.  On those platforms, the ``--static`` or ``--dynamic``
flag can be used to select which type of library (and thus which kind of
linking) is performed by default.  Library files which are named explicitly on
the ``chpl`` command line take precedence over any found through object
library paths (``-L``).  When there is a conflict, the last library
specified takes precedence.

.. note::
   When building a dynamic library, building position independent code is
   recommended.  To do this, set the environment variable ``CHPL_LIB_PIC`` to
   ``pic`` and ensure this configuration is built by performing a ``make``
   command from ``$CHPL_HOME``.  Note that position independent code will likely
   encounter some performance degradation as opposed to normal Chapel code.
   For this reason, we recommend only using ``CHPL_LIB_PIC`` when position
   independent code is required.

.. _Location of the Generated Library:

Location of the Generated Library
=================================

The library will be placed by default in a sub-directory named ``lib`` (which
will be created if it does not already exist).  The location for the generated
library and associated files can be changed using the compilation flag
``--library-dir``::

  # Library built into bar/libfoo.a
  chpl --library --library-dir=bar foo.chpl

How to Define Your Library
==========================

When creating a library file from Chapel code, only those symbols with
``export`` attached to them will be available from outside the library.  For
example, one can define a Chapel file ``foo.chpl`` like this:

.. code-block:: Chapel

   // This function will be available to the library user
   export proc bar(): int {
     // Does something
     ...
   }

   // As will this one
   export proc baz(x: int) {
     // Does something different
     ...
   }

   // but this function will not be, though it can be used by the exported
   // functions
   proc gloop() {
     // Does something else
     ...
   }

See :ref:`Exporting Symbols` for the current limitations on what can be
exported.

.. _Library Name:

Library Name
============

The generated library name will be the same as the file being compiled, except
it will start with ``lib`` if the name does not already, and it will be followed
by a ``.so``/``.dylib`` or ``.a`` suffix.  Thus, in the example above, the generated
library will be named ``libfoo.so``/``libfoo.dylib`` or ``libfoo.a``.

.. code-block:: bash

   # Builds library as lib/libfoo.a
   chpl --library --static foo.chpl

   # Builds library as lib/libfoo.so (On MacOS, lib/libfoo.dylib)
   chpl --library --dynamic foo.chpl

   # Builds library as lib/libfoo.so (On MacOS, lib/libfoo.dylib) (note: file named libfoo.chpl)
   chpl --library --dynamic libfoo.chpl

The basename used (the ``foo`` portion) can be changed with the ``-o`` or
``--output`` compilation flag.

This flag is required if multiple top level modules or files are being compiled
into the same library, as the default name is determined by the top-most module.

.. code-block:: bash

   # Builds library as lib/libbar.so
   chpl --library --dynamic foo.chpl -o bar

   # -o flag required because of multiple modules
   # Builds library as lib/libfoo.so
   chpl --library --dynamic foo.chpl bar.chpl -o foo

Using Your Library in C
=======================

The Header File
---------------

A header file will be generated for the library by default, using the same base
name as the library (replacing ``.so`` or ``.a`` with ``.h`` and omitting the
``lib`` portion).  This name can be changed independently of the generated
library name using the flag ``--library-header`` at compilation.

.. code-block:: bash

   # Builds header as lib/foo.h
   chpl --library --dynamic foo.chpl

   # Builds header as lib/bar.h, library is still lib/libfoo.so
   chpl --library --dynamic --library-header=bar foo.chpl

The header file will contain any exported function, including the exported
module initialization functions (which are generated by default).  It will also
contain a ``#include`` for ``stdchpl.h`` and any ``.h`` files specified in the
program via a ``require`` clause.

.. _Initializing Your Library In C:

Initializing Your Library
-------------------------

When using a Chapel library from C, one must first initialize the Chapel runtime
and standard modules.  This is done by calling the function
``chpl_library_init()`` before the Chapel library function calls and by calling
``chpl_library_finalize()`` after all the Chapel library function calls are
finished.  These functions are defined in
``$CHPL_HOME/runtime/include/chpl-init.h`` and accessible when you ``#include``
the generated header file:

.. code-block:: C

   void chpl_library_init(int argc, char* argv[]);
   void chpl_library_finalize(void);

Here is an example program which uses the ``foo`` library:

.. code-block:: C

   #include "foo.h"

   int main(int argc, char* argv[]) {
       chpl_library_init(argc, argv);

       baz(7); // Call into a library function

       chpl_library_finalize();

       return 0;
   }

If your exported functions rely upon any global variables defined in your module
(or the modules it relies upon), then you must additionally call the generated
module initialization function.  This function will be named
``chpl__init_<moduleName>``, and you can find its declaration in your generated
``.h`` file.

At present, the generated module initialization function takes two arguments,
``int64_t _ln`` and ``int32_t _fn``.  These correspond to "line number" and
"file number", respectively.  The values passed to them are used by the Chapel
runtime when providing error messages, but do not matter in this context.  In
the future, they may not be included at all when compiling into a library.  For
now, feel free to pass any valid number to them.

.. note::

   It is recommended that you always call the module initialization function
   before calling any of the exported functions in your library.  You do not
   need to do this more than once per program.

.. _readme-libraries-linking:

Compiling C Code with the Library
---------------------------------

When using a Chapel library file in C code, a fairly exact incantation is
required.  If compiling dynamically, update the ``$LD_LIBRARY_PATH`` environment
variable to include the directory where the new library file lives and the
directory where the Chapel build lives.  The latter can be found by looking at
the output of a ``$CHPL_HOME/util/printchplenv`` call and finding the
appropriate directory under ``$CHPL_HOME/lib``; the directory name can be found
by running ``$CHPL_HOME/util/printchplenv --runtime --path``.

.. code-block:: sh

   # Replace the first lib with the appropriate path to your library file if its
   # location has been changed by --library-dir, or if you are not in its parent
   # directory
   export LD_LIBRARY_PATH=lib/:$CHPL_HOME/lib/`$CHPL_HOME/util/printchplenv --runtime --path`:$LD_LIBRARY_PATH

.. _Makefile Helper:

Makefile Helper
~~~~~~~~~~~~~~~

Compilation of the C program involves some additional command line includes and
links.  For your convenience, a sample Makefile can be generated using
``--library-makefile``.  This will generate a file named
``Makefile.<basename>``:

.. code-block:: bash

   # Builds makefile as lib/Makefile.foo
   chpl --library --dynamic --library-makefile foo.chpl

   # Builds makefile as lib/Makefile.bar
   chpl --library --dynamic --library-makefile foo.chpl -o bar

This Makefile can then be included and its variables referenced in your own
Makefile.

The generated Makefile will contain the user-facing and internal variables.  The
user-facing variables intended for use in your own Makefile are:

- ``CHPL_CFLAGS`` contains the flags and ``-I`` directories needed at compile
  time.
- ``CHPL_LDFLAGS`` contains the ``-L`` directories and ``-l`` libraries needed
  at link time, including libraries specified by your program via ``require``
  statements.
- ``CHPL_COMPILER`` stores the compiler used when compiling your library.  Using
  a different compiler when linking to your library from another code may cause
  ABI incompatibility issues or problems when the flags specified in
  ``CHPL_CFLAGS`` are not applicable in that compiler.
- ``CHPL_LINKER`` and ``CHPL_LINKERSHARED`` store linker commands.

The internal variables support those others in an attempt to make their contents
slightly more readable.

An example Makefile which uses the generated ``Makefile.foo`` looks like this:

.. code-block:: make

   include lib/Makefile.foo

   myCProg: myCProg.c lib/libfoo.a
     $(CHPL_COMPILER) $(CHPL_CFLAGS) -o myCProg myCProg.c $(CHPL_LDFLAGS)

.. _Makefileless Compilation In Single Locale:

CMake Helper
~~~~~~~~~~~~

Similar to the makefile helper, the Chapel compiler can also generate a
CMakeLists file containing the includes directories and linker flags that must
be added to a CMake project to properly compile. Such a CMakeLists file can be
generated using ``--library-cmakelists``.

For a Chapel library with the name ``FooLibrary``, this CMakeLists file defines
``FooLibrary_INCLUDE_DIRS`` and ``FooLibrary_LINK_LIBS`` which can
be used in your CMake project. To incorporate your Chapel library into a
target named ``myTarget``, add the following lines to your project's CMakeLists:

.. code-block:: cmake

   include(path/to/generated/CmakeLists/FooLibrary.cmake)
   target_include_directories(myTarget PUBLIC ${FooLibrary_INCLUDE_DIRS})
   target_link_libraries(myTarget PUBLIC ${FooLibrary_LINK_LIBS})

.. _CMake Helper Example:

Makefile-less Compilation
~~~~~~~~~~~~~~~~~~~~~~~~~

You can also generate the compilation flags necessary to compile a C program
using a Chapel library by using the ``compileline --compile`` and ``compileline
--libraries`` tools we provide.  The compilation command would then look like
this (replacing ``myCProg.c`` with the name of your C program that will use the
library):

.. code-block:: sh

   `$CHPL_HOME/util/config/compileline --compile` myCProg.c -Llib/ -lfoo `$CHPL_HOME/util/config/compileline --libraries`

Note that ``compileline --compile-c++`` is also available for compiling a C++
program.


.. _readme-libraries.Python:

Using Your Library in Python
============================

Prerequisites
--------------

To make use of your library in Python with minimal work, the Chapel compiler
requires the following:

- ``python3`` installed in your ``$PATH``
- ``Cython``
- ``numpy``

If you are on a system where libraries are built to be position dependent by
default (e.g.  not OSx), you will need to set the environment variable
``CHPL_LIB_PIC`` to ``pic`` and perform a ``make`` command from ``$CHPL_HOME``.
This will cause the Chapel runtime and third-party libraries to be built with
position independent code, which Python interoperability requires.  Note that
position independent code will likely encounter some performance degradation as
opposed to normal Chapel code.  For this reason, we recommend only using
``CHPL_LIB_PIC=pic`` when position independent code is required (e.g. when
calling Chapel code from Python).

Compiling Your Chapel Library
-----------------------------

To create a Python-compatible module in addition to the normally generated
library and header, add ``--library-python`` to the compilation.

.. note::

   When compiling on a Cray, or a machine with multiple C compilers, you should
   ensure your ``CHPL_TARGET_COMPILER`` is the same as the compiler used to
   install Cython (usually the default C compiler for the machine, or
   ``cray-prgenv-gnu`` on Cray systems).  Using a different
   ``CHPL_TARGET_COMPILER`` may lead to ABI incompatibility issues or the use of
   unexpected flags when compiling your Python module.  See
   :ref:`readme-chplenv.CHPL_COMPILER` for more information on the values of
   ``CHPL_TARGET_COMPILER``

.. _Python_Output_Directory_Name:

Python Output Directory Name
----------------------------

By default, the name of the directory created to contain the generated Python
module will match the generated Python module name. To change the
output directory name so that it does not match the generated Python module
name, use the compilation flag ``--library-dir``.

.. code-block:: bash

  # Builds Python module as foo/foo.py from foo.chpl
  chpl --library-python foo.chpl

  # Builds Python module as lib/foo.py from foo.chpl
  chpl --library-python --library-dir=lib foo.chpl

Python Module Name
------------------

By default, the name of the generated Python module will match the basename
of the generated library, but can be changed independently of the generated
library name using the compilation flag ``--library-python-name``:

.. code-block:: bash

   # Builds Python module as foo/foo.py from foo.chpl
   chpl --library-python foo.chpl

   # Builds Python module as bar/bar.py from foo.chpl
   chpl --library-python --library-python-name=bar foo.chpl

Because the default output directory name mirrors the Python module name,
changing the name of the generated Python module will also change the output
directory name (as in the second example above).

To change the output directory name and the output module name, use a
combination of ``--library-dir`` and ``--library-python-name``.

.. code-block:: bash

  # Builds Python module as foo/bar.py from baz.chpl
  chpl --library-python --library-python-name=bar --library-dir=foo baz.chpl

PYTHONPATH
----------

To use your library in a Python program, you will need to extend your
``PYTHONPATH`` environment variable to include the directory where your library
files are generated, e.g.:

.. code-block:: sh

   export PYTHONPATH=lib/:$PYTHONPATH

See :ref:`Python_Output_Directory_Name` for where your library files are
generated, and how to change this location when generating a Python module
from your Chapel library.

.. _Python Libraries:

Initializing and Using Your Library in Python
---------------------------------------------

Once your ``PYTHONPATH`` is set up and the Python module created, you can
``import`` the module like a normal Python module.

Similarly to using your library with C, you will need to call a set up function
to ensure the Chapel runtime and standard modules are initialized, as well as
a clean up function.

Unlike the C case, the set up function is called ``chpl_setup()`` and will also
handle initializing your module.   This function will still need to be called
prior to any Chapel library function calls.

Also unlike the C case, the clean up function is called ``chpl_cleanup()``.
This function will still need to be called after all the Chapel library
function calls are finished, unless you have imported the output directory
as a package using the :ref:`Python_Init_File`.

For example:

.. code-block:: Python

   import foo

   foo.chpl_setup()

   foo.baz(7) // Call into a library function

   foo.chpl_cleanup()

.. note::

   The ``chpl_cleanup()`` function will also cause the Python program to exit.
   Make sure your Python functionality is also complete before calling this
   function.

.. note::

  If you are taking advantage of the generated ``__init__.py`` initializer
  file to import the output directory as a package, you do not need to call
  ``chpl_cleanup()`` yourself because it is already registered to be called
  at program exit. The generated initializer is explained below.

.. _Python_Init_File:

Python Init File
----------------

A simple ``__init__.py`` file is generated in the output directory along with
the Python module. It looks roughly like the following:

.. code-block:: python

  import atexit

  #
  # Here directoryName is the name of the directory containing your Python
  # module, and moduleName is the name of your Python module.
  #
  from directoryName.moduleName import *

  atexit.register(moduleName.chpl_cleanup)

The initializer file allows the output directory to be imported as a Python
package. It will also register ``chpl_cleanup()`` to be called automatically
at program exit.

Like any other package, the generated Python package must be visible in order
to import it, such as through importing it locally or by adding it to the
``PYTHONPATH``. Refer to the Python 3 import
`documentation <https://docs.python.org/3/reference/import.html#the-import-system>`_
for more details.

.. code-block:: bash

  # Builds Python module as foo/foo.py
  chpl --library-python foo.chpl

  # Adds the current directory to your PYTHONPATH
  export PYTHONPATH="$PWD:$PYTHONPATH"

From within your Python script:

.. code-block:: Python

  import foo

  #
  # Setup and use foo as normal. Note that we no longer have to call
  # ``chpl_cleanup()`` when we are finished.
  #
  foo.chpl_setup()
  foo.baz(2)

.. note::

  The Chapel compiler will not generate an initializer file if a file with
  the name ``__init__.py`` already exists in the output directory. The
  compiler will emit a warning instead.

Argument Default Values
-----------------------

Python has the capacity to support default values for arguments.  The ability to
call Chapel exported functions with argument default values from Python is
present, but is not yet fully supported.  See :ref:`the Caveat section
<default-values>` for more details.

For the cases that are not supported, the compiler will generate a warning. The
argument must always be provided when calling the function.

c_ptr Arguments
---------------

Python code can pass ``numpy`` arrays or ``ctypes`` pointers to ``c_ptr``
arguments.

Debugging Issues with --library-python
--------------------------------------

This compilation strategy uses Cython under the covers, generating a
``chpl_foo.pxd`` file, a ``foo.pyx`` file, and a ``foo.py`` file by default for
a ``libfoo.a`` / ``libfoo.so``, which are then called using a Cython command
(this command is rather long due to the need to include the Chapel runtime and
third-party libraries).  These files are currently left in the same location as
the generated library - if compilation fails due to generating one or more of
these files incorrectly, you may be able to modify the file and re-run the
Cython command yourself.

.. _readme-libraries.Fortran:

Using Your Library in Fortran
=============================

Prerequisites
-------------

To make use of your library in Fortran, a Fortran compiler that implements
the ISO_Fortran_binding.h header and interface defined by ISO/IEC TS 29113
is required.

Compiling Your Chapel Library
-----------------------------

To create a Fortran compatible module in addition to the normally generated
library and header, add ``--library-fortran`` to the compilation. This will
create a Fortran module containing declarations for each Chapel function
declared with ``export``. This module can be used from Fortran in order to
make the functions exported from Chapel available.  At present, the generated
module only handles basic types for function arguments and return types, and
the compiler will emit warnings for any types it is unable to handle properly.

Initializing and Using Your Library From Fortran
------------------------------------------------

Once the library and Fortran interface module are generated, you can ``use``
the interface module and make calls to the functions it declares.

Similarly to using your library with C and Python, you will need to call a
set up function to ensure the Chapel runtime and standard modules are
initialize. Unlike C and Python, your library currently needs to define
this function itself.  The following should work after replacing
``MyModuleName`` with the name of the actual module:

.. code-block:: Chapel

    export proc chpl_library_init_ftn() {
      // Make the runtime/library initialization function visible
      extern proc chpl_library_init(argc: c_int, argv: c_ptr(c_ptr(c_char)));
      var filename = "fake":c_ptrConst(c_char);
      // Initialize the internal runtime/library
      chpl_library_init(1, c_ptrTo(filename): c_ptr(c_ptr(c_char)));
      // Initialize the main user module
      chpl__init_MyModuleName();
    }

Note that the module initializer call in this context takes no arguments - that
is because the arguments will be inserted during Chapel compilation.

A simple Fortran example using a function ``myChapelFunction`` from the
``MyModuleName`` library is:

.. code-block:: Fortran

    program Example
      ! use the interface module generated with --library-fortran
      use MyModuleName
      implicit none

      integer(8) :: arg, ret
      arg = 3

      ! initialize the Chapel library using the function defined above
      call chpl_library_init_ftn()

      ! call a function from the Chapel library
      ret = myChapelFunction(arg)

      print *, ret
    end program Example

This would then be compiled with commands to first build the interface module,
then to build the example program and link with the Chapel library and Chapel
runtime libraries:

.. code-block:: sh

    ftn -c lib/MyModuleName.f90
    ftn Example.f90 -Llib -lMyModuleName `$CHPL_HOME/util/config/compileline --libraries` -o Example

Arrays
======

Arrays can be returned by exported Chapel functions as one of two C types:

- ``chpl_external_array``

  - For arrays that can be translated into native C or Python arrays.  In
    Python, the contents of this type is copied into a Python array.

- ``chpl_opaque_array``

  - For arrays that are not currently translated.  In Python, this is used as a
    field in a Python class named ``ChplOpaqueArray``.

chpl_external_array
-------------------

A ``chpl_external_array`` can be created in C or returned by a Chapel function
declared as returning specific Chapel array types.  To create a
``chpl_external_array`` in C, you can call:

- ``chpl_make_external_array(elt_size, num_elts)`` to create an empty array of
  the given size.

- ``chpl_make_external_array_ptr(elts, num_elts)`` where ``elts`` is an existing
  array of the given size.

Users should call ``chpl_free_external_array`` to indicate that they are done
using the ``chpl_external_array`` instance if it was created for them by a
Chapel function or via ``chpl_make_external_array``.  Users should explicitly
free any memory that was stored in a ``chpl_external_array`` using
``chpl_make_external_array_ptr``.

.. note::
   The names of these functions may change.

chpl_opaque_array
-----------------

Chapel arrays that cannot be returned using ``chpl_external_array`` will be
returned using ``chpl_opaque_array``.  ``chpl_opaque_array`` instances cannot be
created outside of Chapel, nor can their contents be accessed.
``chpl_opaque_array`` instances can only be received and sent to Chapel
functions.

Users should call ``cleanupOpaqueArray`` to indicate they are done using the
``chpl_opaque_array`` instance.

It is our intention to support as many Chapel array types as we can using
``chpl_external_array``.  Chapel arrays types that are currently supported using
``chpl_opaque_array`` may become supported by ``chpl_external_array`` instead
in the future.

Fortran arrays
--------------

A 1-D contiguous Fortran array can be passed to an exported Chapel function
for an argument with the type ``[] t`` where ``t`` is a primitive type.  The
Chapel compiler will automatically translate such an array into a Chapel array.
This allows it to be used in all the ways any other Chapel array can be used,
for example in parallel loops or reductions.

Using Your Library in Chapel
============================

Chapel library files cannot be used from Chapel code.  The library files must
include the chapel runtime and standard modules for use in a non-Chapel program
and when the library is linked to a Chapel program this leads to multiple
definitions of these functions.

Using Your Library in Multilocale Settings
==========================================

Prerequisites
-------------

Chapel also supports ``--library`` when ``CHPL_COMM != none``.  We intend to
support other settings in the future, see :ref:`Other Settings` in the
:ref:`Multilocale Caveats` section for more information.

To compile a multilocale library, `ZeroMQ <https://zeromq.org/>`_ must be
installed.

If ZeroMQ is not installed in a way that enables your C compiler to find it
easily, the environment variable ``CHPL_ZMQ_HOME`` can be set.  This environment
variable should be set to a directory containing both an ``include`` directory
which contains ``zmq.h`` and a ``lib`` directory which contains ``libzmq.*``.
For example, for a directory structure:

.. code-block:: text

   |-- .local/
   |    |-- include/
   |    |    |-- zmq.h
   |    |-- lib/
   |    |    |-- libzmq.a
   |    |    |-- libzmq.so

``CHPL_ZMQ_HOME`` would be set to ``/absolute/path/to/.local/``.

Initializing Your Multilocale Library
-------------------------------------

Multilocale libraries can be used in a manner similar to single locale
libraries.  However, as with transitioning between a single locale executable
and a multilocale one, it is necessary to specify the number of locales required
for the multilocale library.

In C
~~~~

Users must still call ``chpl_library_init()`` before utilizing the exported
Chapel functions.  However, the ``char* argv[]`` must now include two additional
entries: the numlocales flag and its intended value.

This can be accomplished either by explicitly adding the arguments in the C
client program itself, or by passing them as arguments to the executable.

This example demonstrates explicitly adding the arguments in the program using
the ``foo`` library.

.. code-block:: C

   #include "foo.h"

   int main(int argc, char* argv[]) {
     int argChapelC = 3;
     char* argChapelV[3] = {argv[0], "-nl", "2"};
     // Initialize the Chapel runtime and standard modules
     chpl_library_init(argChapelC, argChapelV);

     baz(7); // Call into a library function

     chpl_library_finalize();

     return 0;
   }

Alternatively, the original single locale client from `Initializing Your
Library In C`_ can be used with the additional two arguments to the executable:

.. code-block:: bash

   ./a.out -nl 2

Users also still need to call the generated module initialization function for
multilocale libraries, as mentioned in that section.

In Python
~~~~~~~~~

Users must still call ``chpl_setup()`` before utilizing the exported Chapel
functions.  However, it requires a ``numLocales`` argument when the library
has been compiled for multilocale settings.  E.g. to run with ``4`` locales,
write:

.. code-block:: Python

   chpl_setup(4)

instead of:

.. code-block:: Python

   chpl_setup()

in addition to the other steps described in :ref:`Python Libraries`.

Makefile-less Compilation
-------------------------

When compiling a C program using a multilocale Chapel library without a
makefile, some additional steps are needed beyond those required for using a
:ref:`single locale Chapel library <Makefileless Compilation In Single Locale>`.
For this reason, we strongly suggest using the Makefile generated by the
``--library-makefile`` flag, as described :ref:`here <Makefile Helper>`.  If
you are using this flag, you can skip the rest of this section.

A new ``compileline`` tool, ``compileline --multilocale-lib-deps`` is currently
required after ``compileline --libraries``.  Additionally, the library being
linked must be listed twice in the compilation command - once before
``compileline --libraries`` and once at the end of the command.  The compilation
command would then look like this (replacing ``myCProg.c`` with the name of your
C program that will use the library):

.. code-block:: sh

   `$CHPL_HOME/util/config/compileline --compile` myCProg.c -Llib/ -lfoo \
   `$CHPL_HOME/util/config/compileline --libraries` \
   `$CHPL_HOME/util/config/compileline --multilocale-lib-deps` -lfoo

What is this _server_real program?
----------------------------------

When you compile a Chapel library for use with multiple locales, you should
typically see both a library (see :ref:`Location of the Generated Library` for
where this will be placed and how to control that location) and a binary (which
is currently generated in the same directory as the "main" source file), in
addition to other support files such as the generated header, makefile, etc.

The library will be named as specified in :ref:`Library Name`; the binary will
use the same base name as the library (omitting the ``lib`` and ``.so`` or
``.a`` portions), followed by ``_server_real``.  Thus for a library
``libfoo.so``, the binary would be named ``foo_server_real``.

The library will appear like a normal single locale library in terms of the
interface it provides to client programs - however, under the covers it will
launch the binary and then communicate with it.  The binary will be what
executes the exported functions and will communicate the result back to the
library, to return to the client program.

Hostnames and Connection Issues
-------------------------------

By default, the generated client library will expect the generated server to
communicate with it using the hostname of where the client program is running,
as obtained by ``gethostname()``.  This default can be overridden by setting
the environment variable :ref:`chpl-rt-masterip`.

Debugging Issues with Multilocale Libraries
-------------------------------------------

The ``chpl`` compiler provides a developer flag, ``--library-ml-debug``, which
can be used to generate communication and underlying library implementation
debugging output.  It is useful for tracking down connection issues between the
generated executable and the generated library and unlikely to be helpful when
tracking down issues with an exported function's body.

Caveats
=======

Supported Types for export procs
--------------------------------

See :ref:`readme-extern-declarations-limitations` for details of what
intents and types are allowed.

Multiple Chapel Libraries
-------------------------

Multiple Chapel libraries cannot currently be used in the same C or Python
program.  Each library file must include the chapel runtime and standard modules
for its own functionality and when two or more libraries are linked to a program
this leads to multiple definitions of these functions.

LLVM
----

LLVM support with ``--library`` is currently a work-in-progress.  For the 1.20
release, it does not support Fortran or multilocale interoperability.  We expect
to extend this support in later releases.


.. _Exporting Symbols:

Exporting Symbols
-----------------

Only functions can be exported currently.  We hope to extend this support to
types and global variables in the future.

.. _default-values:

Argument Default Values
~~~~~~~~~~~~~~~~~~~~~~~

Python interoperability currently supports default values for function
arguments, but only when the default value is a literal (e.g. ``4``,
``"blah"``).  Default values that are more complicated are not currently
supported.  We hope to extend this support in the future.

C interoperability does not support default values for function arguments.  We
do not anticipate supporting argument default values in C.

.. _Python Intents:

Intents in Python Interoperability
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Libraries compiled for Python do not support the default intent for ``string``,
``c_string`` or 1D arrays, as copies are currently performed (which would
violate ``const ref``).  Instead, only the ``in`` and ``const in`` intents are
supported for these types.  We may be able to support all intents in the future.

.. _Multilocale Caveats:

Multilocale Caveats
-------------------

.. _Other Settings:

Other Settings
~~~~~~~~~~~~~~

The following settings are not yet supported for ``--library`` compilation:

- ``--no-local``
- ``CHPL_COMM = none`` when ``CHPL_LAUNCHER != none``

These settings would behave similarly to the current behavior with ``CHPL_COMM =
gasnet``, when relevant - for instance, it is expected that all of these
settings would result in an executable that communicates with the user's program
via the generated library.

Other configurations may also become supported in the future.

Host and Target Compilers
^^^^^^^^^^^^^^^^^^^^^^^^^

Multilocale libraries currently require the host and target compiler to be
compatible. For example, on Crays, a host value of ``gnu`` and a target
value of ``cray-prgenv-gnu`` would be considered equivalent.

In the near future, the client library (the library that a user will link
against) will be compiled by the host compiler, while the server will be
compiled by the target compiler.

Supported Types
~~~~~~~~~~~~~~~

Multilocale libraries support the same argument and return types as single
locale libraries, with the notable exception of ``complex`` numbers, arrays, and
pointer types.  We anticipate extending the supported types in the future,
though may not end up supporting pointer types.

Intents
~~~~~~~

Multilocale libraries do not support the default intent for ``string`` and
``c_string``, as the default intent is ``const ref``.  Only the ``in`` and
``const in`` intents are supported.  We may expand to support ``out`` and
``inout`` in the future.
