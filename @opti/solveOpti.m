function [x,fval,exitflag,info] = solveOpti(optObj,x0)
%SOLVE an OPTI object
%
%   Called By opti Solve
%   Copyright (C) 2011-2013 Jonathan Currie (IPL)

%Allocate input args
prob = optObj.prob; 
opts = optObj.opts;
nl = optObj.nlprob;

%Check initial point
if(~isempty(x0))
    if(numel(x0) ~= optObj.prob.sizes.ndec), error('x0 is not the correct length! Expected %d x 1',optObj.prob.sizes.ndec);end
    if(size(x0,2) > 1), x0 = x0'; end     
    if(any(isnan(x0))), error('Initial Guess (x0) Contains NaN!'); end
    if(any(isinf(x0))), error('Initial Guess (x0) Contains Inf!'); end
    %Ensure dense starting guess
    x0 = full(x0);
    %Save x0 to all problem structures
    if(~isempty(prob)), prob.x0 = x0; end
    if(~isempty(nl)), nl.x0 = x0; end
else
    if(~isempty(nl) && isfield(nl,'x0') && ~isempty(nl.x0))
        if(any(isnan(nl.x0))), error('Initial Guess (x0) Contains NaN!'); end
        if(any(isinf(nl.x0))), error('Initial Guess (x0) Contains Inf!'); end
    elseif(~isempty(prob) && ~isempty(prob.x0))
        if(any(isnan(prob.x0))), error('Initial Guess (x0) Contains NaN!'); end
        if(any(isinf(prob.x0))), error('Initial Guess (x0) Contains Inf!'); end
    end
end


%Switch based on problem type
switch(prob.type)
    case 'SLE'
        [x,fval,exitflag,info] = solveSLE(prob,opts);
    case 'LP'
        [x,fval,exitflag,info] = solveLP(prob,nl,opts);
    case 'MILP'
        [x,fval,exitflag,info] = solveMILP(prob,nl,opts);
    case 'BILP'
        [x,fval,exitflag,info] = solveBILP(prob,nl,opts);
    case 'QP'
        [x,fval,exitflag,info] = solveQP(prob,nl,opts);
    case 'QCQP'
        [x,fval,exitflag,info] = solveQCQP(prob,nl,opts);
    case 'MIQP'
        [x,fval,exitflag,info] = solveMIQP(prob,nl,opts);
    case 'MIQCQP'
        [x,fval,exitflag,info] = solveMIQCQP(prob,nl,opts);
    case 'SDP'
        [x,fval,exitflag,info] = solveSDP(prob,opts);
    case 'MISDP'
        [x,fval,exitflag,info] = solveMISDP(prob,opts);
    case 'SNLE'
        [x,fval,exitflag,info] = solveSNLE(nl,opts,optObj.prob.misc.forceSNLE);
    case 'SCNLE'
        [x,fval,exitflag,info] = solveSCNLE(nl,opts,optObj.prob.misc.forceSNLE);
    case {'DNLS','NLS'}
        [x,fval,exitflag,info] = solveNLS(nl,opts);
    case 'UNO'
        [x,fval,exitflag,info] = solveUNO(nl,opts);    
    case 'NLP'
        [x,fval,exitflag,info] = solveNLP(nl,opts);
    case 'MINLP'
        [x,fval,exitflag,info] = solveMINLP(nl,opts);        
    otherwise
        error('Problem Type ''%s'' is not implemented yet',prob.type);
end


function [x,fval,exitflag,info] = solveSLE(prob,opts)
%Solve a System of Linear Equations
fval = [];
exitflag = [];
error('The solver %s cannot be used to solve a LSE',opts.solver);


function [x,fval,exitflag,info] = solveLP(p,nl,opts)
%Solve a Linear Program using a selected solver

switch(opts.solver)       
    case 'scip'
        [x,fval,exitflag,info] = opti_scip([],p.f,p.A,p.rl,p.ru,p.lb,p.ub,[],[],[],opts);
    otherwise
        error('The Solver %s cannot be used to solve a LP',opts.solver);        
end

