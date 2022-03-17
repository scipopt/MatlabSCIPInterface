%% OPTI Test Problems
% A collection of test problems I've used for building the toolbox.
clc
%Run the following line to see what solvers are available to OPTI:
optiSolver();

%Alternatively you can find the best available solver for a given problem 
%type:
lp = optiSolver('best_lp')

%Or see all available solvers for a given problem type
optiSolver('lp')

%#ok<*ASGLU,*NASGU,*NOPTS>

%% LP1
clc
%Objective & Constraints
f = -[6 5]';
A = ([1,4; 6,4; 2, -5]); 
b = [16;28;6];    
%Build Object
Opt = opti('grad',f,'ineq',A,b,'bounds',[0;0],[10;10])
%Build & Solve
[x,fval,exitflag,info] = solve(Opt)  
%Plot
plot(Opt)
%Check Solution
[ok,msg] = checkSol(Opt)

%% LP2
clc
%Objective & Constraints
f = -[1 2 3]';
A = [-1,1,1; 1,-3,1];
b = [20,30]';
%Setup Options
opts = optiset('solver','scip');
%Build & Solve
Opt = opti('grad',f,'ineq',A,b,'eq',[1 1 1],40,'bounds',[0 0 0]',[40 inf inf]','options',opts)
[x,fval,exitflag,info] = solve(Opt)

%% LP3
clc
%Objective & Constraints
f = [8,1]';
A = [-1,-2;1,-4;3,-1;1,5;-1,1;-1,0;0,-1]; 
b = [-4,2,21,39,3,0,0]';
%Setup Options
opts = optiset('solver','scip');
%Build & Solve
Opt = opti('grad',f,'ineq',A,b,'options',opts)
x0 = [9,6]';
[x,fval,exitflag,info] = solve(Opt,x0)
%Plot
plot(Opt,10)

%% MILP1
clc
%Objective & Constraints
f = -[6 5]';
A = [1,4; 6,4; 2, -5]; 
b = [16;28;6];  
%Setup Options
opts = optiset('solver','scip');
%Build & Solve
Opt = opti('grad',f,'ineq',A,b,'bounds',[0;0],[10;10],'int','II','options',opts)
[x,fval,exitflag,info] = solve(Opt)
plot(Opt)

%% MILP2
clc
%Objective & Constraints
f = -[1 2 3 1]'; 
A = [-1 1 1 10; 1 -3 1 0]; 
b = [20;30];  
Aeq = [0 1 0 -3.5];
beq = 0;
%Setup Options
opts = optiset('solver','scip');
%Build & Solve
Opt = opti('grad',f,'ineq',A,b,'eq',Aeq,beq,'bounds',[0 0 0 2]',[40 inf inf 3]','int','CCCI','options',opts)
[x,fval,exitflag,info] = solve(Opt)
plot(Opt)

%% MILP3 Infeasible
clc
%Objective & Constraints
f = -[6 5]';
A = [4,1; 1.5,1; -2, 1; -0.2, 1]; 
b = [5.3;4.5;-2.5; 1.5];  
%Setup Options
opts = optiset('solver','scip');
%Build & Solve
Opt = opti('grad',f,'ineq',A,b,'bounds',[0;0],[10;10],'int','II','options',opts)
[x,fval,exitflag,info] = solve(Opt)
%Plot
plot(Opt)

%% MILP4
clc
%Objective & Constraints
f = -[-1, 2]';
A = [2, 1;-4, 4];
b = [5, 5]';
e = -[1, 1];
%Setup Options
opts = optiset('solver','scip');
%Build & Solve
Opt = opti('grad',f,'mix',A,b,e,'int','II','options',opts)
[x,fval,exitflag,info] = solve(Opt)
%Plot
plot(Opt)

%% MILP5
clc
%Objective & Constraints
f = [3, -7, -12]';
A = [-3, 6, 8;6, -3, 7;-6, 3, 3];
b = [12, 8, 5]';
e = [-1, -1, -1];
%Setup Options
opts = optiset('solver','scip');
%Build & Solve
Opt = opti('grad',f,'mix',A,b,e,'int','III','options',opts)
[x,fval,exitflag,info] = solve(Opt)

