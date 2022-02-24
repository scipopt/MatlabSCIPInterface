%% OPTI Toolbox Nonlinear Programming Examples
%
% This file contains a number of NLP problems and demonstrates how to
% solve them using the OPTI Toolbox.
%
%   Copyright (C) 2014 Jonathan Currie (IPL)

% There is also a page on the Wiki which supplements this example:
%web('https://www.inverseproblem.co.nz/OPTI/index.php/Probs/NLP');

%% Determing which Solver to Use
% OPTI Toolbox comes with a number of NLP solvers, thus to determine which
% ones are available on your system you can type:
clc
optiSolver('NLP')

%% NLP Considerations - 1st Derivatives
% See DifferentationExamples.m

%% NLP Considerations - Helping the Solver
% The most important point when solving NLPs is to give the solver as much
% information as you know, this should not only speed up the solver, but
% may also result in a better optimum. OPTI Toolbox allows the following
% (but not all solvers may support all the below):
%
% - Gradient Function (f,grad)
%   A function handle which the optimizer can use to determine the 1st
%   derivatives of the objective function.
%
% - Hessian Function (H,hess)
%   A function handle which the optimizer can use to determine the 2nd
%   derivatives of the objective function + constraint functions.
%
% - Hessian Structure (Hstr)
%   A function handle of returning a sparse matrix indicating the non-zero
%   terms within the Hessian. This saves a numerical difference / BFGS update
%   from evaluating these terms in the Hessian.
%
% - Jacobian Function (nljac)
%   A function handle which the optimizer can use to determine the 1st
%   derivatives of the constraint function.
%
% - Jacobian Structure (nljacstr)
%   A function handle of returning a sparse matrix indicating the non-zero
%   terms within the Jacobian.
%
% And most importantly - use a good (i.e. feasible & well chosen) initial
% guess for x0!

%% Example 1
% This is a simple two decision variable NLP which will use for the next
% few examples:
clc

% problem
obj = @(x) 100*(x(2) - x(1)^2)^2 + (1 - x(1))^2; % nl objective
grad = @(x)[2*x(1)-400*x(1)*(x(2)-x(1)^2)-2;     % nl gradient
            200*x(2)-200*x(1)^2];

lb = [-inf; -1.5];      % bounds on x (lb <= x <= ub)
x0 = [-2; 1];           % starting guess of solution

% build OPTI problem
Opt = opti('obj',obj,'grad',grad,'lb',lb)

% Call solve to solve the problem:
[x,fval,exitflag,info] = solve(Opt,x0)

%% Example 2 - Alternative Setup Strategies
% Naming of arguments, as well as pairing is flexible when using opti

Opt = opti('fun',obj,'grad',grad,'lb',lb); %fun = obj

% OR
Opt = opti('obj',obj,'f',grad,'lb',lb); %f = grad

%% Example 3 - Using Numerical Difference for Gradient
% Simply drop 'grad' from the problem to use a numerical difference scheme
% instead - you will note mklJac (a numerical difference routine) is now
% used for the gradient.
clc
Opt = opti('obj',obj,'lb',lb)

x = solve(Opt,x0, xval)

%% Example 4 - Unconstrained Nonlinear Optimization
% If you do not supply any constraints OPTI will solve the problem using an
% unconstrained NL solver. For problems where OPTI cannot determine the
% number of decision variables you must supply this to optiprob:
clc
Opt = opti('obj',obj,'ndec',2)

x = solve(Opt,x0)