function [x,fval,exitflag,info] = solveMILP(p,nl,opts)
%Solve a Mixed Integer Linear Program using a selected solver

switch(opts.solver)
    case 'scip'
        [x,fval,exitflag,info] = opti_scip([],p.f,p.A,p.rl,p.ru,p.lb,p.ub,p.int.str,p.sos,[],opts);
    otherwise
        error('The Solver %s cannot be used to solve a MILP',opts.solver);
end

function [x,fval,exitflag,info] = solveBILP(p,nl,opts)
%Solve a Linear Program using a selected solver

switch(opts.solver)       
    case 'scip'
        [x,fval,exitflag,info] = opti_scip([],p.f,p.A,p.rl,p.ru,p.lb,p.ub,p.int.str,p.sos,[],opts);
    otherwise
        error('The Solver %s cannot be used to solve a BILP',opts.solver);        
end


function [x,fval,exitflag,info] = solveQP(p,nl,opts)
%Solve a Quadratic Program using a selected solver

%Check for unconstrained QP
if(~p.iscon)
    %Solve an Unconstrained QP
    t = tic;
    x = -p.H\p.f;
    info = struct('Iterations',[],'Time',toc(t),'Algorithm','MATLAB: mldivide','StatusString',[]);
    fval = 0.5*x'*p.H*x + p.f'*x;
    exitflag = 1;
else
    switch(opts.solver)       
        case 'scip'
            [x,fval,exitflag,info] = opti_scip(p.H,p.f,p.A,p.rl,p.ru,p.lb,p.ub,[],[],[],opts);
        otherwise
            error('The Solver %s cannot be used to solve a QP',opts.solver);        
    end
end

function [x,fval,exitflag,info] = solveQCQP(p,nl,opts)
%Solve a Quadratically Constrained Program using a selected solver

%Form QC structure
qc.Q = p.Q; qc.l = p.l; qc.qrl = p.qrl; qc.qru = p.qru;

switch(opts.solver)      
    case 'scip'
        [x,fval,exitflag,info] = opti_scip(p.H,p.f,p.A,p.rl,p.ru,p.lb,p.ub,[],[],qc,opts);
    otherwise
        error('The Solver %s cannot be used to solve a QCQP',opts.solver);        
end

function [x,fval,exitflag,info] = solveMIQP(p,nl,opts)
%Solve a Mixed Integer Quadratic Program using a selected solver

switch(opts.solver)      
    case 'scip'
        [x,fval,exitflag,info] = opti_scip(p.H,p.f,p.A,p.rl,p.ru,p.lb,p.ub,p.int.str,p.sos,[],opts);    
    otherwise
        error('The Solver %s cannot be used to solve a MIQP',opts.solver);        
end

function [x,fval,exitflag,info] = solveMIQCQP(p,nl,opts)
%Solve a Mixed Integer Quadratically Constrained Quadratic Program using a selected solver

%Form QC structure
qc.Q = p.Q; qc.l = p.l; qc.qrl = p.qrl; qc.qru = p.qru;

switch(opts.solver)      
    case 'scip'
        [x,fval,exitflag,info] = opti_scip(p.H,p.f,p.A,p.rl,p.ru,p.lb,p.ub,p.int.str,p.sos,qc,opts);  
    otherwise
        error('The Solver %s cannot be used to solve a MIQCQP',opts.solver);        
end

function [x,fval,exitflag,info] = solveSDP(p,opts)
%Solve a Semidefinite Programming problem using a selector solver
switch(opts.solver) 
    case 'scipsdp'
        [x,fval,exitflag,info] = opti_scipsdp(p.f,p.A,p.rl,p.ru,p.lb,p.ub,p.sdcone,p.int.str,opts);
    otherwise
        error('The Solver %s cannot be used to solve a MISDP',opts.solver);  
end

function [x,fval,exitflag,info] = solveMISDP(p,opts)
%Solve a Mixed Integer Semidefinite Programming problem using a selector solver
switch(opts.solver) 
    case 'scipsdp'
        [x,fval,exitflag,info] = opti_scipsdp(p.f,p.A,p.rl,p.ru,p.lb,p.ub,p.sdcone,p.int.str,opts);
    otherwise
        error('The Solver %s cannot be used to solve a MISDP',opts.solver);  
