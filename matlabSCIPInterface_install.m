function matlabSCIPInterface_install(runTests, specifyCPP)
% OPTI Toolbox Installation File
%
%  matlabSCIPInterface_install(runTests, specifyCPP)
%
%   runTests: Run the post-installation tests
%   specifyCPP: Specify a non-default C++-compiler version
%
% All arguments are optional and if not supplied, the user will be prompted
% to enter their selection in the MATLAB Command Window. True is the
% default option for each argument.
%
% Based on code by Jonathan Currie:
% Copyright (C) 2018 Jonathan Currie (Inverse Problem Limited)
% https://inverseproblem.co.nz/OPTI/
% Adpated by Nicolai Simon, Copyright (C) 2021 TU Darmstadt


% options:
opts = [];
opts.verb = false;  % verbose logging
opts.debug = false; % debug for mex files
opts.pp = [];       % cell array of preprocessor options, i.e. -D flags
opts.expre = '';    % string to insert further commands before source file
opts.quiet = false; % set mex compilation to quiet

if (strcmp(computer, "PCWIN"))
    error("Win32 systems are not supported");
end

% set paths needed for the interface install
cd(fileparts(which(mfilename)));
addpath([pwd() '/Source']);
addpath([pwd() '/Source/scip']);
addpath([pwd() '/Source/scip/Include']);
addpath([pwd() '/Utilities/Install']);
addpath([pwd() '/Utilities/opti']);
addpath([pwd() '/Examples']);

% handle missing input args
if ~(exist('runTests', 'var')), runTests = []; end
if ~(exist('specifyCPP', 'var')), specifyCPP = []; end

% MEX interface source files
src = {'scip/scipmex.cpp scip/scipeventmex.cpp scip/scipnlmex.cpp'};

% set path to SCIP files
scippath = setSCIPPath();

% set include files needed for mex-file
[inc, installversion] = setInclude(scippath);

% set libraries needed for mex-file & specify the corresponding LDflags
[lib, opts.expre] = setLibraries(scippath, opts.expre, installversion);

% allow for custom CXX Versions for Matlab on linux systems
if strcmp(computer, 'GLNXA64')
    cxx_custom = setCXXVersion(specifyCPP);
else
    cxx_custom=[];
end

opti_solverMex('scip',src, cxx_custom, inc, lib, opts);

fclose all;


cpath = pwd();
try
    cd('Utilities/opti');
catch %#ok<CTCH>
    error('You do not appear to be in the interface directory.');
end

% get current versions
localVer = optiver();

fprintf('\n------------------------------------------------\n')
fprintf('INSTALLING Matlab-SCIP Interface ...\n')

cd(cpath);
% check ML ver
if(~isOctave())
    matlabVerCheck();
end

% perform MEX file check (also checks pre-reqs)
if (~mexFileCheck(localVer))
    return;
end

% update files in cache
rehash;

% post install test if requested
if (isempty(runTests))
    fprintf('\n');
    in = input('Would you like to run post installation tests? (Recommended) (y/n): ','s');
else
    in = bool2yn(runTests);
end
if(strcmpi(in,'y'))
    opti_Install_Test(1);
end

% finished
fprintf('\n\nMatlab-SCIP Interface installation complete!\n');

optiSolver;

fprintf('To be able to use the Matlab-SCIP Interface after restarting Matlab, you need to save path changes\n')
fprintf('or add them manually every time you launch matlab (the script addPaths.m can be used to do so).\n')
fprintf('Paths can be saved using the command:\n')
fprintf('     savepath() \n')
fprintf('If you are unable to save path changes we refer to the ''Common Problems'' section in the README\n')
fprintf('or directly to the Matlab documentation.\n')





% The function setSCIPPath determines the path to the SCIP installation and
% prompts user input, if no path is found.
function scippath = setSCIPPath()

scippath = [];

% include directories
fprintf('\nSearching for SCIP install ...\n');
% check environment variables
fprintf('Checking environment variables SCIPOPTDIR and SCIPDIR ...\n');

