function options = optiset(varargin)
%OPTISET  Create or alter the options for Optimization with OPTI
%
% options = optiset('param1',value1,'param2',value2,...) creates an OPTI
% options structure with the parameters 'param' set to their corresponding
% values in 'value'. Parameters not specified will be set to the OPTI
% default.
%
% options = optiset(oldopts,'param1',value1,...) creates a copy of the old
% options 'oldopts' and then fills in (or writes over) the parameters
% specified by 'param' and 'value'.
%
% options = optiset() creates an options structure with all fields set to
% OPTI defaults.
%
% solverOpts can pass parameters to the solvers, e.g., by
% sopts = {'limits/solutions',1};
% opts = optiset('solverOpts',sopts);
%
% optiset() prints a list of all possible fields and their function.

% Copyright (C) 2011 Jonathan Currie (IPL)
% Copyright (C) 2023 Marc Pfetsch

% Print out possible values of properties.
if((nargin == 0) && (nargout == 0))
    printfields();
    return
end

% Names and Defaults
Names = {'solver';'maxiter';'maxnodes';'maxtime';'tolrfun';'tolafun';'tolint';'solverOpts';'warnings';'display';'probfile';'presolvedfile'};
Defaults = {'auto';1e8;1e8;3600;1e-6;1e-6;1e-6;[];'off';'iter';[];[]};

% Enter and check user args
try
    options = opticheckset(Names,Defaults,@checkfield,varargin{:});
catch ME
    throw(ME);
end

% Ensure backwards compatible
if(strcmpi(options.warnings,'on'))
    options.warnings = 'all';
elseif(strcmpi(options.warnings,'off'))
    options.warnings = 'none';
end



function newvalue = checkfield(field,value)
  newvalue = value;
  % Check a field contains correct data type
  switch lower(field)
    % Scalar non negative double
    case {'tolafun','tolrfun','tolint'}
        err = opticheckval.checkScalarNonNeg(value,field);
    % Scalar non zero double
    case 'maxtime'
        err = opticheckval.checkScalarGrtZ(value,field);
    % Scalar non zero integer
    case {'maxiter','maxnodes'}
        err = opticheckval.checkScalarIntGrtZ(value,field);
    % Struct
    case {'solveropts'}
        err = opticheckval.checkStruct(value,field);
    %Other misc
    case 'display'
        err = opticheckval.checkValidString(value,field,{'off','iter','full'});
    case 'warnings'
        err = opticheckval.checkValidString(value,field,{'on','critical','off','all','none'});
    case 'solver'
        err = opticheckval.checkOptiSolver(value,field);
    case {'probfile'}
        err = opticheckval.checkChar(value,field);
    case {'presolvedfile'}
        err = opticheckval.checkChar(value,field);
    otherwise
        err = MException('OPTI:SetFieldError','Unrecognized parameter name ''%s''.', field);
  end
  if ( ~isempty(err) )
    throw(err);
  end


function printfields()
% print out fields with defaults

solvers = [{'AUTO'} optiSolver('all')];
len = length(solvers);
str = '';
for i = 1:len
    if(i < len)
        str = [str solvers{i} ', ']; %#ok<AGROW>
    else
        str = [str solvers{i}]; %#ok<AGROW>
    end
end
str = regexprep(str,'AUTO','{AUTO}');

fprintf('\n OPTISET Fields:\n');
fprintf(['            solver: [ ' str ' ] \n']);
fprintf('           maxiter: [ Maximum Solver Iterations {1.5e3} ] \n');
fprintf('          maxnodes: [ Maximum Integer Solver Nodes {1e4} ] \n');
fprintf('           maxtime: [ Maximum Solver Evaluation Time {1e3s} ] \n');
fprintf('           tolrfun: [ Relative Function Tolerance {1e-7} ] \n');
fprintf('           tolafun: [ Absolute Function Tolerance {1e-7} ] \n');
fprintf('            tolint: [ Absolute Integer Tolerance {1e-5} ] \n');
fprintf('        solverOpts: [ Solver Specific Options Structure (e.g. for SCIP(SDP))] \n');
fprintf('          warnings: [ ''all'' or {''critical''} or ''none'' ] \n');
fprintf('           display: [ ''off'' or ''iter'' or ''full'' ] \n');
fprintf('          probfile: [ Write problem to file: {[]}, ''filename''] \n');
fprintf('     presolvedfile: [ Write presolved problem to file: {[]}, ''filename''] \n');