%% MILP6
clc
%Objective + Constraints
f = [-1 -1 -3 -2 -2]';
A = [-1 -1 1 1 0;
     1 0 1 -3 0];
b = [30;30];
ub = [40;1;inf;inf;1];
%Special Ordered Sets
sos = '12';
sosind = {(1:2) (3:5)};
soswt = {(1:2) (3:5)};
%Setup Options
opts = optiset('solver','scip');
%Build & Solve
Opt = opti('f',f,'ineq',A,b,'ub',ub,'sos',sos,sosind,soswt,'options',opts)
[x,fval,exitflag,info] = solve(Opt)

%% BILP1
clc
%Objective & Constraints
f = -[6 5]';
A = [-3,5; 6,4; 3, -5; -6, -4]; 
b = [6;9;1;3];  
%Setup Options
opts = optiset('solver','scip');
%Build & Solve
Opt = opti('grad',f,'ineq',A,b,'int','BB','options',opts)
[x,fval,exitflag,info] = solve(Opt)
%Plot
plot(Opt,3)

%% BILP2
clc
%Objective & Constraints
f = -[9 5 6 4]';
A = [6 3 5 2; 0 0 1 1; -1 0 1 0; 0 -1 0 1];
b = [9; 1; 0; 0];
%Setup Options
opts = optiset('solver','scip');
%Build & Solve
Opt = opti('grad',f,'ineq',A,b,'int','BBBB','options',opts)
[x,fval,exitflag,info] = solve(Opt)

%% QP1
clc
%Objective & Constraints
H = eye(3);
f = -[2 3 1]';
A = [1 1 1;3 -2 -3; 1 -3 2]; 
b = [1;1;1];
%Setup Options
opts = optiset('solver','scip');
%Build & Solve
Opt = opti('hess',H,'grad',f,'ineq',A,b,'options',opts)
[x,fval,exitflag,info] = solve(Opt)

%% QP2
clc
%Objective & Constraints
H = [1 -1; -1 2];
f = -[2 6]';
A = [1 1; -1 2; 2 1];
b = [2; 2; 3];
%Setup Options
opts = optiset('display','iter','solver','scip');
%Build & Solve
Opt = opti('qp',H,f,'ineq',A,b,'lb',[0;0],'options',opts)
[x,fval,exitflag,info] = solve(Opt)
%Plot
plot(Opt)

%% QP3
clc
%Objective & Constraints
H = [1 -1; -1 2];
f = -[2 6]';
A = [1 1; -1 2; 2 1];
b = [2; 2; 3];
Aeq = [1 1.5];
beq = 2;
%Setup Options
opts = optiset('solver','scip');
%Build & Solve
Opt = opti('qp',H,f,'eq',Aeq,beq,'ineq',A,b,'bounds',[0;0],[10;10],'options',opts)
[x,fval,exitflag,info] = solve(Opt)
%Plot
plot(Opt)

%% Nonconvex QP
clc
%Objective & Constraints
H = sparse([0 -1; -1 0]);
f = [0;0];
%Constraints
lb = [-0.5;-0.5];
ub = [1;1];
%Build & Solve
Opt = opti('qp',H,f,'bounds',lb,ub,'options',optiset('solver','scip','display','iter'))
[x,fval,exitflag,info] = solve(Opt)
%Plot
plot(Opt)

%% QCQP1 [-0.4004]
clc
%Objective & Constraints
H = [33 6    0;
     6  22   11.5;
     0  11.5 11];
f = [-1;-2;-3];
A = [-1 1 1; 1 -3 1];
b = [20;30];
Q = eye(3);
l = [2;2;2];
r = 0.5;
lb = [0;0;0];
ub = [40;inf;inf];
%Build & Solve
Opt = opti('H',H,'f',f,'ineq',A,b,'qc',Q,l,r,'bounds',lb,ub,'options',optiset('display','iter','solver','scip'))
[x,fval,exitflag,info] = solve(Opt)

%% QCQP2 [-3.5]
clc
%Objective & Constraints
H = eye(2);
f = [-2;-2];
A = [-1 1; 1 3];
b = [2;5];
Q = [1 0;0 1];
l = [0;-2];
r = 1;
lb = [0;0];
ub = [40;inf];
%Build & Solve
Opt = opti('H',H,'f',f,'ineq',A,b,'qc',Q,l,r,'bounds',lb,ub)
[x,fval,exitflag,info] = solve(Opt)
% Plot
plot(Opt)

