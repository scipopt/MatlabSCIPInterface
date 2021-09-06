# Matlab Interface to SCIP and SCIP-SDP

This repository provides an interface of Matlab/Octave to
[SCIP](https://www.scipopt.org/) and [SCIPSDP](http://www.opt.tu-darmstadt.de/scipsdp/).

This interface is based on the OPTI Toolbox by Jonathan Currrie, see:

https://github.com/jonathancurrie/OPTI

Since the OPTI Toolbox is unfortunately no longer maintained, we wanted
to provide such a Matlab/Octave interface. However, the OPTI Toolbox
contains interfaces to many solvers, which we cannot support in its
full generality. We therefore took the OPTI Toolbox, stripped away all
parts concerned with other solvers and reworked the SCIP specific
code. Thus, in comparison to the orginal OPTI Toolbox, this interface
has the following features:

- It only interfaces to SCIP and SCIP-SDP, but retains the modeling
  features of the OPTI Toolbox, including symbolic modeling of
  nonlinear optimization problems.

- Code that worked for the OPTI Toolbox using SCIP as a solver should
  still work.

- The interface now works under Linux and for Octave.

- The preliminary interface to SCIP-SDP has been extended to a
  full-fledged version.

In case someone wants to reactivate the OPTI Toolbox, these changes
might be incorporated.

We carefully tested the interface, but it is very likely that problems
with the installation and/or application arise. Some obstacale are:
there are many ways of installing SCIP/SCIP-SDP (make/cmake/packages),
the code should run both under Linux and Windows, and interfacing with
Matlab is not easy. Please see below for some known issues.

#### Compatibility

- The interface should work for Matlab versions starting with 2011a and
  Octave versions of at least 6.0.  It has been tested with Matlab
  2020a/b as well as Octave 6.1.

- There is currently a bug in Octave such that running the test fails with
  `error: mark_as_constructed: invalid object`, see [https://savannah.gnu.org/bugs/?func=detailitem&item_id=59775].
  Until this bug is resolved, the interface will not be fully operational.

- One needs least version 7 of [SCIP](https://www.scipopt.org/)
  and, if wanted, at least version 4 of
  [SCIPSDP](http://www.opt.tu-darmstadt.de/scipsdp/).


## MATLAB-SCIP Interface

Obviously the interface needs [SCIP](https://www.scipopt.org/).

### Installation

The installation process sets the appropriate paths for the mex file
in Matlab/Octave to run. These paths will reset if you restart
Matlab. To save path changes run the command

  savepath()

with appropriate permissions (see Common Problems: Saving path changes).


#### Linux

- Download the latest version of SCIP. See the corresponding
  installation description.

- If you build SCIP from source, the Matlab-SCIP installation process
  supports both make and cmake installs.

- The mex file uses shared libraries, which are standard for cmake.
  Make users need to specifically build shared libraries by using
  `make ... SHARED=true`.

- To specify the location of SCIP for the Matlab interface, you can
  set the environment variable SCIPDIR to the SCIP directory. This
  variable has to be defined before starting Matlab/Octave and in the
  same terminal.

  Alternatively you can specify the SCIP directory during the
  installation process.

- Note: In case you have installed the full SCIP Optimization Suite,
  the environment variable SCIPOPTDIR can be used to specify the main
  directory of the full optimization suite instead.

- To start the installation process, change to the "opti" directory
  within Matlab/Octave and run the file
  `matlabSCIPInterface_install.m` in Matlab or Octave. The
  installation automatically searches environment variables for SCIP
  and prompts for user input, if it does not find a valid SCIP
  installation. On linux systems specifying a gcc compiler might be
  necessary (see below).


#### Windows

- Download the latest version of SCIP. See the corresponding
  installation description.

- The Matlab-SCIP installation process supports both make and cmake
  installs as well as the Windows specific .exe install of SCIP.

- If you plan on building SCIP from source, the GNU specific
  make-functionality is needed. Make is supported by, for example,
  [WSL](https://docs.microsoft.com/en-us/windows/wsl/install-win10) or
  [Chocolatey](https://chocolatey.org/courses#getting-started). Refer
  to their documentation for more information.

- To specify the location of SCIP for the Matlab/Octabe interface, you
  can permanently set the environment variable SCIPDIR to the SCIP
  directory. The definition has to be performed before starting
  Matlab/Octave and in the same terminal.

  Alternatively you can specify the SCIP directory during the
  installation process.

- To start the installation process, change to the "opti" directory in
  Matlab/Octave and run the file `matlabSCIPInterface_install.m` in
  Matlab or Octave.  The installation automatically searches
  environment variables for SCIP and prompts for userinput, if it does
  not find a valid SCIP installation.

- Note that for octave, the SCIP installation paths should not contain
  spaces. So you might need to rename "SCIPOptSuite 8.0.0", e.g., to
  "SCIPOptSuite8.0.0".

### Troubleshooting

#### Compiler

The mex compilation process needs a compatible C++ compiler to work.
On Linux systems the [C++ compiler support of
Matlab](https://de.mathworks.com/support/requirements/previous-releases.html)
is severly restricted.  Make sure you have a compatible C++-Compiler
installed on your system. You will be prompted to specify such a
compiler during the installation process.

On Windows, the Matlab internal C++ runtime or the Matlab internal
mingw addon are recommended for ease of use and compatibility with
Matlab. Other C++ compilers may be used, but need to be compatible
with the respective Matlab/Octave version and must be made available
at runtime to allow the installation script to work. Advanced users
may compile the mex files manually using the Matlab mex compiler
directly in the system console, but no active support for this process
is provided.

#### Linux: Problems with Lapack/MKL using SCIP with IPOPT

This problem will often manifest with the message:

'Intel MKL ERROR: Parameter 5 was incorrect on entry to DSYEV.'

Matlab under Linux ships with a MKL version that replaces Blas, but is
compiled with 64 Bit integers. The dynamic shared object loader will
replace the Lapack/Blas implementation used by IPOPT by this MKL. If
IPOPT uses a 32 Bit version, this causes a crash.

There does not seem to exist a nice and easy solution, but the following two
might work for you:

- You can tell the system to use the Lapack versions that were used by IPOPT with

  `LD_PRELOAD=/path/to/Lapack/liblapack.so matlab`

  Possibly further shared libraries have to be added.

  This solutions seems to work, but has the price that all comands in
  Matlab that use Lapack will fail (because they do not use MKL
  anymore). In particular, eigenvalue comutations will fail.

- You can build IPOPT using static versions of Lapack and possibly
  linear algebra packages like Mumps. We have tested this under Ubuntu
  16.04 with the 'stable/3.14' version of Ipopt and Ubuntu Mumps
  packages. Then one needs to build a static version of Ipopt using
  the static versions of Mumps. This can for example be done as
  follows:

  './configure --prefix=<your install directory> --with-lapack --with-lapack-lflags="/usr/lib/liblapack.a /usr/lib/libblas.a /usr/lib/gcc/x86_64-linux-gnu/5/libgfortran.so /usr/lib/x86_64-linux-gnu/libm.so" --with-mumps --with-mumps-lflags="/usr/lib/libdmumps_seq.a /usr/lib/libmumps_common_seq.a /usr/lib/libmpiseq_seq.a /usr/lib/libpord_seq.a" --with-mumps-cflags="-I/usr/include/mumps_seq" --enable-static --disable-shared --enable-shared=no --enable-relocatable'

  Clearly, the paths need to be adjusted to your machine and you might
  want to use the parallel verison of Mumps (remove 'libmpiseq' and
  remove '_seq' from the library names). Then building SCIP with

  'make IPOPT=true SHARED=true USRLDFLAGS=-Wl,-Bsymbolic'

  creates 'libscipsolver.so' with no outside dependencies to Lapack or Blas.


Any further solution options are welcome.

#### Error after compilation

One possible error is:

`Error: Error Compiling SCIP! '/path/to/mexfile.mexext' is not a MEX file. For more information, see File is not a MEX file.`

This is a possible false-positive and can usually be ignored. It
occurs, if the mex-extension of the mex file does not fit your system,
or the mex-File is not recognized.  On some Linux distributions this
error occurs despite successfully compiling a fully functional
mex-File.

#### MEX file is not compatible with this version of the Interface

If this error is encountered, the mex file could not be determined to
be working.  Most likely because the links in your mex-call were not
working correctly. If the mex command generated a mex-file, you can
check the specific error by evaluating the mex-File using the command:

 `eval scip`

#### GLIBCXX not found

Evaluating the mex-File results in:

`Invalid MEX-file '/path/to/mexfile.mexext': path/to/matlab/libstdc++.so: version 'GLIBCXX_x.x.xx' not found (required by /path/to/scipinstall/lib/libscip.so).`

This error is encountered on some Linux systems using the default
cmake-install.  It is caused by Matlab using its own separate standard
C++ library.  To circumvent this error you can inject your systems
``libstdc++`` by starting Matlab using the command

 `LD_PRELOAD=/path/to/system/libstdc++.so matlab`

so that Matlab prefers your system's C++ standard library over its
own. This has to be repeated every time you launch
Matlab. Alternatively consider statically linking SCIP to your systems
`libstdc++` file, to prevent this conflict.

#### Symbol lookup error undefined symbol

Errors like this typically occur if one of the links in the mex call
is broken. Please check the mex call made by Matlab during the
installation and verify that all paths and filenames are set correctly
and that the referenced files are working as intended.

#### Saving path changes

Matlab saves changes to the path in a file called `pathdef.m` located
in the install folder of Matlab. Depending on your enivronment, the
Matlab instance might not have permission to write to this
file. Usually Matlab will create a root directory for each user
(e.g., `/home/username/Documents/MATLAB`), where a custom `pathdef.m`
file can be placed (see
[searchpath](https://de.mathworks.com/help/matlab/matlab_env/what-is-the-matlab-search-path.html)
and
[savepath](https://de.mathworks.com/help/matlab/ref/savepath.html)).
Alternatively a startup.m file can be used to add new paths when
restarting Matlab (see
[startup](https://de.mathworks.com/help/matlab/ref/startup.html)).
For determining which paths are needed in a startup file, refer to the
file `ReAddPaths.m`.


## Matlab-SCIPSDP Interface

Most comments for the Matlab-SCIP interface also apply to the
Matlab-SCIP-SDP interface.

Ensure that you are using the latest version of
[SCIP](https://www.scipopt.org/) and see the comments above.  Ensure
that you are using the latest version of
[SCIPSDP](http://www.opt.tu-darmstadt.de/scipsdp/).  The installation
process currently supports make installs of SCIPSDP on both Linux and
Windows platforms.  Make sure that you build the program with shared
libraries (see above). The mex compilation process is currently
incompatible with static libraries. To specify the install location,
you can either permanently set the environment variable `SCIPSDPDIR`
to the SCIP-SDP directory or specify the source directory during the
installation process.

Please refer to the Troubleshooting section of the Matlab-SCIP
interface above.

## License

The original OPTI toolbox was released under the 3-clause BSD
license. The changes and extensions of this interface are released
under the same license, as detailed in the file LICENSE. It is free
software, and is released as an open-source package. Note that SCIP
has its own academic license.

## Authors

This interface is based on the OPTI Toolbox by Jonathan Currrie. He
wrote most of the Matlab-code. The interface has been adpated and
extended by Nicolai Simon and Marc Pfetsch, TU Darmstadt.


