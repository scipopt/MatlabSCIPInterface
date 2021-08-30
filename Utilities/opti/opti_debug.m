function opti_debug()
% Collects information about your OPTI install, MATLAB and OS and writes to
% a txt file for use in debugging interface problems.

fid = fopen('opti_debug.txt','w');
if(fid < 0)
    error('Cannot create a debug file in the current directory. Please re run in a directory you have write permissions');
end

try
    % header
    fprintf(fid,'OPTI Debug File generated %s\n\n',datestr(now));

    % environment variable information
    fprintf(fid,'OS: %s\n',getenv('OS'));
    fprintf(fid,'ARCH: %s\n',getenv('PROCESSOR_ARCHITECTURE'));
    fprintf(fid,'IDENT: %s\n',getenv('PROCESSOR_IDENTIFIER'));
    fprintf(fid,'LEVEL: %s\n',getenv('PROCESSOR_LEVEL'));
    fprintf(fid,'REV: %s\n',getenv('PROCESSOR_REVISION'));
    fprintf(fid,'#PROC: %s\n',getenv('NUMBER_OF_PROCESSORS'));

    % MATLAB information
    mver = ver('MATLAB');
    fprintf(fid,'\nMATLAB: %s, v%s, %s\n',mver.Release,mver.Version,mver.Date);
    fprintf(fid,'MATLAB OS: %s\n',computer);

    % memory information (only on windows)
    if (strcmp(computer, "PCWIN64"))
        [UV,SV] = memory();
        fprintf(fid,'\nTotal RAM: %d MB\n',int32(SV.PhysicalMemory.Total/1e6));
        fprintf(fid,'Available RAM: %d MB\n',int32(SV.PhysicalMemory.Available/1e6));
        fprintf(fid,'MATLAB Memory: %d MB\n',int32(UV.MemUsedMATLAB/1e6));
    end

    % MEX Information
    cc = mex.getCompilerConfigurations();
    try
        fprintf(fid,'MEX: %s, ver %s\n',cc.Name,cc.Version);
    catch
        fprintf(fid,'MEX: Not Setup\n');
    end

    % get OPTI information
    p = which('optiver.m');
    if(~isempty(p))
        % print version
        if (optiSolver('scip',0))
            str = 'Academic';
        else
            str = 'Open Source';
        end
        fprintf(fid,'\nOPTI: v%4.2f (%s)\n', optiver, str);
        % print install location
        fprintf(fid,'Install Location: %s\n', which('matlabSCIPInterface_install.m'));

        % print solver availability
        fprintf(fid,'\nSolver Availability:\n');
        s = optiSolver('ver');
        for i = 1:length(s)
            fprintf(fid,'%s\n', s{i});
        end
    else
       fprintf('OPTI: Cannot find optiver, OPTI not installed?\n\n');
    end

    % tell user all done
    fprintf('Debug information written to ''opti_debug.txt'' in %s.\n',cd);
catch ME
    fclose(fid);
    rethrow(ME);
end

fclose(fid);
