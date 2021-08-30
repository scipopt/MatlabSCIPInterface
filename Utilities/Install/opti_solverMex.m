function opti_solverMex(name,src, cxx_custom,inc,lib, opts)
%OPTI_SOLVERMEX  Compiles a MEX Interface to an OPTI Solver
%
%   opti_solverMex(name,src,inc,libs,opts)
%
%       src:    Source Files to Compile
%       inc:    Include Directories to Add
%       libs:   Static Libraries to Compile Against
%       opts:   Options Structure with fields below (all optional):
%
%           verb:       True for compiler verbosity (-v)
%           debug:      True for debug build (-g)
%           pp:         Cell array of preprocessors to add (-D)
%           expre:      Extra arguments for mex before source file (one string)
%           quiet:      Don't print anything

% Process Options
if(nargin > 4)
    if(~isfield(opts,'verb') || isempty(opts.verb)), opts.verb = false; end
    if(~isfield(opts,'debug') || isempty(opts.debug)), opts.debug = false; end
    if(~isfield(opts,'pp')), opts.pp = []; end
    if(~isfield(opts,'expre')), opts.expre = []; end
    if(~isfield(opts,'quiet') || isempty(opts.quiet)), opts.quiet = false; end
else
    opts.verb = false;
    opts.debug = false;
    opts.pp = [];
    opts.expre = [];
    opts.quiet = false;
end

% remove existing mex file from memory
clear(name);

if (~opts.quiet)
    fprintf('\n------------------------------------------------\n');
    fprintf('%s MEX FILE INSTALL\n\n',upper(name));
end

% build source file string
if(iscell(src))
    src_str = src{1};
    for i = 2:length(src)
        src_str = sprintf('%s %s',src_str,src{i});
    end
else
    src_str = src;
end

% build include string
if(iscell(inc))
    inc_str = [' -I' inc{1}];
    for i = 2:length(inc)
        inc_str = sprintf('%s -I%s',inc_str,inc{i});
    end
elseif(~isempty(inc))
    inc_str = [' -I' inc];
else
    inc_str = '';
end
inc_str = [inc_str ' -Iopti '];
inc_str = [inc_str ' -IInclude '];

% build library string
switch(computer)
case 'PCWIN64'
    matlabpath = strrep(matlabroot, '\', '\\');
    lib = [lib ' -L''' matlabpath '\extern\lib\win64\mingw64'' -lut '];
case 'GLNXA64'
    lib = [lib ' -L''/usr/local/lib'' -lut '];
case 'x86_64-pc-linux-gnu'
    lib = [lib ' -L''/usr/lib'' -lutil '];
end

% post messages (output name + preprocessors)
post = [' -output ' name];
if(isfield(opts,'pp') && ~isempty(opts.pp))
    if(iscell(opts.pp))
        for i = 1:length(opts.pp)
            post = sprintf('%s -D%s',post,opts.pp{i});
        end
    else
        post = [post ' -D' opts.pp];
    end
end

% other options
if(opts.verb)
    verb = ' -v ';
else
    verb = ' ';
end

if(opts.debug)
    debug = ' -g ';
    post = [post ' -DDEBUG']; % some mex files use this to provide extra info
else
    debug = ' ';
end
if(~isempty(opts.expre))
    opts.expre = [' ' opts.expre ' '];
end

% extra preprocessor defines
if (isOctave())
    mver = ver('octave');
    mver = mver.Version;
    if (mver(1) == '('), mver = mver(2:end-1); end
    post = [post ' -DML_VER=' mver ' -DOPTI_VER='  sprintf('%.2f',optiver)];
else
    mver = ver('matlab');
    mver = mver.Release;
    if (mver(1) == '('), mver = mver(2:end-1); end
    post = [post ' -DML_VER=' mver ' -DOPTI_VER='  sprintf('%.2f',optiver)];
end

% cd to source directory
cdir = pwd();
cd 'Source';

% compile & move
try
    if(isOctave())
        evalstr = ['mkoctfile --mex' verb debug cxx_custom opts.expre src_str inc_str lib post];
        if (~opts.quiet)
            fprintf('MEX Call:\n%s\n\n',evalstr);
        end
        try
            eval(evalstr)
        catch e
            if(exist([name '.' mexext], 'file')==3)
                movefile([name '.' mexext],'../','f')
            end
            if (~opts.quiet)
                fprintf('Done!\n');
            end
            rethrow(e);
        end
        if(exist([name '.' mexext], 'file')==3)
                movefile([name '.' mexext],'../','f')
        end
        if (~opts.quiet)
            fprintf('Done!\n');
        end
    else
        evalstr = ['mex' verb debug '-largeArrayDims ' cxx_custom opts.expre src_str inc_str lib post];
        if (~opts.quiet)
            fprintf('MEX Call:\n%s\n\n',evalstr);
        end
        try
            eval(evalstr)
        catch e
            if(exist([name '.' mexext], 'file')==3)
                movefile([name '.' mexext],'../','f')
            end
            if (~opts.quiet)
                fprintf('Done!\n');
            end
            rethrow(e);
        end
        if(exist([name '.' mexext], 'file')==3)
                movefile([name '.' mexext],'../','f')
        end
        if (~opts.quiet)
            fprintf('Done!\n');
        end
    end
catch ME
    cd(cdir);
    fprintf(2,'Error Compiling %s!\n%s',upper(name),ME.message);
end
cd(cdir);

% This function returns true if we are running Octave.
function retval = isOctave()
    persistent cacheval;  % speeds up repeated calls

    if isempty (cacheval)
      cacheval = (exist ("OCTAVE_VERSION", "builtin") > 0);
    end

    retval = cacheval;
