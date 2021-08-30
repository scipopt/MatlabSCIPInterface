function info = optiSolverInfo(solver,probtype,nlprob,opts)
%OPTISOLVERINFO  Return Configuration Information about a particular solver
%
%   info = optiSolverInfo(solver,probtype)

% default input args and checks
if(nargin < 4), opts = []; end
if(nargin < 3), nlprob = []; end
if(nargin < 2 || isempty(probtype)), probtype = ''; end
if(nargin < 1 || ~ischar(solver)), error('You must supply the name of the solver to this function.'); end

% create default information structure
info.der1 = 0;
info.der2 = 0;
info.global = 0;
info.sparse = 0;
info.multialg = 0;
info.parallel = 0;
info.whitebox = 0;
info.con = struct('con',0,'bnd',0,'lineq',0,'leq',0,'qineq',0,'qeq',0,'sdp',0,'nlineq',0,...
                  'nleq',0,'int',0,'sos',0);
info.opt = struct('miter',0,'meval',0,'mnode',0,'mtime',0,'tolr',0,...
                  'tola',0,'tolint',0,'disp',0,'opts',0,'ctrlc',0,...
                  'iterf',0);
info.unq = struct('x0',0,'qpform',0,'hform',0,'jstr',0,'hstr',0);

% constants
YES = 1;
SOME = -1;

% fill in information based on selected solver
switch(lower(solver))
    case 'matlab'
        info.der1 = SOME;
        info.der2 = SOME;
        info.sparse = YES;
        info.multialg = YES;
        info.con.con = YES;
        info.con.bnd = YES;
        switch(lower(probtype))
            case {'lp','qp','bilp','milp'}
                %[bnd,lineq,leq,qineq,sdp,nlineq,nleq,sp] = fillFields(yes,yes,yes,no,no,no,no,yes);
                info.con.lineq = YES;
                info.con.leq = YES;
                %unique
                info.unq.x0 = SOME;
                if(~isempty(which('intlinprog.m')))
                    info.con.int = YES;
                end
            case {'nlp',''}
                info.der1 = YES;
                info.der2 = SOME;
                %[bnd,lineq,leq,qineq,sdp,nlineq,nleq,sp] = fillFields(yes,yes,yes,no,no,yes,yes,yes);
                info.con.lineq = YES;
                info.con.leq = YES;
                info.con.nlineq = YES;
                info.con.nleq = YES;
                if(~isempty(which('intlinprog.m')))
                    info.con.int = YES;
                end
                %unique
                info.unq.x0 = YES;
                info.unq.hform = 'sym';
                info.unq.jstr = YES;
                info.unq.hstr = YES;
        end
        %[miter,meval,mnode,mtime,tolr,tola,tolint,disp,opts,ctrlc,iterf] = fillCFields(yes,yes,some,yes,yes,no,some,yes,yes,yes,yes);
        info.opt.miter = YES;
        info.opt.meval = YES;
        info.opt.mtime = YES;
        info.opt.tolr = YES;
        info.opt.disp = YES;
        info.opt.opts = YES;
        info.opt.ctrlc = YES;
        info.opt.iterf = YES;

    case 'scip'
        info.global = YES;
        info.sparse = YES;
        info.whitebox = YES;
        %[bnd,lineq,leq,qineq,sdp,nlineq,nleq,sp] = fillFields(yes,yes,yes,yes,no,yes,yes,yes);
        info.con.con = YES;
        info.con.bnd = YES;
        info.con.lineq = YES;
        info.con.leq = YES;
        info.con.qineq = YES;
        info.con.qeq = YES;
        info.con.nlineq = YES;
        info.con.nleq = YES;
        info.con.int = YES;
        info.con.sos = YES;
        %[miter,meval,mnode,mtime,tolr,tola,tolint,disp,opts,ctrlc,iterf] = fillCFields(some,no,yes,yes,yes,no,no,yes,no,yes,no);
        info.opt.miter = SOME;
        info.opt.mnode = YES;
        info.opt.mtime = YES;
        info.opt.tolr = YES;
        info.opt.opts = YES;
        info.opt.disp = YES;
        info.opt.ctrlc = YES;

  case 'scipsdp'
        info.global = YES;
        info.sparse = YES;
        info.whitebox = YES;
        info.con.con = YES;
        info.con.bnd = YES;
        info.con.lineq = YES;
        info.con.leq = YES;
        info.con.qineq = SOME;
        info.con.qeq = SOME;
        info.con.nlineq = SOME;
        info.con.nleq = SOME;
        info.con.int = YES;
        info.con.sos = SOME;
        info.opt.miter = SOME;
        info.opt.mnode = YES;
        info.opt.mtime = YES;
        info.opt.tolr = YES;
        info.opt.opts = YES;
        info.opt.disp = YES;
        info.opt.ctrlc = YES;

    case 'auto'
        % assume we require derivatives
        info.der1 = YES;

    otherwise
        error('Unknown solver: %s',solver);
end
