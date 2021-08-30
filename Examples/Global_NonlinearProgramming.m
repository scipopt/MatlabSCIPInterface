%% OPTI Toolbox Global Nonlinear Programming Examples
%
% This file contains a number of Global NLP problems and demonstrates how
% to solve them using the OPTI Toolbox.
%
%   Copyright (C) 2014 Jonathan Currie (IPL)


%% Determing which Solver to Use
% OPTI Toolbox comes with a number of NLP solvers, thus to determine which
% ones are available on your system you can type:
clc
optiSolver('NLP')

% Note the columns DR and GL. A cross in DR indicates the solver requires
% 1st (and perhaps 2nd) derivatives, while a cross in GL indicates the
% solver can solve Global Optimization problems. For noisy problems unless
% you have exact derivatives, avoid solvers with a cross in DR.

%% Typical Global Optimization Problems
% Global optimization problems result from any of the following circumstances:
%
%       - Objectives containing noise / a stochastic element
%       - Non-convex functions (functions which are not a bowl / hill in 2D)
%       - Objectives that include periodic functions (sin, cos)
%       - Parameter estimation of ODEs solved with adaptive step integrators
%       - Any problem that contains multiple local minima.
%
% An extreme example from Wolfram is shown below:
clc
% objective
fun = @(x) norm([x(1) - sin(2*x(1) + 3*x(2)) - cos(3*x(1) - 5*x(2));
          x(2) - sin(x(1) - 2*x(2)) + cos(x(1) + 3*x(2))]);
lb = [-4;-4]; ub = [4;4];
x0 = [-4;-4];

% plot
n = 1e2;
x = linspace(-4,4,n); y = linspace(-4,4,n); Z = zeros(n,n);
for i = 1:n
    for j = 1:n
        Z(j,i) = fun([x(i),y(j)]);
    end
end
surfc(x,y,Z)
colormap summer; shading interp; lighting phong; view(-38,58);
xlabel('x1'); ylabel('x2'); zlabel('obj'); title('Wolfram Global Optimization Problem');

%% Problem 2 - Quartic
% includes linear and nonlinear constraints
clc
% problem
fun = @(x) x(1)^4 - 14*x(1)^2 + 24*x(1) - x(2)^2;

% linear constraints
A = [-1 1]; b = 8;
% nonlinear constraints
nlcon = @(x) (-x(1)^2) - 2*x(1) + x(2);
nlrhs = -2;
nle = -1;
% bounds + starting guess
lb = [-8;0];
ub = [10;10];
x0 = [0;0];

% Solving a constrained global optimization problem. Note linear
% constraints will be converted to nonlinear ones for this solver.
opts = optiset('solver','scip');
Opt = opti('fun',fun,'ineq',A,b,'nlmix',nlcon,nlrhs,nle,'bounds',lb,ub,'options',opts)

% this may take a few seconds ...
[x,fval,exitflag,info] = solve(Opt,x0)

% TODO Commented out because it ran endlessly or looped
% %% Example 3 - Solving with PSwarm
% % Note x0 is on the local minima side of the saddle
% clc
% %Problem
% fun = @(x) -2*x(1)*x(2);
%
% %Constraints
% lb = [-0.5;-0.5];
% ub = [1;1];
% x0 = [-0.3;-0.3];
%
% %Plot
% n = 1e2;
% x = linspace(-0.5,1,n); y = linspace(-0.5,1,n); Z = zeros(n,n);
% for i = 1:n
%     for j = 1:n
%         Z(j,i) = fun([x(i),y(j)]);
%     end
% end
% surfc(x,y,Z); hold on; plot3(x0(1),x0(2),fun(x0),'r.','markersize',20); hold off;
% colormap winter; shading flat; lighting gouraud; view(18,28);
% xlabel('x1'); ylabel('x2'); zlabel('obj'); title('Saddle Point Optimization Problem');
%
% % PSwarm solves bounded and linearly constrained global problems
% Opt = opti('fun',fun,'bounds',lb,ub,'options',optiset('solver','scip'))
%
%
% [x,fval,exitflag,info] = solve(Opt,x0)
% plot(Opt,3)

%% Example 4 - White Box Quartic
% The following problem is the same as problem 2, however this time we are
% going to solve it using a white box solver (SCIP). The SCIP interface
% will parse the following functions into an algebraic description,
% allowing SCIP to find a global solution to this problem.
clc
% problem
fun = @(x) x(1)^4 - 14*x(1)^2 + 24*x(1) - x(2)^2;

% linear constraints
A = [-1 1]; b = 8;
% nonlinear constraints
nlcon = @(x) (-x(1)^2) - 2*x(1) + x(2);
nlrhs = -2;
nle = -1;
% bounds + starting guess
lb = [-8;0];
ub = [10;10];
x0 = [0;0];

% SCIP solves a subset of nonlinear and mixed integer problems, provided
% the problem is deterministics and constains a subset of allowable functions.
Opt = opti('fun',fun,'ineq',A,b,'nlmix',nlcon,nlrhs,nle,'bounds',lb,ub,...
           'options',optiset('solver','scip'))

[x,fval,exitflag,info] = solve(Opt,x0)
plot(Opt)

%% Summary
% While Global Optimization solvers may take longer, many real
% engineering problems result in noisy or non-convex objectives and
% local solvers will often struggle to return a result, or fall into
% the closest local optima. Global optimization solvers can return
% much better results.
