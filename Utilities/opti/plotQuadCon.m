function plotQuadCon(Q,l,rl,ru,data)
%PLOTQUADCON Plot Quadratic Constraints on the current figure
%   plotQuadCon(Q,l,r)
%
%   Copyright (C) 2013 Jonathan Currie (IPL)

xl = data.xl; yl = data.yl;

hold on;

% colour
dkg = [0.2 0.2 0.2];

% determine number of quad constraints
if(iscell(Q))
    no = length(Q);
else
    no = 1;
end
%>= 2014b requires more points...
if(~verLessThan('matlab','8.4'))
    nmul = 2;
else
    nmul = 1;
end

% generate constraint surface points
[x1,x2] = meshgrid(linspace(xl(1),xl(2),data.npts*nmul),linspace(yl(1),yl(2),data.npts*nmul));

% for each quadratic constraint, plot:
for i = 1:no
    % get constraint variables
    if(iscell(Q))
        nQ = Q{i}; nl = l(:,i); nrl = rl(i); nru = ru(i);
    else
        nQ = Q; nl = l; nrl = rl; nru = ru;
    end  
    % form constraint function
    if(data.ndec==1)
        con = @(x) x(1)*nQ*x(1) + nl*x(1);
    else
        con = @(x) x.'*nQ*x + nl.'*x;
    end
    % plot each quad con as general nonlinear constraint
    plotNLCon(con,[],nrl,nru,x1,x2,dkg,data);    
end
hold off;
