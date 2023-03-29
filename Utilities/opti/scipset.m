function options = scipset(varargin)
%SCIPSET  Create or alter the options for Optimization with SCIP
%
% options = scipset('param1',value1,'param2',value2,...) creates a SCIP
% options structure with the parameters 'param' set to their corresponding
% values in 'value'. Parameters not specified will be set to the SCIP
% default.
%
% options = scipset(oldopts,'param1',value1,...) creates a copy of the old
% options 'oldopts' and then fills in (or writes over) the parameters
% specified by 'param' and 'value'.
%
% options = scipset() creates an options structure with all fields set to
% SCIPSET defaults.
%
% scipset() prints a list of all possible fields and their function.
%
% See supplied SCIP Documentation for further details of these options.

%   Copyright (C) 2013 Jonathan Currie (IPL)

% Print out possible values of properties.
if ((nargin == 0) && (nargout == 0))
    printfields();
    return
end

% names and defaults
Names = {'globalEmphasis','heuristicsEmphasis','presolvingEmphasis','separatingEmphasis'};
Defaults = {'default','default','default','default'};

% enter and check user args
try
    options = opticheckset(Names,Defaults,@checkfield,varargin{:});
catch ME
    throw(ME);
end


% Check a field contains correct data type.
function checkfield(field,value)
switch lower(field)
    % specific emphasis
    case {'heuristicsemphasis', 'presolvingemphasis', 'separatingemphasis'}
        err = opticheckval.checkValidString(value, field, {'aggressive','fast','off','default'});
    % global emphasis
    case 'globalemphasis'
        err = opticheckval.checkValidString(value, field, {'counter','cpsolver','easycip','feasibility','hardlp','optimality','default'});
    otherwise
        err = MException('OPTI:SetFieldError','Unrecognized parameter name ''%s''.', field);
end
if(~isempty(err)), throw(err); end


% Print out fields with defaults.
function printfields()
fprintf('     globalEmphasis: [ Global predefined parameter setting for emphasis: {''default''}, ''counter'', ''cpsolver'', ''easycip'', ''feasibility'', ''hardlp'', ''optimality'' ] \n');
fprintf(' heuristicsEmphasis: [ Heuristics emphasis predefined parameter setting (overrides global): {''default''}, ''aggressive'', ''fast'', ''off'' ] \n');
fprintf(' presolvingEmphasis: [ Presolving emphasis predefined parameter setting (overrides global): {''default''}, ''aggressive'', ''fast'', ''off'' ] \n');
fprintf(' separatingEmphasis: [ Separating emphasis predefined parameter setting (overrides global): {''default''}, ''aggressive'', ''fast'', ''off'' ] \n');
fprintf('\n');


