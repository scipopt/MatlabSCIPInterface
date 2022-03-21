%% Mixed-Integer Semidefinite Programming Examples
%
% This file contains a number of MISDP problems and demonstrates how to
% solve them using the OcdPTI Toolbox.
%
% Copyright (C) 2022 Marc Pfetsch

no = 1;
misdp_tprob = cell(no,1);
misdp_sval = zeros(no,1);
i = 1;

% MISDP
f = [1 -2 -1]';
A = [1 1 1; -1 -1 -1];
lhs = [1 -8];
rhs = [];
C = [0 0 0 0; 0 0 0 0; 0 0 0 0; 0 0 0 -2.1];
A0 = [1 0 0 0; 0 0 0 0; 0 0 0 1; 0 0 1 0];
A1 = [0 1 0 0; 1 0 0 0; 0 0 0 0; 0 0 0 0];
A2 = [0 0 0 0; 0 1 0 0; 0 0 1 0; 0 0 0 0];
sdp = sparse([C(:) A0(:) A1(:) A2(:)]);
xtype = 'III';

% demonstrate how to write the constructed problem to a file
opts = optiset('probfile','testmisdp.cip','display','iter');

misdp = opti('f',f,'lin',A,lhs,rhs,'sdcone',sdp,'int',xtype,'options',opts);

% again solve the problem - this will only write the problem:
[x,fval,exitflag,info] = solve(misdp);
