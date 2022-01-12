function ok = opti_Install_Test(verb)
% Installation Test File
%
% A collection of tests to verify the installation of the toolbox has been
% successful.
%
%   Copyright (C) 2012 Jonathan Currie (IPL)
%
% 14/3/12 Changed to Rel Error Check
% 8/2/14 Changed tol to 1e-3

% set verbosity
if(nargin < 1)
    verb = 1; % default show all messages
end

% set to 1 for output of solvers
moreverb = 0;

% set default ok
ok = 1;

fprintf('\nChecking OPTI Toolbox Installation:\n');


%% Setup tests to run
tests = {'lp','milp','qp','miqp','sdp','misdp','nls','nlp','minlp'};
tres = ones(size(tests));

% loop for each test set
for t = 1:length(tests)
    load(['Utilities/Install/' tests{t} '_test_results.mat']); % load presolved results
    probs = eval([tests{t} '_tprob']);
    sols = eval([tests{t} '_sval']);
    msolvers = optiSolver(tests{t});
    ind = strcmpi(msolvers,'MATLAB'); msolvers(ind) = [];
    ind = strcmpi(msolvers,'GMATLAB'); msolvers(ind) = [];
    noS = length(msolvers);
    if(noS == 0)
        continue;
    end
    noP = length(probs);
    if(verb); fprintf('Checking %5s Solver Results... ',upper(tests{t})); end
    for i = 1:noS
        if(verb); fprintf(' %s ', msolvers{i}); end
        for j = 1:noP
            if(verb); fprintf(' %d ', j); end
            if(moreverb)
                Opt = opti(probs{j},optiset('solver',msolvers{i},'warnings','off','display','iter'));
            else
                Opt = opti(probs{j},optiset('solver',msolvers{i},'warnings','off','display','off'));
            end
            [~,fval] = solve(Opt);
            if(sols(j) == 0)
                if(abs(fval-sols(j)) > 1e-3)
                    fprintf('\nFailed %s Result Check on Test %d, Solver: %s.',tests{t},j,msolvers{i});
                    tres(t) = 0;
                end
            else
                if(abs((fval-sols(j))/fval) > 1e-3)
                    fprintf('\nFailed %s Result Check on Test %d, Solver: %s.',tests{t},j,msolvers{i});
                    tres(t) = 0;
                end
            end
        end
    end
    if(verb && tres(t)), fprintf('Ok\n'); else, fprintf('\n\n'); end
end

%% Final
if(ok && all(tres))
    ok = 1;
    fprintf('\nToolbox checked out ok! - Enjoy.\n');
else
    ok = 0;
    fprintf(2,'\nThere was an error during post installation checks, please ensure your download was not corrupted!\n');
end

end
