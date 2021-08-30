function [prob,opts,nlprob] = buildConfig(prob,opts)
%BUILDCONFIG  Setup Solver Dependent Configuration
%
%   This function builds the problem and options configuration suitable for
%   solving a problem with a specified solver. It is not designed to be
%   called by the user.

%   Copyright (C) 2011 Jonathan Currie (IPL)

%Get Warning Level
if(strcmpi(opts.warnings,'all'))
    warn = 2;
elseif(strcmpi(opts.warnings,'critical'))
    warn = 1;
else
    warn = 0;
end

nlprob = []; %default we don't have a nonlinear problem

%--Check & Build Config--%
switch(lower(opts.solver))
    case 'scip'
        [prob,nlprob,opts] = setupSCIP(prob,opts,warn);
    case 'scipsdp'
        [prob,nlprob,opts] = setupSCIPSDP(prob,opts,warn);
    otherwise
        error('Unknown solver: %s',opts.solver);
end

%Build General Objective & Constraint Callbacks
prob.objective = buildObjectiveCallback(prob,warn);
prob.constraints = buildConstraintsCallback(prob);
    
% -- SCIP -- %
function [prob,nlprob,opts] = setupSCIP(prob,opts,warn)
%Add objbias to options
opts.objbias = prob.objbias;

%nonlinear
if(isempty(prob.fun)) %linear / quadratic /sdp
    prob = fixLin('row',prob,warn,'SCIP'); %Check & Fix Linear Constraints
    prob = fixSparsity(prob,warn,'SCIP'); %Check & Fix Sparsity
    nlprob = [];
else %nonlinear
    nlprob = prob;
    %Setup constraints
    nlprob = fixLin('row',nlprob,warn,'SCIP'); %Check & Fix Linear Constraints
    nlprob = fixSparsity(nlprob,warn,'SCIP'); %Check & Fix Sparsity
    nlprob = fixNlin('row',nlprob,warn,'SCIP'); %Check & Fix Nonlinear Constraints
    %Setup Options (note options processing done in opti_scipnl)
    nlprob.options = opts;
end

% -- SCIPSDP -- %
function [prob,nlprob,opts] = setupSCIPSDP(prob,opts,warn)
%Add objbias to options
opts.objbias = prob.objbias;

%nonlinear
prob = fixLin('row',prob,warn,'SCIPSDP'); %Check & Fix Linear Constraints
prob = fixSparsity(prob,warn,'SCIPSDP'); %Check & Fix Sparsity
nlprob = [];

