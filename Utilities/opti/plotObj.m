function [npts,xl,yl] = plotObj(prob,xb,data)
%PLOTOBJ Plot the objective function contour
%   plotObj(prob,xb,data)
%   Copyright (C) 2011 Jonathan Currie (IPL)

% contour colour
dkg = [0.4 0.4 0.4];
% gather commonly used inputs
npts = data.npts;
scale = data.scale;
idx = data.idx;
fval = [];
ss = warning('off','MATLAB:contour:ConstantData');
% Detail based on problem type, only if not specified by the user:
if(isempty(npts))
    switch(lower(prob.type))
        case {'lp','milp','bilp'}
            npts = 5;
        case {'qp','miqp','sdp','misdp','qcqp','miqcqp'}
            npts = 30;
        case {'snle','scnle','nls','uno','nlp','minlp'}
            npts = 50;
        otherwise
            npts = 50;
    end
end

% Determine if we have a 1D or 2D problem:
if((~isempty(prob.lb) && length(prob.lb) == 1) || (~isempty(xb) && length(xb) == 1))
    % generate vector based on mode
    switch(data.mode)
        case {'normal','usex0','bounded_scale'}
            if(length(scale) == 2)
                x = linspace(scale(1),scale(2),npts);
            elseif(length(scale) == 1)
                x = linspace(xb(1)-scale,xb(1)+scale,npts);
            else
                error('Input ''scale'' should be a vector with two inputs [xmin,xmax] for 1D problems');
            end
        case {'multi','bounded'}
            x = linspace(prob.lb(1),prob.ub(1),npts);
    end
    obj = zeros(npts,1);
    % get objective (created in buildConfig)
    fun = prob.objective;
    if(any(strcmpi(prob.type,{'snle','scnle'}))) % manually override
        fun = prob.fun;
    end
    % create objective line
    for i = 1:npts
        obj(i) = fun(x(i));
    end
    % do log plot if asked
    if(data.dolog)
        obj = log(obj);
    end
    % draw plot
    plot(x,obj.*prob.sense,'-','color',dkg);
    yl = ylim; axis([x(1) x(end) yl]);
    % get min
    if(~isempty(xb))
        fval = fun(xb).*prob.sense;
    end

else %2D or higher problem
    % generate grid based on mode
    switch(data.mode)
        case {'normal','usex0','bounded_scale'}
            if(length(scale) == 4)
                [x1,x2] = meshgrid(linspace(scale(1),scale(2),npts),linspace(scale(3),scale(4),npts));
            elseif(length(scale) == 1)
                [x1,x2] = meshgrid(linspace(xb(idx(1))-scale,xb(idx(1))+scale,npts),linspace(xb(idx(2))-scale,xb(idx(2))+scale,npts));
            else
                error('Input ''scale'' should be a vector with four inputs [x1min, x1max, x2min, x2max] for 2D problems.');
            end
        case {'multi','bounded'}
            [x1,x2] = meshgrid(linspace(prob.lb(idx(1)),prob.ub(idx(1)),npts),linspace(prob.lb(idx(2)),prob.ub(idx(2)),npts));
    end
    nox = size(x1);
    noy = size(x2);
    obj = zeros(nox(1),noy(2));
    % get objective (created in buildConfig)
    fun = prob.objective;
    x = data.fixval;
    % create objective surface
    for npts = 1:nox(1)
        for m = 1:noy(2)
            x(idx) = [x1(npts,m) x2(npts,m)]';
            obj(npts,m) = fun(x);
        end
    end
    % do log plot if asked
    if(data.dolog)
        obj = log(obj);
    end
    % draw contour plot
    [hc,hl] = contour(x1,x2,obj.*prob.sense,':','color',dkg);
    if(~isempty(hc))
        if(data.ndec > 2)
            clabel(hc,hl,'fontsize',8);
        else
            clabel(hc,hl);
        end
        hold on;
        % plot minimum contour
        if(~isempty(xb))
            fval = fun(xb).*prob.sense;
            contour(x1,x2,obj.*prob.sense,':','color',dkg,'levellist',fval);
        end 
    else
        optiwarn('OPTI:EmptyContour','Objective Contour Data is Empty - Cannot Plot Objective!');
        % manually set axis of interest
        try
            axis([min(min(x1)) max(max(x1)) min(min(x2)) max(max(x2))]);
        catch
        end
    end

    xlabel(sprintf('x_%d',idx(1))); ylabel(sprintf('x_%d',idx(2)));
end
warning(ss);

% 1D Root Solve plot horizontal 0 line
if(any(strcmpi(prob.type,{'snle','scnle'})) && data.ndec == 1)
    xl = xlim;
    hold on
    plot(xl,[0 0],'r:');
    hold off
end
    
% plot optimum + title
data.fval = fval;
title(plotTitle(prob,xb,data));
hold off;

% save limits of plot
xl = xlim;
yl = ylim;
end