%% QCQ(L)P2 [-5.2]
clc
%Objective & Constraints
f = [-2;-2];
A = [-1 1; 1 3];
b = [2;5];
Q = [1 0;0 1];
l = [0;-2];
r = 1;
lb = [0;0];
ub = [40;inf];
%Build & Solve
Opt = opti('f',f,'ineq',A,b,'qc',Q,l,r,'bounds',lb,ub)
[x,fval,exitflag,info] = solve(Opt)
% Plot
plot(Opt)

%% QCQP3 [-1.7394]
clc
H = zeros(2);
f = [-2;-2];
A = [-1 1; 1 3];
b = [2;5];
Q = {[1 0;0 1];[1 0;0 1]};
l = [0 2;2 -2];
r = [1 1];
lb = [0;0];
ub = [40;inf];
%Build & Solve
Opt = opti('H',H,'f',f,'ineq',A,b,'qc',Q,l,r,'bounds',lb,ub)
[x,fval,exitflag,info] = solve(Opt)
% Plot
plot(Opt)

%% QCQP3a [-1.7394]
clc
H = zeros(2);
f = [-2;-2];
A = [-1 1; 1 3];
b = [2;5];
%Alternate QC form
Q = {[1 0;0 1];[1 0;0 1]};
l = {[0;2];[2 -2]};
r = [1 1];
lb = [0;0];
ub = [40;inf];
%Build & Solve
Opt = opti('H',H,'f',f,'ineq',A,b,'qc',Q,l,r,'bounds',lb,ub)
[x,fval,exitflag,info] = solve(Opt)
% Plot
plot(Opt)

%% QCQP4 [-1.7394]
clc
H = zeros(2);
f = [-2;-2];
A = [-1 1; 1 3];
b = [2;5];
Q = {[1 0;0 1];[1 0;0 1];[1 0;0 1]};
l = [0 2 -2;2 -2 -1];
r = [1 1 1];
lb = [0;0];
ub = [40;inf];
%Build & Solve
Opt = opti('H',H,'f',f,'ineq',A,b,'qc',Q,l,r,'bounds',lb,ub)
[x,fval,exitflag,info] = solve(Opt)
% Plot
plot(Opt)

%% Positive Semidefinite QCQP [-3.0834]
clc
%Objective & Constraints
H = eye(2);
f = [-2;-2];
A = [-1 1; 1 3];
b = [2;3.8];
Q = [0 0;0 10];
l = [0;-2];
r = 3;
lb = [0;0];
ub = [40;inf];
%Build & Solve
Opt = opti('H',H,'f',f,'ineq',A,b,'qc',Q,l,r,'bounds',lb,ub,'options',optiset('solver','scip'))
[x,fval,exitflag,info] = solve(Opt)
% Plot
plot(Opt,3)

%% Indefinite QCQP [-3.55]
clc
%Objective & Constraints
H = eye(2);
f = [-2;-2];
A = [-1 1; 1 3];
b = [2;5];
Q = [-1 0;0 1];
l = [0;-2];
r = -0.5;
lb = [0;0];
ub = [40;inf];
%Build & Solve
Opt = opti('H',H,'f',f,'ineq',A,b,'qc',Q,l,r,'bounds',lb,ub,'options',optiset('solver','scip'))
[x,fval,exitflag,info] = solve(Opt)
% Plot
plot(Opt)

%% MIQP1 [-11.5]
clc
%Objective & Constraints
H = [1 -1; -1 2];
f = -[2 6]';
A = [1 1; -1 2];
b = [3; 3.5];
ivars = 1;
%Setup Options
opts = optiset('display','iter','solver','scip');
%Build & Solve
Opt = opti('hess',H,'grad',f,'ineq',A,b,'bounds',[0;0],[10;10],'int',ivars,'options',opts)
[x,fval,exitflag,info] = solve(Opt)
plot(Opt,4)

%% MIQP2 [-2.75]
clc
%Objective & Constraints
H = eye(3);
f = -[2 3 1]';
A = [1 1 1;3 -2 -3; 1 -3 2]; 
b = [1;1;1];
%Setup Options
opts = optiset('solver','scip');
%Build & Solve
Opt = opti('hess',H,'grad',f,'ineq',A,b,'int','CIC','options',opts)
try
[x,fval,exitflag,info] = solve(Opt)
catch
end

