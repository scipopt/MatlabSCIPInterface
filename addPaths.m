% This script can be used to quickly add all files needed for the MatlabSCIP interface.
% Please ensure that this file remains in the root of the interface
addpath([fileparts(which(mfilename)) '/Utilities/opti']);
addpath([fileparts(which(mfilename)) '/Utilities/Install']);
addpath([fileparts(which(mfilename)) '/Source']);
addpath(fileparts(which(mfilename)));


% If you want to test the examples also uncomment the following
% addpath([fileparts(which(mfilename)) '/Examples'])