if (~isempty(getenv('SCIPOPTDIR')))

    fprintf('SCIPOPTDIR variable found.\n')
    scipoptpath = getenv('SCIPOPTDIR');
    scipoptpath = strrep(scipoptpath, '\', '/');
    if(exist([scipoptpath '/scip'], 'dir'))
        scippath = checkScipDir([scipoptpath '/scip']);
    else
        scippath = checkScipDir(scipoptpath);
    end
elseif (~isempty(getenv('SCIPDIR')))

    fprintf('SCIPDIR variable found.\n')
    scipdirpath = getenv('SCIPDIR');
    scipdirpath = strrep(scipdirpath, '\', '/');
    scippath = checkScipDir(scipdirpath);

end

if (isempty(scippath))
    yesno = input('SCIP not found! Would you like to specify a custom install location? (y/n)', 's');
    if (strcmp(yesno,'y'))
        fprintf('Please specify SCIP directory:');
        scipdirpath = uigetdir();
        scipdirpath = strrep(scipdirpath, '\', '/');
        scippath = checkScipDir(scipdirpath);
    end
end

if(isempty(scippath))
    error('No SCIP installation found!');
end


% This function checks if the specified SCIP directory can be used as a valid SCIP
% installation.
function scippath = checkScipDir(scipdirpath)

if (~(exist([scipdirpath '/src'], 'dir') || exist([scipdirpath '/include'], 'dir')))
    fprintf('\nCould not identify src or include directory: \n');
    fprintf('Structure of specified SCIP directory ''')
    fprintf(scipdirpath)
    fprintf(''' not recognized.\n')
    scippath = [];
    if (exist([scipdirpath '/scip'], 'dir') && exist([scipdirpath '/soplex'], 'dir') && exist([scipdirpath '/papilo'], 'dir'))
        yesno = input('Specified directory seems to contain scipoptsuite root. Use subfolder /scip as scip directory? (y/n)', 's');
        if (strcmp(yesno,'y'))
            scippath = [scipdirpath '/scip'];
        end
    end
else
    fprintf(['\nSetting SCIP directory to ''' scipdirpath '''.\n']);
    scippath = scipdirpath;
end


% This function setInclude uses the SCIP path to set the correct include
% directory and determine the installation method used.
function [inc, installversion] =  setInclude(scippath)


% include directories
if (exist([scippath '/obj'], 'dir'))
    fprintf('\nSCIP build using ''make'' detected.\n');
    installversion = 1;
    inc = {'scip/Include',[scippath '/src'],[scippath '/src/blockmemshell']};
elseif (exist([scippath '/build'], 'dir'))
    fprintf('\nSCIP build using ''cmake'' detected.\n');
    installversion = 2;
    inc = {'scip/Include',[scippath '/src'],[scippath '/src/blockmemshell']};
elseif (exist([scippath '/../build'], 'dir'))
    fprintf('\nSCIPOPT build using ''cmake'' detected.\n');
    installversion = 3;
    inc = {'scip/Include',[scippath '/src'],[scippath '/src/blockmemshell']};
elseif (exist([scippath '/lib'], 'dir'))
    fprintf('\nSCIP install using ''make install'' or ''.exe'' detected.\n');
    installversion = 4;
    inc = {'scip/Include',[scippath '/include'],[scippath '/include/blockmemshell']};
end

% Check if the automatically chosen directory contains the include files and prompt for user input if not
if ~checkDir(inc(2:length(inc)))
    yesno = input('\nNo include files found in default location. Would you like to manually specify the location of the SCIP include directory? (y/n)', 's');
    if strcmp(yesno, 'y')
        scippath = uigetdir();
        installversion = 5;
        inc = {'scip/Include',scippath,[scippath '/blockmemshell']};
        if ~checkDir(inc(2:length(inc)))
            error('No include files found in specified directory.');
        end
    else
        error('No include files found in default directory.');
    end
end

% Ensure that the specified paths cannot break due to whitespaces
for i = 2:length(inc)
    inc{i} = ['''' inc{i} ''''];
end

% This functions checks if the given path contains all directories we expect
function [defaultFound] = checkDir(dirLoc)

defaultFound = false;
if (~(isempty(dirLoc)))
    defaultFound = true;
    for i = 1:length(dirLoc)
        if ~(exist(sprintf('%s', dirLoc{i}), 'dir'))
            defaultFound = false;
        end
    end
end

% The function setLibraries uses the scippath and the identified
% installation version to setup the correct libraries for the installation
% process.
function [lib, expre] = setLibraries(scippath, expre, installversion)

% lib names [static libraries to link against]
switch(installversion)
    case 1
        scipLibPath = [scippath '/lib/shared/'];
    case 2
        scipLibPath = [scippath '/build/lib/'];
    case 3
        scipLibPath = [scippath '/../build/lib/'];
    case {4,5}
        scipLibPath = [scippath '/lib/'];
end

% Check if the automatically chosen directory contains the library folder and prompt for user input if not
[defaultFound, installversion] = checkScipLib(installversion, scipLibPath);
if ~defaultFound
    yesno = input('\nNo lib folder found in default location. Would you like to manually specify the location of the SCIP library? (y/n)', 's');
    if strcmp(yesno, 'y')
        scipLibPath = uigetdir();
        [defaultFound, installversion] = checkScipLib(installversion, scipLibPath);
        if ~defaultFound
            error(['No SCIP library found in' scipLibPath])
        end
    else
        error('No SCIP library found in default location.')
    end
end

% Format the library link
if isOctave()
    scipLibPath = ['"' scipLibPath '"'];
    scipLibOctPath = ['''' scipLibPath ''''];
    lib = ['-L' scipLibOctPath];
else
    scipLibPath = ['''' scipLibPath ''''];
    lib = ['-L' scipLibPath];
end
% Append the appropriate library name
switch(installversion)
    case 1
        lib = sprintf('%s -l%s',lib,'scipsolver');
    case {2,3}
        lib = sprintf('%s -l%s',lib,'scip');
    case {4,5}
        yesno = input('Was the specified SCIP library build using cmake/.exe? (y/n)','s');
        if strcmp(yesno, 'y')
            lib = sprintf('%s -l%s',lib,'scip');
        else
            lib = sprintf('%s -l%s',lib,'scipsolver');
        end
end

expre = [expre ' -DNO_CONFIG_HEADER'];
lib = [lib ' '];

if ~strcmp(computer, 'PCWIN64')
    if isOctave()
        expre = [expre ' ''-Wl,-rpath,' scipLibPath ''' '];
    else
        expre = [expre ' LDFLAGS=''-Wl,-rpath,' scipLibPath ''' '];
    end
end

% This functions checks if the given path contains all directories we expect
function [defaultFound, installversion] = checkScipLib(installversion, libLoc)

defaultFound = false;
if installversion == 5
    yesno = input('\nWas the specified library build using cmake/.exe? (y/n)', 's');
    if (strcmp(yesno,'n'))
        installversion = 1;
    end
end

if installversion == 1
    if (exist( [libLoc '/libscipsolver.so'], 'file') || exist([libLoc '/libscip.dll'], 'file'))
        defaultFound = true;
    end
else
    if (exist([libLoc '/libscip.so'], 'file') || exist([libLoc '/libscip.lib'], 'file') || exist([libLoc '/libscip.dll'], 'file'))
        defaultFound = true;
    end
end

% The function setCXXVersion allows users to specify a gcc compiler on linux
% versions of the installation.
function cxx_custom = setCXXVersion(specifyCPP)
cxx_custom='';
if specifyCPP
    cxx_custom = input('Please specify which compiler to use: ', 's');
    cxx_custom = [ ' GCC=''' cxx_custom ''' '];
    fprintf(['Setting ''' cxx_custom '''\n'])
elseif(isempty(specifyCPP))
    fprintf('\n');
    yesno = input('Would you like to specify a non-default GNU C++-Compiler (e.g. ''/usr/bin/gcc-8'')? (y/n)', 's');
    if (strcmp(yesno,'y'))
        cxx_custom = input('Please specify which compiler to use: ', 's');
        cxx_custom = [ ' GCC=''' cxx_custom ''' '];
        fprintf(['Setting ''' cxx_custom '''\n'])
    else
        fprintf('Using default.')
    end
end

% This function checks the Matlab version.
function matlabVerCheck()

fprintf('\n- Checking MATLAB version and operating system ...\n');
mver = ver('MATLAB');
% Sometimes we get multiple products here (no idea why), ensure we have MATLAB
if (length(mver) > 1)
    for i = 1:length(mver)
        if (strcmp(mver(i).Name, 'MATLAB'))
            mver = mver(i);
            break;
        end
    end
end

vv = regexp(mver.Version,'\.','split');
if(str2double(vv{1}) < 8)
    if(str2double(vv{2}) < 12)
        if(str2double(vv{2}) < 10)
            error('MATLAB 2011a or above is required to run the Interface - sorry!');
        else %2010a/b/sp1
            fprintf(2,'Matlab-SCIP Interface is designed for MATLAB 2011a or above.\nIt will install into 2010a, but you may experience reliability problems.\nPlease upgrade to R2011a or later.\n');
        end
    end
end

switch(mexext)
    case 'mexw32'
        error('32bit Windows Installations are not supported.');
    case 'mexw64'
        fprintf('MATLAB %s 64bit (Windows x64) detected.\n',mver.Release);
    case 'mexa64'
        fprintf('MATLAB %s 64bit (Linux x64) detected.\n',mver.Release);
    otherwise
        error('The interface cannot be compiled for your operating system - sorry!');
end


% This function checks release versions.
function OK = mexFileCheck(localVer)

fprintf('\n- Checking MEX file release information ...\n');
% check whether the current OPTI version matches the mex files
[mexFilesOK, mexBuildVer] = checkMexFileVersion(localVer, false);
if (mexFilesOK == false)
    % one or more mex files are out of date, report to the user
    if (isnan(mexBuildVer))
        fprintf(2,'MEX file is not compatible with this version of the Interface.\n');
    else
        fprintf(2,'MEX file is not compatible with this version of the Interface (MEX v%.2f vs Interface v%.2f).\n', mexBuildVer, localVer);
    end
    error('MEX file incompatible.')
else
    % All up to date, nothing to check
    fprintf('MEX files match interface release.\n');
    OK = true;
end

% This function checks the version of the MEX file.
function [OK,buildVer] = checkMexFileVersion(localVer, verbose)

OK = true;
buildVer = localVer;
mexFile = optiSolver('onlyscip');

% Now search through and compare version info
for i = 1:length(mexFile)
    if (~any(strcmpi(mexFile{i},{'matlab'})))
        try
            [~,optiBuildVer] = feval(lower(mexFile{i}));
        catch
            % Initially lots of mex files in the wild which don't support
            % the second arg, assume out of date.
            OK = false;
            buildVer = NaN;
            if (verbose)
                fprintf('Error evaluating MEX file: %s.\n', lower(mexFile{i}));
            end
            return;
        end
        if (optiBuildVer ~= localVer)
            if (optiBuildVer > localVer) % unusual case where user has newer mex files than opti source
                fprintf(2,'MEX files are for a newer version of the interface.');
                fprintf(2,'Please update your version of the interface.');
                error('Install Error: Interface is out of date.');
            else
                OK = false;
                buildVer = optiBuildVer;
                if (verbose)
                    fprintf('MEX File ''%s.%s'' is out of date (MEX v%.2f vs Interface v%.2f).\n',mexFile{i},mexext, optiBuildVer, localVer);
                else
                    break; % no point continuing check if not displaying
                end
            end
        end
    end
end


function in = bool2yn(val)
if (isempty(val) || val == true)
    in = 'y';
else
    in = 'n';
end

% This function returns true if we are running Octave.
function retval = isOctave()
persistent cacheval;  % speeds up repeated calls

if isempty (cacheval)
    cacheval = (exist ("OCTAVE_VERSION", "builtin") > 0);
end

retval = cacheval;