%% MIQCQP1 [-2.5429] [non-convex]
clc
%Objective & Constraints
H = eye(2);
f = [-2;-2];
A = [-1 1; 1 3];
b = [2;5];
Q = [1 0;0 1];
l = [0;-2];
qrl = 3.5;
qru = 5;
lb = [0;0];
ub = [40;inf];
xtype = 'IC';
%Build & Solve
Opt = opti('H',H,'f',f,'ineq',A,b,'qcrow',Q,l,qrl,qru,'bounds',lb,ub,'xtype',xtype)
[x,fval,exitflag,info] = solve(Opt)
% Plot
plot(Opt)

%% MIQCQP2 [-0.0942]
clc
%Objective & Constraints
H = [33 6    0;
     6  22   11.5;
     0  11.5 11];
f = [-1;-2;-3];
A = [-1 1 1; 1 -3 1];
b = [20;30];
Q = eye(3);
l = [0;0;0];
r = 1;
lb = [0;0;0];
ub = [40;inf;inf];
int = 'CCI';
%Setup Options
opts = optiset('solver','scip','display','iter');
% Solve
Opt = opti('H',H,'f',f,'ineq',A,b,'qc',Q,l,r,'bounds',lb,ub,'int',int,'options',opts)
[x,fval,exitflag,info] = solve(Opt)

[ok,msg] = checkSol(Opt)

