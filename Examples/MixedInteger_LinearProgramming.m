%% OPTI Toolbox Mixed-Integer Linear Programming Examples
%
% This file contains a number of MILP problems and demonstrates how to
% solve them using the OPTI Toolbox.
%
%   Copyright (C) 2014 Jonathan Currie (IPL)

%% Determing which Solver to Use
% OPTI Toolbox comes with a number of MILP solvers, thus to determine which
% ones are available on your system you can type:
clc
optiSolver('MILP')

%% Example 1
% This is a simple two decision variable MILP which will use for the next
% few examples:
clc
f = -[6 5]';                % objective function (min f'x)
A = [1,4; 6,4; 2, -5];      % linear inequality constraints (Ax <= b)
b = [16;28;6];
lb = [0;0];                 % bounds on x (lb <= x <= ub)
ub = [10;10];
xtype = 'II';               % integer variables (I = integer, C = continuous, B = binary)

% Building an MILP problem is very similar to an LP, except just add the
% 'xtype' argument for integer variables:
Opt = opti('f',f,'ineq',A,b,'bounds',lb,ub,'xtype',xtype)

% Call solve to solve the problem:
[x,fval,exitflag,info] = solve(Opt)

%% Example 2 - Alternative Integer Setup
% You can also supply an vector of integer indices indicating the position of
% continuous and integer variables respectively (note no binary variables
% can be entered this way).

xtype = [1 2];

Opt = opti('f',f,'ineq',A,b,'bounds',lb,ub,'xtype',xtype);
solve(Opt);

%% Example 3 - Plotting the Solution
% Several problem types have a default plot command available. Note for
% MILP plots it will also plot the integer constraints.

plot(Opt)

% Example 6 - Sparse MILPs
% As with LPs, all solvers are setup to directly solve sparse systems, which
% is the preferred format for most solvers:
clc
% A larger sparse MILP:
load sparseMILP1;

opts = optiset('solver','scip');    % solve with SCIP
Opt = opti('f',f,'ineq',A,b,'eq',Aeq,beq,'xtype',find(xint),'options',opts)
[x,fval,exitflag,info] = solve(Opt);
fval
info

%% Problem 4
% MILP with Special Ordered Sets (SOS)
clc
% problem
f = [-1 -1 -3 -2 -2]';
A = [-1 -1 1 1 0;
      1 0 1 -3 0];
b = [30;30];
lb = zeros(5,1);
ub = [40;1;inf;inf;1];

% SOS type 1
sos_type = '1';
sos_index = [1 2 3 4 5]';
sos_weight = [1 2 3 4 5]';

% Build the problem, specifying the three SOS fields. Note only some MILP
% solvers are setup to solve problems with SOS:
opts = optiset('solver','scip');
Opt = opti('f',f,'ineq',A,b,'bounds',lb,ub,'sos',sos_type,sos_index,sos_weight,'options',opts)

[x,fval,exitflag,info] = solve(Opt)
