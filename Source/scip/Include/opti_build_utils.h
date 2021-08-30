/* OPTI Build Utilities Header File
 * (C) Inverse Problem Limited 2017
 * J. Currie
 */

#ifndef OPTI_BUILD_UTILS
#define OPTI_BUILD_UTILS

/* Error Macros if Required Preprocessor Defines are not Defined */
#ifndef ML_VER
#error Please define the MATLAB/OCTAVE version when building the MEX file
#endif
#ifndef OPTI_VER
#error Please define the OPTI version when building the MEX file
#endif

#include "mex.h"
#include <string.h>

/** OPTI Version Checker Function */
inline void CheckOptiVersion(const mxArray* opts)
{
   static bool displayedWarning = false; // prevent multiple warnings
   double localVer = 0.0;

   if ( opts != NULL )
   {
      if ( mxIsStruct(opts) && mxGetField(opts, 0, "optiver") && ! mxIsEmpty(mxGetField(opts, 0, "optiver")) )
      {
         localVer = *mxGetPr(mxGetField(opts, 0, "optiver"));
         if ( OPTI_VER != localVer )
         {
            if ( displayedWarning == false )
            {
               char buf[1024];
               snprintf(buf, 1024, "The MEX File Version (%.2f) does not match OPTI's Version (%.2f), please run matlabSCIPInterface_install to update your MEX files.", OPTI_VER, localVer); 
               mexWarnMsgTxt(buf);
               displayedWarning = true; // don't spam
            }
         }
      }
   }
}

#endif // OPTI_BUILD_UTILS
