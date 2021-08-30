function ret = optiSolver(instr,err)
%OPTISOLVER  See if a solver is installed on your PC and details about it.
%
%   optiSolver() prints a list of OPTI interfaced solvers, their version
%   numbers and and whether they are available on your PC.
%
%   optiSolver(type) prints a list of solvers for a given problem, 'type',
%   together with a table of constraint and derivative information. See
%   below for heading information. Examples are 'lp', 'milp', etc.
%
%   ok = optiSolver(solver) returns true if the passed solver is available
%   on your PC. Examples are 'clp', 'ipopt', etc.
%
%   optiSolver(solver) displays information about the solver (if
%   available) and configuration specifics (no output args).
%
%   solver = optiSolver(bestSolver) returns the best available solver as a
%   string for a given problem type. Examples are 'best_lp', 'best_milp',
%   etc.
%
%   Problem Type table headings are as follows:
%
%   BD - Bounds
%   LI - Linear Inequality (* indicates a conversion will take place)
%   LE - Linear Equality   (* indicates a conversion will take place)
%   QI - Quadratic Inequality
%   SS - Special Ordered Set
%   NI - Nonlinear Inequality
%   NE - Nonlinear Equality
%   SP - Supports Sparse Matrices
%   D1 - Requires First Derivatives (Gradient) (* indicates optional)
%   D2 - Requires Second Derivatives (Hessian) (* indicates optional)
%   GL - Global Solver

% Copyright (C) 2011-2015 Jonathan Currie (IPL)
% Copyright (C) 2021, Marc Pfetsch

global SLE LP MILP BILP QP QCQP MIQP MIQCQP SDP MISDP SNLE SCNLE NLS UNO NLP MINLP DESC PTYPES

% Ordered lists of 'best' solvers
SLE = {};
LP = {'scip'};
MILP = {'scip'};
BILP = {'scip'};
QP = {'scip'};
QCQP = {'scip'};
MIQP = {'scip'};
MIQCQP = {'scip'};
SDP = {'scipsdp'};
MISDP = {'scipsdp'};
SNLE = {};
SCNLE = {};
NLS = {};
UNO = {'scip'};
NLP = {'scip'};
MINLP = {'scip'};

% description list
DESC = containers.Map({'scip','scipsdp'},{'Solving Constraint Integer Programs','Solving Mixed-Integer SDPs'});

% problem type list
PTYPES = {'sle','lp','milp','bilp','qp','qcqp','miqp','miqcqp','sdp','misdp','snle','scnle','nls','uno','nlp','minlp'};

% default input args
if (nargin < 2), err = 1; end % default to create an error
if (nargin < 1), instr = []; end
% default internal args
ret = [];

if(isempty(instr))
    printSolvers();     % print a list of solvers and availability
elseif (strcmp(instr,'ver'))
    ret = genSolverList();
else
    % check if we have a known solver
    allS = getAllSolvers();
    for i = 1:length(allS)
        if(strcmpi(instr,allS{i}))
            ret = check(instr,err);
            if(nargout == 0 && ret), printSolverInfo(instr); end
            return;
        end
    end

    % if we got here, check for a known problem type
    for i = 1:length(PTYPES)
        if(strcmpi(instr,PTYPES{i}))
            ret = findAllForProb(instr,nargout);
            return;
        end
    end

    if(strfind(instr,'best'))
        ret = findBestSolver(instr);
    elseif(strfind(instr,'all'))
        ret = findAllForProb(instr,nargout);
    elseif(strfind(instr,'onlyscip'))
        ret = {'scip'};
    else
        error('Unknown Solver or Option: %s',instr); % not found
    end

end

% check a particular solver is installed
function ok = check(name,err,ptype)
if(nargin < 3), ptype = []; end
c = which([lower(name) '.' mexext]); %bug fix 22/2/12
if(~isempty(c)) % ensure mex file is OK
    try
        a = eval(lower(name)); %#ok<NASGU>
    catch
        c = [];
    end