% TODO Include those to work with SCIPSDP?
%%% SDP1 [1.4142]
%%Note OPTI accepts Standard Primal Form [sum(xi*Ai) - C >= 0]
%clc
%%Objective
%f = 1;
%%SDP Constraints [x sqrt(2); sqrt(2) x] >= 0
%C = -[0 sqrt(2); sqrt(2) 0];
%A = eye(2);
%sdcone = [C A];
%%Setup Options
%opts = optiset('solver','scip','display','iter');
%% Solve
%Opt = opti('f',f,'sdcone',sdcone,'options',opts)
%[x,fval,exitflag,info] = solve(Opt)
%
%[ok,msg] = checkSol(Opt)
%plot(Opt)
%
%%% SDP2 [4]
%clc
%%Objective
%f = [1;1];
%%Linear Constraints
%lb = [0;0];
%ub = [10;10];
%%SDP Constraints [x1 2; 2 x2] >= 0
%C = -[0 2; 2 0];
%A0 = [1 0; 0 0];
%A1 = [0 0; 0 1];
%sdcone = {[C A0 A1]};
%%Setup Options
%opts = optiset('solver','scip','display','iter');
%%Solve
%Opt = opti('f',f,'bounds',lb,ub,'sdcone',sdcone,'options',opts)
%[x,fval,exitflag,info] = solve(Opt)
%%Plot Solution
%plot(Opt)
%
%%% SDP3 [1.2] (Note assume matrices are symmetric triu to avoid x5)
%clc
%clear
%%Objective
%f = [1 0 0 0]';
%%SDP Constraint1 [x2 x3; x3 x4] <= x1*eye(2)
%C = zeros(2);
%A0 = eye(2);
%A1 = -[1 0; 0 0];
%A2 = -[0 1; 1 0];
%A3 = -[0 0; 0 1];
%sdcone{1} = [C A0 A1 A2 A3];
%%SDP Constraint2 [x2 x3; x3 x4] >= [1 0.2; 0.2 1]
%C  = [1 0.2; 0.2 1];
%A0 = zeros(2);
%A1 = [1 0; 0 0];
%A2 = [0 1; 1 0];
%A3 = [0 0; 0 1];
%sdcone{2} = [C A0 A1 A2 A3];
%%Setup Options
%opts = optiset('solver','scip','display','iter');
%% Solve
%Opt = opti('f',f,'sdcone',sdcone,'options',opts)
%[x,fval,exitflag,info] = solve(Opt)
%[ok,msg] = checkSol(Opt)
%plot(Opt,1)
%
%%% SDP4 [10.1787]
%clc
%clear
%%Objective
%f = [1 1 1];
%%Linear Constraints
%lb = [10;0;0];
%ub = [1000;1000;1000];
%%SDP Constraints [x1 1 2; 1 x2 3; 2 3 100] >= 0
%C = -[0 1 2; 1 0 3; 2 3 100];
%A0 = [1 0 0; 0 0 0; 0 0 0];
%A1 = [0 0 0; 0 1 0; 0 0 0];
%A2 = zeros(3);
%sdcone = [C A0 A1 A2];
%%Setup Options
%dopts = dsdpset('ptol',1e-8,'rho',3,'zbar',0);
%opts = optiset('solver','scip','display','iter','solverOpts',dopts);
%% Solve
%Opt = opti('f',f,'bounds',lb,ub,'sdcone',sdcone,'options',opts)
%[x,fval,exitflag,info] = solve(Opt)
%plot(Opt)
%
%%% SDP5 [Sparse Column Format] [4]
%clc
%%Objective
%f = [1;1];
%%Linear Constraints
%lb = [0;0];
%ub = [10;10];
%%SDP Constraints [x1 2; 2 x2] >= 0
%C = -[0 2; 2 0];
%A0 = [1 0; 0 0];
%A1 = [0 0; 0 1];
%% sdcone = [C A0 A1];
%sdcone = sparse([C(:) A0(:) A1(:)]);
%%Setup Options
%opts = optiset('solver','scip','display','iter');
%% Solve
%Opt = opti('f',f,'bounds',lb,ub,'sdcone',sdcone,'options',opts)
%[x,fval,exitflag,info] = solve(Opt)
%
%%% SDP6 [SeDuMi Structure Format] [sqrt(2)]
%clc
%%Objective
%b = -1;
%%SDP Constraints [x sqrt(2); sqrt(2) x] >= 0
%C = -[0 sqrt(2); sqrt(2) 0];
%A = -eye(2);
%K = struct('s',2);
%%Setup Options
%opts = optiset('solver','scip','display','iter');
%% Solve
%Opt = opti('sedumi',A(:),b,C(:),K,'options',opts)
%[x,fval,exitflag,info] = solve(Opt)
%plot(Opt)
%
%%% SDP7 [SeDuMi Structure with Linear Con] [4.1667]
%clc
%%Objective
%b = -[1;1];
%%Linear Constraints ([0;0] <= x <= [10;1.5])
%c = [0;0;10;1.5];
%A = [-1 0; 0 -1; 1 0; 0 1];
%%SDP Constraints [x1 2; 2 x2] >= 0
%C = -[0 2; 2 0];
%A0 = [1 0; 0 0];
%A1 = [0 0; 0 1];
%%Concantenate
%A = [A; -A0(:) -A1(:)];
%c = [c; -C(:)];
%K = struct('l',4,'s',2);
%sdcone = [];
%sdcone.At = A;
%sdcone.b = b;
%sdcone.c = c;
%sdcone.K = K;
%%Setup Options
%opts = optiset('solver','scip','display','iter');
%% Solve
%Opt = opti('sedumi',sdcone,'options',opts)
%[x,fval,exitflag,info] = solve(Opt)
%plot(Opt)

%% MINLP1
clc
%Objective
obj = @(x) (1-x(1))^2 + 100 *(x(2)-x(1)^2)^2;
% Constraints
A = [-1 1]; 
b = -1;
Aeq = [1 1]; 
beq = 5; 
lb = [0;0]; ub = [4;4];
% Solve
x0 = [2;2];
Opt = opti('obj',obj,'ndec',2,'bounds',lb,ub,'ivars',1,'ineq',A,b,'eq',Aeq,beq)
[x,fval,ef,info] = solve(Opt,x0)
%Plot
plot(Opt,[],1)


%% MINLP3
clc
%Objective
obj = @(x) (x(1) - 5)^2 + x(2)^2 - 25;
% Constraints
nlcon = @(x) -x(1)^2 + x(2)-0.5;
nlrhs = 0;
nle = 1;      
% Solve
Opt = opti('obj',obj,'ndec',2,'nlmix',nlcon,nlrhs,nle,'int',[1 2],'options',optiset('solver','scip','display','iter'))
x0 = [4.9;0.1];
[x,fval,ef,info] = solve(Opt,x0,xval)
%Plot
plot(Opt,[])