%Build general objective callback function
function obj = buildObjectiveCallback(prob,warn)
switch(lower(prob.type))
    case 'sle'
        obj = @(x) sum((prob.A*x - prob.b).^2);
    case {'lp','milp','bilp','sdp','misdp'}
        if(prob.objbias)
            obj = @(x) prob.f'*x + prob.objbias;
        else
            obj = @(x) prob.f'*x;
        end
    case {'qp','miqp','qcqp','miqcqp'}
        %See if H is symmetric      
        if(~optiIsSymmetric(prob.H))
            %Determine if tril or triu
            switch(optiIsTriULD(prob.H))
                case 'L'
                    H = (prob.H + tril(prob.H,-1).');
                case 'U'
                    H = (prob.H + triu(prob.H,1).');
                otherwise
                    H = prob.H;
                    if(warn)
                        optiwarn('opti:qpH','%s objective H matrix is not symmetric, tri-lower or tri-upper!',prob.type);
                    end
            end
            if(prob.objbias)
                obj = @(x) 0.5*x'*H*x + prob.f'*x + prob.objbias;
            else
                obj = @(x) 0.5*x'*H*x + prob.f'*x;
            end
        else
            if(prob.objbias)
                obj = @(x) 0.5*x'*prob.H*x + prob.f'*x + prob.objbias;
            else
                obj = @(x) 0.5*x'*prob.H*x + prob.f'*x;
            end
        end
    case {'nls','dnls'}
        if(nargin(prob.fun) == 2)
            obj = @(x) sum((prob.fun(x,prob.xdata)-prob.ydata).^2);
        else
            obj = @(x) sum((prob.fun(x)-prob.ydata).^2);
        end  
    case {'snle','scnle'}
        obj = @(x) sum((prob.fun(x)).^2);
    otherwise %assume general nonlinear
        obj = prob.fun;
end



%Build general constraints callback function
function con = buildConstraintsCallback(prob)
%Check if we actually have any constraints
if(prob.sizes.ncon)
    %Convert linear constraints to general form
    if(~isempty(prob.rl))
        [A,b,Aeq,beq] = row2gen(prob.A,prob.rl,prob.ru);
    else
        A = prob.A; b = prob.b; Aeq = prob.Aeq; beq = prob.beq;
    end
    %Convert nonlinear constraints to mixed form
    if(~isempty(prob.cl))
        prob = nrow2mix(prob,0,false);    
    end
    nlcon = prob.nlcon; nlrhs = prob.nlrhs; nle = prob.nle;
    %Convert sdcone to OPTI format
    sdcone = prob.sdcone;
    %Assign callback
    con = @(x) optiConViolation(x,A,b,Aeq,beq,nlcon,nlrhs,nle,prob.Q,prob.l,prob.qrl,prob.qru,prob.lb,prob.ub,prob.int,sdcone);
else
    con = [];
end


%Check and fix infinite bounds
function prob = fixBnds(prob,warn,solver)
if(~isempty(prob.lb))
    ind = isinf(prob.lb);
    if(any(ind))
        if(warn)
            optiwarn('opti:finbnds','%s Config - This solver expects finite lower bounds. This interface will set infinite lower bounds to -1e30, but this may cause numerical issues.',upper(solver));
        end
        prob.lb(ind) = -1e30;
    end
end
if(~isempty(prob.ub))
    ind = isinf(prob.ub);
    if(any(ind))
        if(warn)
            optiwarn('opti:finbnds','%s Config - This solver expects finite upper bounds. This interface will set infinite upper bounds to 1e30, but this may cause numerical issues.',upper(solver));
        end
        prob.ub(ind) = 1e30;
    end
end       

%Convert Linear Constraints to a Form the Solver Accepts
function prob = fixLin(mode,prob,warn,solver)

switch(mode)            
    case 'gen' %Ensure constraints of the form Ainx <= bin, Aeqx <= beq
        if(~isempty(prob.rl) || ~isempty(prob.ru))
            if(warn > 1)
                optiwarn('opti:lincon','%s Config - This solver expects linear constraints of the form A*x <= b, Aeq*x <= beq, correcting.',upper(solver));
            end
            [prob.A,prob.b,prob.Aeq,prob.beq] = row2gen(prob.A,prob.rl,prob.ru);
            %Remove unused fields
            prob.rl = []; prob.ru = [];
        end
        
    case 'row' %Ensure constraints of the form rl <= Ax <= ru
        if(~isempty(prob.b) || ~isempty(prob.beq))
            if(warn > 1)
                optiwarn('opti:lincon','%s Config - This solver expects linear constraints of the form rl <= A*x <= ru, correcting.',upper(solver));
            end
            [prob.A,prob.rl,prob.ru] = gen2row(prob.A,prob.b,prob.Aeq,prob.beq);
            %Remove unused fields
            prob.b = []; prob.Aeq = []; prob.beq = []; 
        end
        
    case 'gen_ineq' %Only Ainx <= bin
        if(~isempty(prob.rl) || ~isempty(prob.ru))
            if(warn > 1)
                optiwarn('opti:lincon','%s Config - This solver expects linear constraints of the form A*x <= b, correcting.',upper(solver));
            end
            [prob.A,prob.b,prob.Aeq,prob.beq] = row2gen(prob.A,prob.rl,prob.ru);
            %Remove unused fields
            prob.rl = []; prob.ru = []; 
        end
        if(~isempty(prob.Aeq))
            if(warn)
                optiwarn('opti:linineq',['%s Config - This solver only supports linear inequality constraints. Equality constraints '...
                                         'will be converted to ''squeezing'' inequalities, but the solver is unlikely to find a solution'],upper(solver));
            end
            prob.A = [prob.A;prob.Aeq;-prob.Aeq];
            prob.b = [prob.b;prob.beq;-prob.beq];   
            %Update sizes
            prob.sizes.nineq = prob.sizes.nineq + 2*prob.sizes.neq;
            prob.sizes.ncon = prob.sizes.ncon + prob.sizes.neq;
            prob.sizes.neq = 0;            
            %Remove unused fields
            prob.Aeq = []; prob.beq = [];
        end
        
    case 'mix' %Constraints of the form rl <= Ax <= ru AND Aeqx <= beq
        if(~isempty(prob.b))
            if(warn > 1)
                optiwarn('opti:lincon','%s Config - This solver expects linear constraints of the form rl <= A*x <= ru (ineq) AND Aeq*x = beq (beq), correcting.',upper(solver));
            end
            [prob.A,prob.rl,prob.ru] = gen2row(prob.A,prob.b,[],[]);
            %Remove unused fields
            prob.b = []; 
        end
        len = length(prob.rl);
        %Now check if any row constraints include equalities, and move if neccessary
        [prob.A,prob.rl,prob.ru,prob.Aeq,prob.beq] = rowe2gene(prob.A,prob.rl,prob.ru,prob.Aeq,prob.beq);
        %Warn if we changed anything
        if(len ~= length(prob.rl) && (warn > 1))
            optiwarn('opti:lincon','%s Config - This solver expects linear constraints of the form rl <= A*x <= ru (ineq) AND Aeq*x = beq (eq), correcting.',upper(solver));
        end
end

%Convert Nonlinear Constraints to a Form the Solver Accepts
function prob = fixNlin(mode,prob,warn,solver)

switch(mode)
    case 'gen'
        if(~isempty(prob.cl) || ~isempty(prob.cu))
            if(warn > 1)
                optiwarn('opti:nlcon','%s Config - This solver expects nonlinear constraints of the form nlcon(x) <=,>=,== nlrhs with type dictated by nle, correcting.',upper(solver));
            end
            prob = nrow2mix(prob); %more complex conversion here (if double bounded)
        end
    case 'row'
        if(~isempty(prob.nlrhs))
            if(warn > 1)
                optiwarn('opti:nlcon','%s Config - This solver expects nonlinear constraints of the form cl <= nlcon(x) <= cu, correcting.',upper(solver));
            end
            prob = nmix2row(prob);          
        end        
end

%Ensure A, Ain, Aeq, H, Arguments are sparse
function prob = fixSparsity(prob,warn,solver)

if(~isempty(prob.A) && ~issparse(prob.A))
    if(warn > 1)
        optiwarn('opti:sparse','%s Config - The A matrix should be sparse, correcting: [sparse(A)]',upper(solver));
    end
    prob.A = sparse(prob.A);
end
if(~isempty(prob.Aeq) && ~issparse(prob.Aeq))
    if(warn > 1)
        optiwarn('opti:sparse','%s Config - The Aeq matrix should be sparse, correcting: [sparse(Aeq)]',upper(solver));
    end
    prob.Aeq = sparse(prob.Aeq);
end
if(~isempty(prob.H) && ~issparse(prob.H) && isnumeric(prob.H))
    if(warn > 1)
        optiwarn('opti:sparse','%s Config - The H matrix should be sparse, correcting: [sparse(H)]',upper(solver));
    end
    prob.H = sparse(prob.H);
end
if(~isempty(prob.Q))
    if(iscell(prob.Q))
        for i = 1:length(prob.Q)
            if(~issparse(prob.Q{i}))
                if(warn > 1)
                    optiwarn('opti:sparse','%s Config - The Q matrix in cell %d, should be sparse, correcting: [sparse(Q{%d})]',upper(solver),i,i);
                end
                prob.Q{i} = sparse(prob.Q{i});
            end
        end
    elseif(~issparse(prob.Q))
        if(warn > 1)
            optiwarn('opti:sparse','%s Config - The Q matrix should be sparse, correcting: [sparse(Q)]',upper(solver));
        end
        prob.Q = sparse(prob.Q);
    end
end 
if(isfield(prob,'sdp') && ~isempty(prob.sdp))
    if(iscell(prob.sdp))
        for i = 1:length(prob.sdp)
            if(~issparse(prob.sdp{i}))
                if(warn > 1)
                    optiwarn('opti:sparse','%s Config - The SDP constraint in cell %d, should be sparse, correcting: [sparse(sdp{%d})]',upper(solver),i,i);
                end
                prob.sdp{i} = sparse(prob.sdp{i});
            end
        end
    elseif(~issparse(prob.sdp))
        if(warn > 1)
            optiwarn('opti:sparse','%s Config - The SDP constraint should be sparse, correcting: [sparse(sdp)]',upper(solver));
        end
        prob.sdp = sparse(prob.sdp);
    end
end 
        
%Ensure H is Sym TRIL
function prob = fixSymHL(prob,warn,solver)

if(~isa(prob.H,'function_handle'))
    if(optiIsSymmetric(prob.H))
        if(warn > 1)
            optiwarn('opti:clp','%s Config - The H matrix should be Symmetric Lower Triangular, correcting: [tril(H)]',upper(solver));
        end
        prob.save.H = prob.H;
        prob.H = tril(prob.H);
    end
end
    
%Ensure H is Sym
function checkSymH(prob,solver)

if(~isempty(prob.H))
    if(~optiIsSymmetric(prob.H))      
        error('%s Config - The H matrix must be Symmetric.',upper(solver));
    end
end

%Setup Data Fitting Objective + Gradient
function [fun,grad,ydata] = setupDataFit(prob,warn,isNLP)
if(nargin < 3), isNLP = false; end

%Skip if done by buildOpti
if(isfield(prob,'misc') && prob.misc.forceSNLE)
    fun = prob.fun;
    grad = prob.f;
    ydata = prob.ydata;
    return;
end

%Warn if conversion is taking place
if(isNLP && warn > 1)
    optiwarn('OPTI:NLSconv','OPTI is converting the SNLE/SCNLE/NLS problem to an NLP to solve. This may effect convergence speed and robustness.');
end

%Data
if(any(strcmpi(prob.type,{'SNLE','SCNLE'})))
    if(~isempty(prob.x0))
        ydata = zeros(size(prob.x0));
    elseif(prob.sizes.ndec > 0)
        ydata = zeros(prob.sizes.ndec,1);
    else
        ydata = prob.ydata;
    end
else
    ydata = prob.ydata;
end

%Weights
wts = prob.weighting;
if(~isempty(wts))
    ydata = ydata.*wts;
    if(isempty(prob.sizes.ndec))
        error('In order to modify the gradient for data fitting weights, OPTI must know the number of decision variables. Please supply x0 or ndec to OPTI.');
    end
    gwts = repmat(wts,1,prob.sizes.ndec); %assume this is correct?
end

%Objective & Gradient Callback Functions
if(~isempty(prob.xdata))
    if(nargin(prob.fun) ~= 2)
        error('When supplying xdata it is expected the objective function will take the form of fun(x,xdata)');
    end    
    if(isNLP)
        %Modify Objective
        if(~isempty(wts))
            fun = @(x) sum((prob.fun(x,prob.xdata).*wts - ydata).^2);
        else
            fun = @(x) sum((prob.fun(x,prob.xdata) - ydata).^2);
        end
        %Check if we have a supplied gradient function
        if(~isempty(prob.f))
            %Check if we are already using mklJac
            if(~isempty(strfind(char(prob.f),'mklJac')))
                %Default gradient
                grad = @(x) mklJac(fun,x,1);
            else
                %Modify user gradient
                if(isempty(prob.sizes.ndec))
                    error('In order to modify the gradient to convert the problem from a NLS to an NLP, OPTI must know the number of decision variables. Please supply x0 or ndec to OPTI.');
                end
                if(nargin(prob.f) == 2) %assume grad also requires xdata
                    if(~isempty(wts))
                        grad = @(x) sum(-2*prob.f(x,prob.xdata).*gwts.*repmat(ydata-prob.fun(x,prob.xdata).*wts,1,prob.sizes.ndec));
                    else
                        grad = @(x) sum(-2*prob.f(x,prob.xdata).*repmat(ydata-prob.fun(x,prob.xdata),1,prob.sizes.ndec));
                    end
                else
                    if(~isempty(wts))
                        grad = @(x) sum(-2*prob.f(x).*gwts.*repmat(ydata-prob.fun(x,prob.xdata).*wts,1,prob.sizes.ndec));
                    else
                        grad = @(x) sum(-2*prob.f(x).*repmat(ydata-prob.fun(x,prob.xdata),1,prob.sizes.ndec));
                    end
                end
            end
        else
            %Default gradient
            grad = @(x)mklJac(fun,x,1);
        end
    else
        if(~isempty(wts))
            fun = @(x) prob.fun(x,prob.xdata).*wts;
        else
            fun = @(x) prob.fun(x,prob.xdata);
        end
        %Check if we have a supplied gradient function
        if(~isempty(prob.f))
            %Gradient needs xdata spec'd from above
            if(~isempty(strfind(char(prob.f),'mklJac')))
                if(strcmpi(prob.type,'DNLS'))
                    grad = @(x)mklJac(fun,x); %not sure at this point how many points we will have
                else
                    %Default gradient
                    grad = @(x)mklJac(fun,x,length(prob.ydata));
                end   
            else
                if(nargin(prob.f) == 2) %assume also requires xdata
                    if(~isempty(wts))
                        grad = @(x) prob.f(x,prob.xdata).*gwts;
                    else
                        grad = @(x) prob.f(x,prob.xdata);
                    end
                else
                    if(~isempty(wts))
                        grad = @(x) prob.f(x).*gwts;
                    else
                        grad = prob.f;
                    end
                end
            end
        else
            if(strcmpi(prob.type,'DNLS'))
                grad = @(x)mklJac(fun,x); %not sure at this point how many points we will have
            else
                %Default gradient
                grad = @(x)mklJac(fun,x,length(prob.ydata));
            end
        end
    end    
else
    if(isNLP)
        if(~isempty(wts))
            fun = @(x) sum((prob.fun(x).*wts - ydata).^2);
        else
            fun = @(x) sum((prob.fun(x) - ydata).^2);
        end
        %Check if we are already using mklJac
        if(~isempty(strfind(char(prob.f),'mklJac')))
            %Simply mklJac fun
            grad = @(x) mklJac(fun,x,1);
        else
            %Modify user gradient
            if(isempty(prob.sizes.ndec))
                error('In order to modify the gradient to convert the problem from a NLS to an NLP, OPTI must know the number of decision variables. Please supply x0 or ndec to OPTI.');
            end
            if(~isempty(wts))
                grad = @(x) sum(-2*prob.f(x).*gwts.*repmat(ydata-prob.fun(x).*wts,1,prob.sizes.ndec));
            else
                grad = @(x) sum(-2*prob.f(x).*repmat(ydata-prob.fun(x),1,prob.sizes.ndec));
            end
        end
    else
        if(~isempty(wts))
            fun = @(x) prob.fun(x).*wts;
            grad = @(x) prob.f(x).*gwts;
        else
            fun = prob.fun;
            grad = prob.f;
        end        
    end    
end


%Setup Nonlinear Inequality Constraints (convert >= to <=)
function [nlcon,nlrhs] = setupNLIneq(prob)
%Build Nonlinear Constraints
if(any(prob.nle == 1)) %if we have >= constraints, have to convert
    max_in = prob.nle == 1;
    nlcon = @(x) nlConLE(x,prob.nlcon,max_in);
    nlrhs = prob.nlrhs;
    nlrhs(max_in) = -nlrhs(max_in);
else
    nlcon = prob.nlcon;
    nlrhs = prob.nlrhs;
end

function c = nlConLE(x,fun,max_in)
% Handle to convert nonlinear >= inequalities to <= inequalities
%Get Constraint Eval
c = fun(x);
%Negate max entries
c(max_in) = -c(max_in);

%Diagnostics state
function state = diagState(print)
if(~strcmpi(print,'off'))
    state = 'on';
else
    state = 'off';
end

%Check Problem Type
function checkPType(solver,ptype,stype)
%Check for error
if(~any(strcmpi(ptype,stype)))
	error('%s is not configured to solve a %s.',upper(solver),upper(ptype));
end

%Check Constraints
function checkCon(solver,sizes,mode)
switch(mode)
    case 'uncon'
        if(sizes.ncon > 0)
            error('%s can only solve unconstrained problems!',upper(solver));
        end
    case 'bound'
        if((sizes.nineq + sizes.neq + sizes.nnlineq + sizes.nnleq + sizes.nqc) > 0)
            error('%s can only solve bounded problems!',upper(solver));
        end
    case 'lincon'
        if((sizes.nnlineq + sizes.nnleq + sizes.nqc) > 0)
            error('%s can only solve bounded and linearly constrained problems!',upper(solver));
        end
        
    case 'lineq'
        if((sizes.neq + sizes.nnlineq + sizes.nnleq + sizes.nqc) > 0)
            error('%s can only solve bounded and linear inequality constrained problems!',upper(solver));
        end 
        
    case 'ineq'
        if((sizes.neq + sizes.nnleq + sizes.nqc) > 0)
            error('%s can only solve bounded and nonlinear inequality constrained problems!',upper(solver));
        end
        
    otherwise
        error('Unknown constraint check');
end
    