end

if(isempty(c))
    if(err)
        throwAsCaller(MException('opti:checkSolver','OPTI cannot find a valid version of %s. Ensure it is installed on your system.',name));
    else
        ok = 0;
    end
else
    ok = 1;
end

%Find the best available solver for a given problem type
function str = findBestSolver(prob)

global SLE LP MILP BILP QP QCQP MIQP MIQCQP SDP MISDP SNLE SCNLE NLS UNO NLP MINLP PTYPES %#ok<NUSED>

s = regexp(prob,'_','split');
if(length(s) < 2)
    error('When requesting the best solver it should be of the form best_xxx, e.g. best_lp.');
end

% Remove 'D' from dynamic problems
if(s{2}(1) == 'D')
    s{2} = s{2}(2:end);
end

fnd = 0;
for i = 1:length(PTYPES)
    if(strcmpi(s{2},PTYPES{i}))
        try
            list = eval(upper(s{2}));
            fnd = 1;
        catch %#ok<CTCH>
            fnd = 0; % global var doesn't exist?
        end
        break;
    end
end
% If not found, we don't have a solver for this problem.
if(~fnd)
    error('Could not find a problem type match for type %s.',upper(s{2}));
% Otherwise return the best solver available.
else
    str = [];
    for i = 1:length(list)
        if(check(list{i},0))
            str = list{i};
            break;
        end
    end
    if(isempty(str))
        error(['You do not have a solver which can solve a %s explicitly.'], upper(s{2}));
    end
end

% Find all available solvers for a given problem type.
function list = findAllForProb(prob,nout)

global SLE LP MILP BILP QP QCQP MIQP MIQCQP SDP MISDP SNLE SCNLE NLS UNO NLP MINLP PTYPES %#ok<NUSED>

% backwards compatibility check
if(strfind(prob,'all_'))
    s = regexp(prob,'_','split');
    prob = s{2};
end

fnd = 0;
for i = 1:length(PTYPES)
    if(strcmpi(prob,PTYPES{i}))
        try
            plist = eval(upper(prob));
            fnd = 1;
        catch %#ok<CTCH>
            fnd = 0; % global var doesn't exist?
        end
        break;
    end
end
% if found keep availale solvers
if(fnd)
    j = 1; list = [];
    for i = 1:length(plist)
        if(check(plist{i},0,prob))
            list{j} = plist{i}; %#ok<AGROW>
            j = j + 1;
        end
    end
% check if we want all ...
else
    if(strcmpi(prob,'all'))
        list = getAllSolvers();
        return;
    else
        error('Unknown problem type: %s\nNote the API has changed in v1.34 to just the problem type, e.g., checkSolver(''lp'').',prob);
    end
end

% if not returning anything, just print
if(~nout)
    printSelSolvers(list,upper(prob))
    list = [];
end

% Create a list of all solvers OPTI is interfaced to (not neccesarily found however).
function list = getAllSolvers()

global SLE LP MILP BILP QP QCQP MIQP MIQCQP SDP MISDP SNLE SCNLE NLS UNO NLP MINLP

fulllist = [SLE LP MILP BILP QP QCQP MIQP MIQCQP SDP MISDP SNLE SCNLE NLS UNO NLP MINLP];

% remove duplicates
fulllist = unique(lower(fulllist));

% sort & return
list = sort(upper(fulllist));

% print each solver and if it exists
function printSolvers()

fprintf('\n------------------------------------------------\n')
fprintf('MATLAB-SCIP-INTERFACE AVAILABLE SOLVERS:\n\n');
list = genSolverList();
for i = 1:length(list)
    fprintf('%s\n',list{i});
end

fprintf('------------------------------------------------\n')

% Generate a display compatible list of all solvers, their availability and version number.
function list = genSolverList()
solv = getAllSolvers();
list = cell(length(solv),1);
for i = 1:length(solv)
    [avail,solVer] = getSolverVer(solv{i});
    list{i} = sprintf('%-10s       %-13s  %s',[solv{i} ':'],avail,solVer);