%TODO Commented as this problem takes a very long time or loops
% %% Example 5 - Nonlinear Constraints
% % Includes Nonlinear Constraints
% clc
% %Problem
% obj = @(x) (x(1) - x(2))^2 + (x(2) + x(3) - 2)^2 + (x(4) - 1)^2 + (x(5) - 1)^2;
% grad = @(x) 2*[ x(1) - x(2);
%                 x(2) + x(3) - 2 - x(1) + x(2);
%                 x(2) + x(3) - 2;
%                 x(4) - 1;
%                 x(5) - 1 ];
%
% % nonlinear constraints + jacobian + structure
% nlcon = @(x) [ x(1) + 3*x(2);
%                x(3) + x(4) - 2*x(5);
%                x(2) - x(5) ];
% nljac = @(x) sparse([ 1  3  0  0  0;
%                         0  0  1  1 -2;
%                         0  1  0  0 -1 ]);
% nljacstr = @() sparse([1 1 0 0 0;
%                        0 0 1 1 1;
%                        0 1 0 0 1]);
% nlrhs = [4 0 0]';   % nonlinear constraints RHS
% nle = [0 0 0]';     % nonlinear constraints type (-1 <=, 0 ==, 1 >=)
% x0 = [ 2.5 0.5 2 -1 0.5 ];
%
% % Nonlinear constraints are added using 'nlmix' or individual args
% % ('nlcon','nlrhs','nle'). To remain flexible the right hand side of the
% % equations is set via nlrhs (some solvers always default to 0) as well as
% % the constraint type (>=,<=,==) via nle.
% Opt = opti('obj',obj,'grad',grad,'nlmix',nlcon,nlrhs,nle,'nljac',nljac,'nljacstr',nljacstr)
%
% [x,fval,exitflag,info] = solve(Opt,x0)

%% Example 6 - User Supplied Hessian
% includes Hessian
clc
% problem
obj = @(x)100*(x(2)-x(1)^2)^2 + (1-x(1))^2 + 90*(x(4)-x(3)^2)^2 + (1-x(3))^2 + ...
      10.1*(x(2)-1)^2 + 10.1*(x(4)-1)^2 + 19.8*(x(2)-1)*(x(4)-1);
grad = @(x) [ -400*x(1)*(x(2)-x(1)^2) - 2*(1-x(1));
              200*(x(2)-x(1)^2) + 20.2*(x(2)-1) + 19.8*(x(4)-1);
              -360*x(3)*(x(4)-x(3)^2) - 2*(1-x(3));
              180*(x(4)-x(3)^2) + 20.2*(x(4)-1) + 19.8*(x(2)-1)];

% Hessian of the Lagrangian & structure
hess = @(x,sigma,lambda) sparse(sigma*[ 1200*x(1)^2-400*x(2)+2  0          0                          0
                                        -400*x(1)               220.2      0                          0
                                         0                      0          1080*x(3)^2-360*x(4)+2     0
                                         0                      19.8       -360*x(3)                  200.2 ]);
Hstr = @() sparse([ 1  0  0  0
                    1  1  0  0
                    0  0  1  0
                    0  1  1  1 ]);

% bounds
lb = [-10 -10 -10 -10]';
ub = [10 10 10 10]';
x0 = [-3  -1  -3  -1]';

% The Hessian + Structure are added just like other arguments. However note
% some solvers require a TRIL, TRIU or SYM Hessian. For instance IPOPT uses
% TRIL (assumes symmetric), while FMINCON requires the full Hessian. Make
% sure to configure your inputs as per the solver you're using.
Opt = opti('obj',obj,'grad',grad,'hess',hess,'Hstr',Hstr,'bounds',lb,ub)

[x,fval,exitflag,info] = solve(Opt,x0)

%% A Note on Hessians (2nd Derivatives)
% Be aware the Hessian required by OPTI is the Hessian of the Lagrangian.
% This means it contains the second derivatives of both the objective
% (scaled by sigma) and the nonlinear constraints (scaled by lambda).
%
% An example Hessian callback including nonlinear constraints (HS #71)
% is shown below:
%
%      function H = hessian (x, sigma, lambda)
%           H = sigma*[ 2*x(4)             0      0   0;
%                       x(4)               0      0   0;
%                       x(4)               0      0   0;
%                       2*x(1)+x(2)+x(3)  x(1)  x(1)  0 ];
%           H = H + lambda(1)*[    0          0         0         0;
%                               x(3)*x(4)     0         0         0;
%                               x(2)*x(4) x(1)*x(4)     0         0;
%                               x(2)*x(3) x(1)*x(3) x(1)*x(2)     0  ];
%           H = sparse(H + lambda(2)*diag([2 2 2 2]));