end


function [x,fval,exitflag,info] = solveSNLE(nl,opts,force)
%Solve a Nonlinear Equation Problem using a selected solver

    error('Nonlinear Equation problems using a selected solver are not supported',opts.solver);

function [x,fval,exitflag,info] = solveSCNLE(nl,opts,force)
%Solve a Constrained Nonlinear Equation Problem using a selected solver

   error('Constrained Nonlinear Equation problems using a selected solver are not supported',opts.solver);

function [x,fval,exitflag,info] = solveNLS(nl,opts)
%Solve a Nonlinear Least Squares Problem using a selected solver

    error('Nonlinear Least Squares Problems using a selected solver are not supported',opts.solver);

function [x,fval,exitflag,info] = solveUNO(nl,opts)
%Solve an Unconstrained Nonlinear Problem

switch(opts.solver)
    case 'scip'
        [x,fval,exitflag,info] = opti_scipnl(nl.fun,[],[],[],[],[],[],[],[],[],nl.x0,nl.options);
    otherwise
        error('The Solver %s cannot be used to solve this type of problem',opts.solver);
end


function [x,fval,exitflag,info] = solveNLP(nl,opts)
%Solve a Nonlinear Program using a selected solver

if(~isfield(nl,'x0')), error('You must supply x0 to solve an NLP'); end

switch(opts.solver)
    case 'scip'
        [x,fval,exitflag,info] = opti_scipnl(nl.fun,nl.A,nl.rl,nl.ru,nl.lb,nl.ub,nl.nlcon,nl.cl,nl.cu,nl.int.str,nl.x0,nl.options);
    otherwise
        error('The Solver %s cannot be used to solve a NLP',opts.solver);
end

function [x,fval,exitflag,info] = solveMINLP(nl,opts)
%Solve a Mixed Integer Nonlinear Program using a selected solver

%Should not happen, but worthwhile to check
if(isempty(nl))
    error('The Solver %s cannot be used to solve a MINLP',opts.solver);
end

if(~isfield(nl,'x0')), error('You must supply x0 to solve an MINLP'); end

switch(opts.solver)
    case 'scip'
        [x,fval,exitflag,info] = opti_scipnl(nl.fun,nl.A,nl.rl,nl.ru,nl.lb,nl.ub,nl.nlcon,nl.cl,nl.cu,nl.int.str,nl.x0,nl.options);
    otherwise
        error('The Solver %s cannot be used to solve a MINLP',opts.solver);
end


function info = matlabInfo(output,lambda,t,alg)
%Convert Matlab Output to opti info structure

if(isempty(output))
    output.iterations = [];
    output.message = [];
end
if(~isfield(output,'message'))
    output.message = 'This is does not appear to be a MATLAB solver - You have overloaded it?';
end
if(~isfield(output,'algorithm'))
    ALG = 'MATLAB';
else
    ALG = ['MATLAB: ' alg ' - ' output.algorithm];
end
if(isfield(output,'iterations'))
    info = struct('Iterations',output.iterations);
else
    info = struct('Nodes',output.numnodes);
end
info.Time = t;
info.Algorithm = ALG;
if(~isempty(output.message))
    msg = regexp(output.message,'\n','split');
    info.Status = msg{1};
else
    info.Status = '';
end

if(~isempty(lambda))
    info.Lambda = lambda;
end


function info = gmatlabInfo(output,t,alg)
%Convert GMatlab Output to opti info structure

if(isempty(output))
    output.iterations = [];
    output.message = [];
end
if(~isfield(output,'message'))
    output.message = 'This is does not appear to be a MATLAB solver  - You have overloaded it?';
end

if(isfield(output,'iterations'))
    info.Iterations = output.iterations;
elseif(isfield(output,'generations'))
    info.Iterations = output.generations;
else
    info.Iterations = [];
end
info.Time = t;
info.Algorithm = ['GMATLAB: ' alg];
info.Status = output.message;                       