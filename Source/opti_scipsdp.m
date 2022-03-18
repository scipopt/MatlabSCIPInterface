function [x,fval,exitflag,info] = opti_scipsdp(f,A,lhs,rhs,lb,ub,sdp,xtype,x0,opts)
%OPTI_SCIPSDP Solve a MISDP using SCIP-SDP
%
%   min f'*x      subject to:     lhs <= A*x <= rhs
%                                 lb <= x <= ub
%                                 for i = 1..n: xi in Z
%                                 for j = 1..m: xj in {0,1}
%
%   x = opti_scipsdp(f,A,lhs,rhs,lb,ub,sdp,xtype,sos,opts,x0) solves an
%   SDP/MISDP where f is the objective vector, A, lhs, rhs are the
%   linear constraints, lb, ub are the variable bounds, sdp are SDP
%   constraints, and xtype are variables types ('C', 'I', 'B'). The
%   point x0 provides a primal solution.
%
%   [x,fval,exitflag,info] = opti_scipsdp(...) returns an optimal
%   solution x, the objective value fval, a solver exitflag, and an
%   information structure.
%
%   This is a wrapper for SCIP-SDP using the mex interface.
%
%   Copyright (C) 2020- Marc Pfetsch (TU Darmstadt)
%
%   Based on code by Jonathan Currie for opti_scip().

t = tic;

% handle missing arguments
if nargin < 10, opts = optiset; end
if nargin < 9, x0 = []; end
if nargin < 8, xtype = repmat('C',size(f)); end
if nargin < 7, sdp = []; end
if nargin < 6, ub = []; end
if nargin < 5, lb = []; end
if nargin < 4, error('You must supply at least 4 arguments to opti_scipsdp.'); end

warn = strcmpi(opts.warnings,'all');

% check sparsity
if(~issparse(A))
    if(warn)
        optiwarn('opti:sparse','The A matrix should be sparse, correcting: [sparse(A)].');
    end
    A = sparse(A);
end

% MEX contains error checking
[x,fval,exitflag,stats] = scipsdp(f, A, lhs, rhs, lb, ub, sdp, xtype, x0, opts);

% assign outputs
info.BBNodes = stats.BBnodes;
info.BBGap = stats.BBgap;
info.PrimalBound = stats.PrimalBound;
info.DualBound = stats.DualBound;
info.Time = toc(t);
info.Algorithm = 'SCIP-SDP: SDP-based Branch and Bound';

% process return code
[info.Status,exitflag] = scipRetCode(exitflag);

% return display level
function print_level = dispLevel(lev)
switch(lower(lev))
    case 'off'
        print_level = 0;
    case 'iter'
        print_level = 4;
    case 'final'
        print_level = 3;
end