end

% Print selected solvers.
function printSelSolvers(solv,type)

fprintf('\n------------------------------------------------\n')
fprintf(['OPTI ' upper(type) ' SOLVERS:\n']);
if(isempty(solv))
    fprintf('None Available\n');
else
    [~,hd] = getSolverInfo(solv{1},type);
    len = length(strtrim(hd));
    fprintf(' %s\n',hd); j = 1;
    for i = 1:length(solv)
        if(check(solv{i},0)) % only print if the solver is available
            fprintf(['(%2d) %-10s  %-'  num2str(len+4) 's %s\n'],j,upper(solv{i}),getSolverInfo(solv{i},type),getDesc(solv{i},type));
            j = j + 1;
        end
    end
end
fprintf('------------------------------------------------\n')

% return solver description
function str = getDesc(solv,type)

global DESC

str = DESC(lower(solv));

% return currently installed solver version
function [avail,solVer] = getSolverVer(solver)

ok = check(lower(solver),0);
if(ok)
    avail = 'Available';
    try
        v = feval(lower(solver));
        if(~strcmp(v,''))
            solVer = ['v' v];
        else
            solVer = '';
        end
    catch %#ok<CTCH>
        solVer = '';
    end
else
    avail = 'Not Available';
    solVer = '';
end

% Generate a string of solver information for a particular solver.
function [str,hd] = getSolverInfo(solver,type)

% get solver information
info = optiSolverInfo(solver,type);
% separate out variables we use
bnd     = getMarker(info.con.bnd);
lineq   = getMarker(info.con.lineq);
leq     = getMarker(info.con.leq);
qineq   = getMarker(info.con.qineq);
ss   = getMarker(info.con.sos);
sdp     = getMarker(info.con.sdp);
nlineq  = getMarker(info.con.nlineq);
nleq    = getMarker(info.con.nleq);
der1    = getMarker(info.der1);
der2    = getMarker(info.der2);
glb     = getMarker(info.global);
sp      = getMarker(info.sparse);

lhs = '%-17s';
switch(lower(type))
    case {'lp','qp'}
        hd = sprintf([lhs 'BD LI LE SP'],'');
        str = sprintf(' %-2s %-2s %-2s %-2s',bnd,lineq,leq,sp);
    case {'milp','miqp'}
        hd = sprintf([lhs 'BD LI LE SS SP'],'');
        str = sprintf(' %-2s %-2s %-2s %-2s %-2s',bnd,lineq,leq,ss,sp);
    case 'bilp'
        hd = sprintf([lhs 'LI LE SP'],'');
        str = sprintf(' %-2s %-2s %-2s',lineq,leq,sp);
    case {'qcqp','miqcqp'}
        hd = sprintf([lhs 'BD LI LE QI SP'],'');
        str = sprintf(' %-2s %-2s %-2s %-2s',bnd,lineq,leq,qineq,sp);
    case {'sdp','misdp'}
        hd = sprintf([lhs 'BD LI LE QI SD SP'],'');
        str = sprintf(' %-2s %-2s %-2s %-2s %-2s',bnd,lineq,leq,qineq,sdp,sp);
    case 'nls'
        hd = sprintf([lhs 'BD LI LE D1'],'');
        str = sprintf(' %-2s %-2s %-2s %-2s',bnd,lineq,leq,der1);
    case {'nlp','minlp'}
        hd = sprintf([lhs 'BD LI LE QI NI NE SP D1 D2 GL'],'');
        str = sprintf(' %-2s %-2s %-2s %-2s %-2s %-2s %-2s',bnd,lineq,leq,qineq,nlineq,nleq,sp,der1,der2,glb);
    otherwise % no constraints
        hd = '';
        str = '';
end

function str = getMarker(value)
switch(value)
    case 1
        str = 'x';
    case 0
        str = '';
    case -1
        str = '*';
end
