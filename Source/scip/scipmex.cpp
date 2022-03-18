/* SCIPMEX - A MATLAB MEX Interface to SCIP
 * Released Under the BSD 3-Clause License.
 *
 * Based on code by Jonathan Currie, which was based in parts on matscip.c supplied with SCIP.
 *
 * Authors:
 * - Jonathan Currie
 * - Marc Pfetsch
 */

#include "mex.h"
#include <ctype.h>
#include <stdio.h>
#include <cmath>
#include <limits>

#include <scip/scip.h>
#include <scip/scipdefplugins.h>
#include <scip/pub_paramset.h>
#include "scipeventmex.h"
#include "opti_build_utils.h"
#include "scipnlmex.h"

using namespace std;

/** argument enumeration (in expected order of arguments) */
enum
{
   eH     = 0,           /**< H: quadratric objective matrix */
   eF     = 1,           /**< f: linear objective function */
   eA     = 2,           /**< A: linear constraint matrix */
   eLHS   = 3,           /**< lhs: lhs of linear constraints */
   eRHS   = 4,           /**< rhs: rhs of linear constraints */
   eLB    = 5,           /**< lb: lower variable bounds */
   eUB    = 6,           /**< ub: upper variable bounds */
   eXTYPE = 7,           /**< xtype: variable types ('C', 'I', 'B') */
   eSOS   = 8,           /**< SOS: SOS constraints */
   eQC    = 9,           /**< Q: quadratic constraint matrix */
   eNLCON = 10,          /**< nlcon: nonlinear constraints */
   eX0    = 11,          /**< x0: primal solution */
   eOPTS  = 12           /**< opts: SCIP options */
};

/* message buffer size */
#define BUFSIZE 2048

/* global message buffer */
static char msgbuf[BUFSIZE];

/* error catching macro */
#define SCIP_ERR(rc,msg) if ( rc != SCIP_OKAY ) { snprintf(msgbuf, BUFSIZE, "%s, Error Code: %d", msg, rc); mexErrMsgTxt(msgbuf);}


/* supporting functions */

/** converts error code to string */
static
const char* scipErrCode(
   int                   x                   /**< error code */
   )
{
   switch ( x )
   {
   case SCIP_OKAY: return "Normal Termination";
   case SCIP_ERROR: return "Unspecified Error";
   case SCIP_NOMEMORY: return "Insufficient Memory Error";
   case SCIP_READERROR: return "Read Error";
   case SCIP_WRITEERROR: return "Write Error";
   case SCIP_NOFILE: return "File Not Found Error";
   case SCIP_FILECREATEERROR: return "Cannot Create File";
   case SCIP_LPERROR: return "Error in LP Solver";
   case SCIP_NOPROBLEM: return "No Problem Exists";
   case SCIP_INVALIDCALL: return "Method Cannot Be Called at This Time in Solution Process";
   case SCIP_INVALIDDATA: return "Error In Input Data";
   case SCIP_INVALIDRESULT: return "Method Returned An Invalid Result Code";
   case SCIP_PLUGINNOTFOUND: return "A required plugin was not found";
   case SCIP_PARAMETERUNKNOWN: return "The parameter with the given name was not found";
   case SCIP_PARAMETERWRONGTYPE: return "The parameter is not of the expected type";
   case SCIP_PARAMETERWRONGVAL: return "The value is invalid for the given parameter";
   case SCIP_KEYALREADYEXISTING: return "The given key is already existing in table";
   case SCIP_MAXDEPTHLEVEL: return "Maximal branching depth level exceeded";
   case SCIP_BRANCHERROR: return "No branching could be created";
   default: return "Unknown Error Code";
   }
}

/** message handler callback */
static
void msginfo(
   SCIP_MESSAGEHDLR*     messagehdlr,        /**< message handler */
   FILE*                 file,               /**< file to output in */
   const char*           msg                 /**< message string to output */
   )
{
    mexPrintf(msg);
    mexEvalString("drawnow;");  /* flush draw buffer */
}

/** check all inputs for size and type errors */
static
void checkInputs(
   const mxArray*        prhs[],             /* array of pointers to input arguments */
   int                   nrhs                /* number of inputs */
   )
{
   size_t ndec;
   size_t ncon;

   /* Correct number of inputs */
   if ( nrhs <= eUB )
      mexErrMsgTxt("You must supply at least 7 arguments to scip (H, f, A, lhs, rhs, lb, ub).");

   /* Check we have an objective */
   if ( mxIsEmpty(prhs[eF]) )
      mexErrMsgTxt("You must supply a linear objective function via f (all zeros if not required)!");

   /* check options is a structure */
   if ( nrhs > eOPTS && ! mxIsEmpty(prhs[eOPTS]) && ! mxIsStruct(prhs[eOPTS]) )
      mexErrMsgTxt("The options argument must be a structure!");

   /* get sizes */
   ndec = mxGetNumberOfElements(prhs[eF]);
   ncon = mxGetM(prhs[eA]);

   /* check quadratic objective */
   if ( ! mxIsEmpty(prhs[eH]) )
   {
      if ( mxGetM(prhs[eH]) != ndec || mxGetN(prhs[eH]) != ndec )
         mexErrMsgTxt("H has incompatible dimensions.");

      if ( ! mxIsSparse(prhs[eH]) )
         mexErrMsgTxt("H must be a sparse matrix.");
   }

   /* check sparsity (only supported in A) */
   if ( ! mxIsEmpty(prhs[eA]) )
   {
      if ( mxIsSparse(prhs[eF]) || mxIsSparse(prhs[eLHS]) || mxIsSparse(prhs[eLB]) )
         mexErrMsgTxt("Only A is a sparse matrix.");
      if ( ! mxIsSparse(prhs[eA]) )
         mexErrMsgTxt("A must be a sparse matrix");
   }

   /* check xtype data type */
   if ( nrhs > eXTYPE && ! mxIsEmpty(prhs[eXTYPE]) && mxGetClassID(prhs[eXTYPE]) != mxCHAR_CLASS )
      mexErrMsgTxt("xtype must be a char array.");

   /* check SOS structure */
   if ( nrhs > eSOS && ! mxIsEmpty(prhs[eSOS]) )
   {
      if ( ! mxIsStruct(prhs[eSOS]) )
         mexErrMsgTxt("The SOS argument must be a structure!");

      if ( mxGetFieldNumber(prhs[eSOS], "type") < 0 )
         mexErrMsgTxt("The SOS structure should contain the field 'type'.");

      if ( mxGetFieldNumber(prhs[eSOS], "index") < 0 )
         mexErrMsgTxt("The SOS structure should contain the field 'index'.");

      if ( mxGetFieldNumber(prhs[eSOS], "weight") < 0 )
         mexErrMsgTxt("The SOS structure should contain the field 'weight'.");

      int no_sets = (int)mxGetNumberOfElements(mxGetField(prhs[eSOS],0,"type"));

      if ( no_sets > 1 )
      {
         if ( ! mxIsCell(mxGetField(prhs[eSOS], 0, "index")) || mxIsEmpty(mxGetField(prhs[eSOS], 0, "index")) )
            mexErrMsgTxt("sos.index must be a cell array, and not empty!");

         if ( ! mxIsCell(mxGetField(prhs[eSOS], 0, "weight")) || mxIsEmpty(mxGetField(prhs[eSOS], 0, "weight")) )
            mexErrMsgTxt("sos.weight must be a cell array, and not empty!");

         if ( mxGetNumberOfElements(mxGetField(prhs[eSOS], 0, "index")) != no_sets )
            mexErrMsgTxt("sos.index cell array is not the same length as sos.type!");

         if ( mxGetNumberOfElements(mxGetField(prhs[eSOS], 0, "weight")) != no_sets )
            mexErrMsgTxt("sos.weight cell array is not the same length as sos.type!");
      }
   }

   /* check QC structure */
   if ( nrhs > eQC && ! mxIsEmpty(prhs[eQC]) )
   {
      if ( ! mxIsStruct(prhs[eQC]) )
         mexErrMsgTxt("The QC argument must be a structure!");

      if ( mxGetFieldNumber(prhs[eQC], "Q") < 0 )
         mexErrMsgTxt("The QC structure should contain the field 'Q'.");

      if ( mxGetFieldNumber(prhs[eQC], "l") < 0 )
         mexErrMsgTxt("The QC structure should contain the field 'l'.");

      if ( mxGetFieldNumber(prhs[eQC], "qrl") < 0 )
         mexErrMsgTxt("The QC structure should contain the field 'qrl'.");

      if ( mxGetFieldNumber(prhs[eQC], "qru") < 0 )
         mexErrMsgTxt("The QC structure should contain the field 'qru'.");

      if ( mxGetNumberOfElements(mxGetField(prhs[eQC], 0, "qrl")) != mxGetNumberOfElements(mxGetField(prhs[eQC], 0, "qru")) )
         mexErrMsgTxt("qrl and qru should have the the same number of elements.");

      int no_qc = (int)mxGetNumberOfElements(mxGetField(prhs[eQC], 0, "qrl"));
      if ( no_qc > 1 )
      {
         if ( ! mxIsCell(mxGetField(prhs[eQC], 0, "Q")) || mxIsEmpty(mxGetField(prhs[eQC], 0, "Q")))
            mexErrMsgTxt("Q must be a cell array, and not empty!");

         if ( mxGetNumberOfElements(mxGetField(prhs[eQC], 0, "Q")) != no_qc )
            mexErrMsgTxt("You must have a Q specified for each row in qrl, qru, and column in l.");

         /* check each Q */
         mxArray* Q;

         for(int i = 0; i < no_qc; i++)
         {
            Q = mxGetCell(mxGetField(prhs[eQC], 0, "Q"), i);
            if ( ! mxIsSparse(Q) )
               mexErrMsgTxt("Q must be sparse!");

            if ( mxGetM(Q) != ndec || mxGetN(Q) != ndec )
               mexErrMsgTxt("Q must be an n x n square matrix.");
         }
      }
      else  /* just one QC */
      {
         if ( mxIsEmpty(mxGetField(prhs[eQC], 0, "Q")))
            mexErrMsgTxt("Q must not be empty!");

         if ( ! mxIsSparse(mxGetField(prhs[eQC], 0, "Q")) )
            mexErrMsgTxt("Q must be sparse!");

         if ( mxGetM(mxGetField(prhs[eQC], 0, "Q")) != ndec || mxGetN(mxGetField(prhs[eQC], 0, "Q")) != ndec )
            mexErrMsgTxt("Q must be an n x n square matrix.");
      }

      /* common checks */
      if ( mxIsEmpty(mxGetField(prhs[eQC], 0, "l")) )
         mexErrMsgTxt("l must not be empty!");

      if ( mxIsSparse(mxGetField(prhs[eQC], 0, "l")) )
         mexErrMsgTxt("l matrix must be dense!");

      if ( mxGetN(mxGetField(prhs[eQC], 0, "l")) != no_qc )
         mexErrMsgTxt("l matrix/vector does not have the same number of columns as there are elements in qrl/qru.");

      if ( mxGetM(mxGetField(prhs[eQC], 0, "l")) != ndec )
         mexErrMsgTxt("l matrix/vector does not have the same number of rows as ndec.");
   }

   /* Check NL structure */
   if ( nrhs > eNLCON && ! mxIsEmpty(prhs[eNLCON]) )
   {
      if ( ! mxIsStruct(prhs[eNLCON]) )
         mexErrMsgTxt("The NL argument must be a structure!");

      if ( mxGetFieldNumber(prhs[eNLCON], "instr") < 0 && mxGetFieldNumber(prhs[eNLCON], "obj_instr") < 0 )
         mexErrMsgTxt("The NL structure should contain the field 'instr' or 'obj_instr'.");

      if ( mxGetField(prhs[eNLCON], 0, "instr") )
      {
         if ( mxGetFieldNumber(prhs[eNLCON], "cl") < 0 )
            mexErrMsgTxt("The NL structure should contain the field 'cl' when specifying nonlinear constraints.");

         if ( mxGetFieldNumber(prhs[eNLCON], "cu") < 0 )
            mexErrMsgTxt("The NL structure should contain the field 'cu' when specifying nonlinear constraints.");

         if ( mxGetNumberOfElements(mxGetField(prhs[eNLCON], 0, "cl")) != mxGetNumberOfElements(mxGetField(prhs[eNLCON], 0, "cu")) )
            mexErrMsgTxt("The number of elements in cl and cu is not the same.");

         /* check number of constraints the same as cl/cu */
         if ( mxIsCell(mxGetField(prhs[eNLCON], 0, "instr")) )
         {
            if ( mxGetNumberOfElements(mxGetField(prhs[eNLCON], 0, "instr")) != mxGetNumberOfElements(mxGetField(prhs[eNLCON], 0, "cl")) )
               mexErrMsgTxt("The number of constraints specified by cell array nl.instr does not match the length of vectors cl & cu.");
         }
         else
         {
            if ( mxGetNumberOfElements(mxGetField(prhs[eNLCON], 0, "cl")) != 1 )
               mexErrMsgTxt("When nl.instr is not a cell (single constraint), cl and cu are expected to be scalars.");
         }
      }
   }

   /* check sizes */
   if ( ncon )
   {
      if ( mxGetN(prhs[eA]) != ndec )
         mexErrMsgTxt("A has incompatible dimensions.");

      if ( ! mxIsEmpty(prhs[eLHS]) && mxGetNumberOfElements(prhs[eLHS]) != ncon )
         mexErrMsgTxt("lhs has incompatible dimensions.");

      if ( ! mxIsEmpty(prhs[eRHS]) && mxGetNumberOfElements(prhs[eRHS]) != ncon )
         mexErrMsgTxt("rhs has incompatible dimensions.");
   }

   if ( ! mxIsEmpty(prhs[eLB]) && (mxGetNumberOfElements(prhs[eLB]) != ndec) )
      mexErrMsgTxt("lb has incompatible dimensions.");

   if ( ! mxIsEmpty(prhs[eUB]) && (mxGetNumberOfElements(prhs[eUB]) != ndec) )
      mexErrMsgTxt("ub has incompatible dimensions");

   if ( nrhs > eXTYPE && ! mxIsEmpty(prhs[eXTYPE]) && (mxGetNumberOfElements(prhs[eXTYPE]) != ndec) )
      mexErrMsgTxt("xtype has incompatible dimensions");

   if ( nrhs > eX0 && ! mxIsEmpty(prhs[eX0]) && (mxGetNumberOfElements(prhs[eX0]) != ndec) )
      mexErrMsgTxt("x0 has incompatible dimensions");
}

/** get long integer option */
static
void getLongIntOption(
   const mxArray*        opts,               /**< options array */
   const char*           option,             /**< option to get */
   SCIP_Longint&         val                 /**< value */
   )
{
   /* leave value untouched if not specified */
   if ( mxGetField(opts, 0, option) )
      val = (SCIP_Longint) *mxGetPr(mxGetField(opts, 0, option));
}

/** get integer option */
static
void getIntOption(
   const mxArray*        opts,               /**< options array */
   const char*           option,             /**< option to get */
   int&                  val                 /**< value */
   )
{
   /* leave value untouched if not specified */
   if ( mxGetField(opts, 0, option) )
      val = (int) *mxGetPr(mxGetField(opts, 0, option));
}

/** get double option */
static
void getDblOption(
   const mxArray*        opts,               /**< options array */
   const char*           option,             /**< option to get */
   double&               val                 /**< value */
   )
{
    if ( mxGetField(opts, 0, option) )
       val = *mxGetPr(mxGetField(opts, 0, option));
}

/** get string option */
static
int getStrOption(
   const mxArray*        opts,               /**< options array */
   const char*           option,             /**< option to get */
   char*                 str                 /**< option string */
   )
{
   mxArray* field = mxGetField(opts, 0, option);
   if ( field != NULL && ! mxIsEmpty(field) )
   {
      mxGetString(field, str, BUFSIZE);
      return 0;
   }
   else
   {
      return -1;
   }
}

/** process options specified by user
 *
 *  Options in the format {'name1', val1; 'name2', val2}
 */
static
void processUserOpts(
   SCIP*                 scip,               /**< SCIP instance */
   mxArray*              opts                /**< options array */
   )
{
   SCIP_RETCODE retcode;
   size_t no;
   char* name;
   char* str_val;
   SCIP_Bool boolval;
   mxArray* opt_name;
   mxArray* opt_val;
   SCIP_PARAM* p = NULL;

   if ( ! mxIsEmpty(opts) )
   {
      if ( ! mxIsCell(opts) || mxGetN(opts) != 2 )
         mexErrMsgTxt("SCIP Options (solverOpts) should be a cell array of the form {'name1', val1; 'name2', val2}.");

      /* process each option */
      no = mxGetM(opts);

      for (size_t i = 0; i < no; i++)
      {
         p = NULL; /* ensures we know if we got a valid parameter or not! */

         opt_name = mxGetCell(opts, i);
         opt_val = mxGetCell(opts, i + no);

         if ( mxIsEmpty(opt_name) )
         {
            sprintf(msgbuf, "SCIP option name in cell row %zd is empty!", i + 1);
            mexErrMsgTxt(msgbuf);
         }

         if ( mxIsEmpty(opt_val) )
            continue; /* skip this one */

         if ( ! mxIsChar(opt_name) )
         {
            sprintf(msgbuf, "SCIP option name in cell row %zd is not a string!", i + 1);
            mexErrMsgTxt(msgbuf);
         }

         name = mxArrayToString(opt_name);

         /* attempt to get SCIP parameter information */
         p = SCIPgetParam(scip, name);

         /* no luck finding it */
         if ( p == NULL )
         {
            /* clean up SCIP here */
            sprintf(msgbuf, "SCIP option \"%s\" (row %zd) is not recognized!", name, i + 1);
            mxFree(name);
            mexErrMsgTxt(msgbuf);
         }

         /* based on parameter type, get from MATLAB and set via correct SCIP method */
         switch ( SCIPparamGetType(p) )
         {
         case SCIP_PARAMTYPE_BOOL:
            if ( ! mxIsDouble(opt_val) && ! mxIsLogical(opt_val) )
            {
               sprintf(msgbuf, "Error setting parameter \"%s\" - expected the value to be a double or logical.", name);
               mexErrMsgTxt(msgbuf);
            }

            if ( mxIsLogical(opt_val) )
            {
               bool* tval = (bool*)mxGetData(opt_val);
               if ( *tval )
                  boolval = TRUE;
               else
                  boolval = FALSE;
            }
            else
            {
               double* tval = (double*)mxGetPr(opt_val);
               if ( *tval != 0 )
                  boolval = TRUE;
               else
                  boolval = FALSE;
            }

            retcode = SCIPsetBoolParam(scip, name, boolval);
            if ( retcode != SCIP_OKAY )
            {
               sprintf(msgbuf, "Error setting SCIP bool option \"%s\" (Row %zd)! Please check the value is within range.", name, i + 1);
               mexErrMsgTxt(msgbuf);
            }
            break;

         case SCIP_PARAMTYPE_INT:
            if ( ! mxIsDouble(opt_val) )
            {
               sprintf(msgbuf, "Error setting parameter \"%s\" - Expected the value to be a double.", name);
               mexErrMsgTxt(msgbuf);
            }

            retcode = SCIPsetIntParam(scip, name, (int) *mxGetPr(opt_val));
            if ( retcode != SCIP_OKAY )
            {
               sprintf(msgbuf, "Error setting SCIP integer option \"%s\" (row %zd)! Please check the value is within range.", name, i + 1);
               mexErrMsgTxt(msgbuf);
            }
            break;

         case SCIP_PARAMTYPE_LONGINT:
            if ( ! mxIsDouble(opt_val) )
            {
               sprintf(msgbuf, "Error setting parameter \"%s\" - Expected the value to be a double.", name);
               mexErrMsgTxt(msgbuf);
            }

            retcode = SCIPsetLongintParam(scip, name, *mxGetPr(opt_val));
            if ( retcode != SCIP_OKAY )
            {
               sprintf(msgbuf, "Error setting SCIP longint option \"%s\" (Row %zd)! Please check the value is within range.", name, i + 1);
               mexErrMsgTxt(msgbuf);
            }
            break;

         case SCIP_PARAMTYPE_REAL:
            if ( ! mxIsDouble(opt_val) )
            {
               sprintf(msgbuf, "Error setting parameter \"%s\" - Expected the value to be a double.", name);
               mexErrMsgTxt(msgbuf);
            }

            retcode = SCIPsetRealParam(scip, name, *mxGetPr(opt_val));
            if ( retcode != SCIP_OKAY )
            {
               sprintf(msgbuf, "Error setting SCIP real option \"%s\" (Row %zd)! Please check the value is within range.", name, i + 1);
               mexErrMsgTxt(msgbuf);
            }
            break;

         case SCIP_PARAMTYPE_CHAR:
            if ( ! mxIsChar(opt_val) )
            {
               sprintf(msgbuf, "Error setting parameter \"%s\" - Expected the value to be a character.", name);
               mexErrMsgTxt(msgbuf);
            }
            str_val = mxArrayToString(opt_val);

            retcode = SCIPsetCharParam(scip, name, str_val[0]);
            if ( retcode != SCIP_OKAY )
            {
               sprintf(msgbuf, "Error setting SCIP char option \"%s\" (Row %zd)! Please check the value is a valid character.", name, i + 1);
               mxFree(str_val);
               mexErrMsgTxt(msgbuf);
            }
            mxFree(str_val);
            break;

         case SCIP_PARAMTYPE_STRING:
            if ( ! mxIsChar(opt_val) )
            {
               sprintf(msgbuf, "Error setting parameter \"%s\" - Expected the value to be a string", name);
               mexErrMsgTxt(msgbuf);
            }
            str_val = mxArrayToString(opt_val);

            retcode = SCIPsetStringParam(scip, name, str_val);
            if ( retcode != SCIP_OKAY )
            {
               sprintf(msgbuf,"Error setting SCIP string option \"%s\" (Row %zd)! Please check the value is a valid string.", name, i + 1);
               mxFree(str_val);
               mexErrMsgTxt(msgbuf);
            }
            mxFree(str_val);
            break;
         }
         /* free string memory */
         mxFree(name);
      }
   }
}

/*
SCIP_PARAMSETTING getEmphasisSetting(char* optsStr)
{
    if ( strcmp(optsStr, "default") == 0 )
        return SCIP_PARAMSETTING_DEFAULT;
    else if ( strcmp(optsStr, "aggressive") == 0 )
        return SCIP_PARAMSETTING_AGGRESSIVE;
    else if ( strcmp(optsStr, "fast") == 0 )
        return SCIP_PARAMSETTING_FAST;
    else if ( strcmp(optsStr, "off") == 0 )
        return SCIP_PARAMSETTING_OFF;
    else
    {
        sprintf(msgbuf,"Error Setting Emphasis Option - Unknown Emphasis Type \"%s\".", optsStr);
        mexErrMsgTxt(msgbuf);
    }
}
*/

/* // Process Emphasis Settings
void processEmphasisOptions(SCIP *scip, mxArray *opts)
{
    char optsStr[BUFSIZE]; optsStr[0]   = NULL;
    SCIP_SET* set;//                       = scip->set;
    SCIP_PARAMSET* paramset             = set->paramset;
    SCIP_MESSAGEHDLR* messagehdlr;//       = scip->messagehdlr;

    // Check for global emphasis (set this first)
    if (getStrOption(opts, "globalEmphasis", optsStr) == 0)
    {
        if ( strcmp(optsStr, "default") == 0 )
        {
            SCIP_ERR( SCIPparamsetSetEmphasis(paramset, set, messagehdlr, SCIP_PARAMEMPHASIS_DEFAULT, TRUE), "Error Setting Global Emphasis to default");
        }
        else if ( strcmp(optsStr, "counter") == 0 )
        {
            SCIP_ERR( SCIPparamsetSetEmphasis(paramset, set, messagehdlr, SCIP_PARAMEMPHASIS_COUNTER, TRUE), "Error Setting Global Emphasis to counter" );
        }
        else if ( strcmp(optsStr, "cpsolver") == 0 )
        {
            SCIP_ERR( SCIPparamsetSetEmphasis(paramset, set, messagehdlr, SCIP_PARAMEMPHASIS_CPSOLVER, TRUE), "Error Setting Global Emphasis to cpsolver" );
        }
        else if ( strcmp(optsStr, "easycip") == 0 )
        {
            SCIP_ERR( SCIPparamsetSetEmphasis(paramset, set, messagehdlr, SCIP_PARAMEMPHASIS_EASYCIP, TRUE), "Error Setting Global Emphasis to easycip" );
        }
        else if ( strcmp(optsStr, "feasibility") == 0 )
        {
            SCIP_ERR( SCIPparamsetSetEmphasis(paramset, set, messagehdlr, SCIP_PARAMEMPHASIS_FEASIBILITY, TRUE), "Error Setting Global Emphasis to feasibility" );
        }
        else if ( strcmp(optsStr, "hardlp") == 0 )
        {
            SCIP_ERR( SCIPparamsetSetEmphasis(paramset, set, messagehdlr, SCIP_PARAMEMPHASIS_HARDLP, TRUE), "Error Setting Global Emphasis to hardlp" );
        }
        else if ( strcmp(optsStr, "optimality") == 0 )
        {
            SCIP_ERR( SCIPparamsetSetEmphasis(paramset, set, messagehdlr, SCIP_PARAMEMPHASIS_OPTIMALITY, TRUE), "Error Setting Global Emphasis to optimality" );
        }
        else
        {
            sprintf(msgbuf,"Error setting SCIP globalEmphasis Option - Unknown Emphasis Type \"%s\".",optsStr);
            mexErrMsgTxt(msgbuf);
        }
    }

    // Remainder of emphasis options
    if (getStrOption(opts, "heuristicsEmphasis", optsStr) == 0)
    {
        SCIP_PARAMSETTING paramsetting = getEmphasisSetting(optsStr);
        SCIP_ERR( SCIPparamsetSetHeuristics(paramset, set, messagehdlr, paramsetting, TRUE), "Error Setting Heuristics Emphasis" );
    }
    if (getStrOption(opts, "presolvingEmphasis", optsStr) == 0)
    {
        SCIP_PARAMSETTING paramsetting = getEmphasisSetting(optsStr);
        SCIP_ERR( SCIPparamsetSetPresolving(paramset, set, messagehdlr, paramsetting, TRUE), "Error Setting Presolving Emphasis" );
    }
    if (getStrOption(opts, "separatingEmphasis", optsStr) == 0)
    {
        SCIP_PARAMSETTING paramsetting = getEmphasisSetting(optsStr);
        SCIP_ERR( SCIPparamsetSetSeparating(paramset, set, messagehdlr, paramsetting, TRUE), "Error Setting Separating Emphasis" );
    }
}
*/

/** main function */
void mexFunction(
   int                   nlhs,               /* number of expected outputs */
   mxArray*              plhs[],             /* array of pointers to output arguments */
   int                   nrhs,               /* number of inputs */
   const mxArray*        prhs[]              /* array of pointers to input arguments */
   )
{
   /* input arguments */
   double* H;
   double* f;
   double* A;
   double* lhs;
   double* rhs;
   double* lb;
   double* ub;
   double* sosind;
   double* soswt;
   double* Q;
   double* l;
   double* qrl;
   double* qru;
   double* x0 = NULL;
   char* xtype;
   char* sostype = NULL;
   char fpath[BUFSIZE];

   /* output arguments */
   double* x;
   double* fval;
   double* exitflag;
   double* iter;
   double* nodes;
   double* gap;
   double* pbound;
   double* dbound;
   const char* fnames[5] = {"LPiter", "BBnodes", "BBgap", "PrimalBound", "DualBound"};

   /* common options */
   SCIP_Longint maxlpiter = -1LL;
   SCIP_Longint maxnodes = -1LL;
   double maxtime = 1e20;
   double primtol = SCIP_DEFAULT_FEASTOL;
   double objbias = 0.0;
   int printLevel = 0;
   int optsEntry = 0;
   char probfile[BUFSIZE]; probfile[0] = '\0';
   mxArray* OPTS;

   /* internal vars */
   size_t ncon = 0;
   size_t ndec = 0;
   size_t ncnt = 0;
   size_t nint = 0;
   size_t nbin = 0;
   size_t i;
   size_t j;
   size_t k;
   int alhs = 0;
   int arhs = 0;
   int alb = 0;
   int aub = 0;
   int no = 0;
   int tm = 0;
   int ts = 1;

   /* sparse indexing */
   mwIndex* H_ir;
   mwIndex* H_jc;
   mwIndex* A_ir;
   mwIndex* A_jc;
   mwIndex* Q_ir;
   mwIndex* Q_jc;
   mwIndex startRow;
   mwIndex stopRow;

   /* SCIP objects */
   SCIP* scip;
   SCIP_VAR** vars = NULL;
   SCIP_CONS** cons = NULL;
   SCIP_VAR* qobj;
   SCIP_VAR* objb = NULL;

   /* return version string if there are no inputs */
   if ( nrhs < 1 )
   {
      if ( nlhs >= 1 )
      {
         sprintf(msgbuf, "%d.%d.%d", SCIPmajorVersion(), SCIPminorVersion(), SCIPtechVersion());
         plhs[0] = mxCreateString(msgbuf);
         plhs[1] = mxCreateDoubleScalar(3.00);
      }
      return;
   }

   /* check inputs */
   checkInputs(prhs, nrhs);

   /* create SCIP object */
   SCIP_ERR( SCIPcreate(&scip), "Error creating SCIP object.");
   SCIP_ERR( SCIPincludeDefaultPlugins(scip), "Error including SCIP default plugins.");

   /* add Ctrl-C event handler */
   SCIP_ERR( SCIPincludeCtrlCEventHdlr(scip), "Error adding Ctrl-C Event Handler.");

   /* options in normal places */
   optsEntry = eOPTS;
   OPTS = (mxArray*)prhs[eOPTS];

   /* get common options if specified */
   if ( nrhs > optsEntry )
   {
      getLongIntOption(OPTS, "maxiter", maxlpiter);
      getLongIntOption(OPTS, "maxnodes", maxnodes);
      getDblOption(OPTS, "maxtime", maxtime);
      getDblOption(OPTS, "tolrfun", primtol);
      getDblOption(OPTS, "objbias", objbias);
      getIntOption(OPTS, "display", printLevel);
      /* make sure level is ok */
      if ( printLevel < 0 )
         printLevel = 0;
      if ( printLevel > 5 )
         printLevel = 5;

      /* Check for nonlinear testing mode */
      getIntOption(OPTS, "testmode", tm);

      /* Check for writing mode */
      getStrOption(OPTS, "probfile", probfile);
      CheckOptiVersion(OPTS);

      /* set common options */
      if ( ! SCIPisInfinity(scip, maxtime) )
      {
         SCIP_ERR( SCIPsetRealParam(scip, "limits/time", maxtime), "Error setting maxtime.");
      }
      if ( maxlpiter >= 0LL )
      {
         SCIP_ERR( SCIPsetLongintParam(scip, "lp/iterlim", maxlpiter), "Error setting LP iterlim.");
      }
      if ( maxnodes >= 0 )
      {
         SCIP_ERR( SCIPsetLongintParam(scip, "limits/nodes", maxnodes), "Error setting nodes.");
      }
      if ( primtol != SCIP_DEFAULT_FEASTOL )
      {
         SCIP_ERR( SCIPsetRealParam(scip, "numerics/feastol", primtol), "Error setting lpfeastol.");
      }
   }

   /* if user has requested print out */
   if ( printLevel )
   {
      /* create message handler */
      SCIP_MESSAGEHDLR* mexprinter;

      SCIPmessagehdlrCreate(&mexprinter, TRUE, NULL, FALSE, &msginfo, &msginfo, &msginfo, NULL, NULL);
      SCIP_ERR( SCIPsetMessagehdlr(scip, mexprinter), "Error adding message handler.");
   }

   /* set verbosity level */
   SCIP_ERR( SCIPsetIntParam(scip, "display/verblevel", printLevel), "Error setting verblevel.");

   if ( printLevel )
   {
      SCIPprintVersion(scip, NULL);
      SCIPinfoMessage(scip, NULL, "\n");

      SCIPprintExternalCodes(scip, NULL);
      SCIPinfoMessage(scip, NULL, "\n");
   }

   /* get pointers to input vars */
   if ( ! mxIsEmpty(prhs[eH]) )
   {
      H = mxGetPr(prhs[eH]);
      H_ir = mxGetIr(prhs[eH]);
      H_jc = mxGetJc(prhs[eH]);
   }
   else
   {
      H = NULL;
      H_ir = NULL;
      H_jc = NULL;
   }
   f = mxGetPr(prhs[eF]);
   A = mxGetPr(prhs[eA]);
   A_ir = mxGetIr(prhs[eA]);
   A_jc = mxGetJc(prhs[eA]);
   lhs = mxGetPr(prhs[eLHS]);
   rhs = mxGetPr(prhs[eRHS]);
   lb = mxGetPr(prhs[eLB]);
   ub = mxGetPr(prhs[eUB]);

   if ( nrhs > eXTYPE )
      xtype = mxArrayToString(prhs[eXTYPE]);

   /* get sizes from input args */
   ndec = mxGetNumberOfElements(prhs[eF]);
   ncon = mxGetM(prhs[eA]);

   /* create outputs */
   plhs[0] = mxCreateDoubleMatrix(ndec, 1, mxREAL);
   plhs[1] = mxCreateDoubleMatrix(1, 1, mxREAL);
   plhs[2] = mxCreateDoubleMatrix(1, 1, mxREAL);
   plhs[3] = mxCreateDoubleMatrix(1, 1, mxREAL);

   x = mxGetPr(plhs[0]);         /* solution */
   fval = mxGetPr(plhs[1]);      /* objective value */
   exitflag = mxGetPr(plhs[2]);  /* flag */

   /* statistic structure output */
   plhs[3] = mxCreateStructMatrix(1, 1, 5, fnames);
   mxSetField(plhs[3], 0, fnames[0], mxCreateDoubleMatrix(1, 1, mxREAL));
   mxSetField(plhs[3], 0, fnames[1], mxCreateDoubleMatrix(1, 1, mxREAL));
   mxSetField(plhs[3], 0, fnames[2], mxCreateDoubleMatrix(1, 1, mxREAL));
   mxSetField(plhs[3], 0, fnames[3], mxCreateDoubleMatrix(1, 1, mxREAL));
   mxSetField(plhs[3], 0, fnames[4], mxCreateDoubleMatrix(1, 1, mxREAL));
   iter  = mxGetPr(mxGetField(plhs[3], 0, fnames[0]));
   nodes = mxGetPr(mxGetField(plhs[3], 0, fnames[1]));
   gap   = mxGetPr(mxGetField(plhs[3], 0, fnames[2]));
   pbound = mxGetPr(mxGetField(plhs[3], 0, fnames[3]));
   dbound = mxGetPr(mxGetField(plhs[3], 0, fnames[4]));

   /* create empty problem */
   SCIP_ERR( SCIPcreateProbBasic(scip, "OPTI Problem"), "Error creating basic SCIP problem");

   /* create continuous xtype array if empty or not supplied */
   if ( nrhs <= eXTYPE || mxIsEmpty(prhs[eXTYPE]) )
   {
      xtype = (char*)mxCalloc(ndec, sizeof(char));
      for (i = 0; i < ndec; i++)
         xtype[i] = 'c';
   }

   /* create infinite bounds if empty */
   if ( mxIsEmpty(prhs[eLB]) )
   {
      lb = (double*)mxCalloc(ndec, sizeof(double));
      alb = 1;
      for (i = 0; i < ndec; i++)
         lb[i] = -SCIPinfinity(scip);
   }

   if ( mxIsEmpty(prhs[eUB]))
   {
      ub = (double*)mxCalloc(ndec, sizeof(double));
      aub = 1;
      for (i = 0; i < ndec; i++)
         ub[i] = SCIPinfinity(scip);
   }

   /* create infinite lhs/rhs if empty */
   if ( mxIsEmpty(prhs[eLHS]) )
   {
      lhs = (double*)mxCalloc(ncon, sizeof(double));
      alhs = 1;
      for (i = 0; i < ncon; i++)
         lhs[i] = -SCIPinfinity(scip);
    }

   if ( mxIsEmpty(prhs[eRHS]))
   {
      rhs = (double*)mxCalloc(ncon, sizeof(double));
      arhs = 1;
      for (i = 0; i < ncon; i++)
         rhs[i] = SCIPinfinity(scip);
   }

   /* create SCIP variables (also loads linear objective + bounds) */
   SCIP_ERR( SCIPallocMemoryArray(scip, &vars, (int)ndec), "Error allocating variable memory");
   double llb;
   double lub;

   for (i = 0; i < ndec; i++)
   {
      SCIP_VARTYPE vartype;

      /* assign variable type */
      switch( tolower(xtype[i]) )
      {
      case 'i':
         vartype = SCIP_VARTYPE_INTEGER;
         llb = lb[i];
         lub = ub[i];
         sprintf(msgbuf, "ivar%zd", nint++);
         break;
      case 'b':
         vartype = SCIP_VARTYPE_BINARY;
         llb = SCIPisInfinity(scip, -lb[i]) ? 0 : lb[i]; /* if we don't do this, SCIP fails during presolve */
         lub = SCIPisInfinity(scip, ub[i])  ? 1 : ub[i];
         sprintf(msgbuf, "bvar%zd", nbin++);
         break;
      case 'c':
         vartype = SCIP_VARTYPE_CONTINUOUS;
         llb = lb[i];
         lub = ub[i];
         sprintf(msgbuf, "xvar%zd", ncnt++);
         break;
      default:
         sprintf(msgbuf, "Unknown variable type for variable %zd.", i);
         mexErrMsgTxt(msgbuf);
      }

      /* create variable */
      SCIP_ERR( SCIPcreateVarBasic(scip, &vars[i], msgbuf, llb, lub, f[i], vartype), "Error creating basic SCIP variable.");

      /* add to problem */
      SCIP_ERR( SCIPaddVar(scip, vars[i]), "Error adding SCIP variable to problem");
   }

   /* add objective bias term if non-zero */
   if ( objbias != 0.0 )
   {
      SCIP_ERR( SCIPcreateVarBasic(scip, &objb, "objbiasterm", objbias, objbias, 1.0, SCIP_VARTYPE_CONTINUOUS), "Error adding objective bias variable.");
      SCIP_ERR( SCIPaddVar(scip, objb), "Error adding objective bias variable.");
   }

   /* add quadratic objective (as quadratic constraint 0.5x'Hx - qobj = 0, and min(x) f'x + qobj, if exists) */
   if ( ! mxIsEmpty(prhs[eH]) )
   {
      /* create an unbounded variable to add to objective, representing the quadratic part */
      SCIP_ERR( SCIPcreateVarBasic(scip, &qobj, "quadobj", -SCIPinfinity(scip), SCIPinfinity(scip), 1.0, SCIP_VARTYPE_CONTINUOUS), "Error adding quadratic objective variable");
      SCIP_ERR( SCIPaddVar(scip, qobj), "Error adding quadratic objective variable.");

      /* create an empty quadratic constraint = 0.0 */
      SCIP_CONS* qobjc;
#if ( SCIP_VERSION >= 800 || ( SCIP_VERSION < 800 && SCIP_APIVERSION >= 100 ) )
      SCIP_ERR( SCIPcreateConsBasicQuadraticNonlinear(scip, &qobjc, "quadobj_con", 0, NULL, NULL, 0, NULL, NULL, NULL, 0.0, 0.0), "Error creating quadratic objective constraint.");
#else
      SCIP_ERR( SCIPcreateConsBasicQuadratic(scip, &qobjc, "quadobj_con", 0, NULL, NULL, 0, NULL, NULL, NULL, 0.0, 0.0), "Error creating quadratic objective constraint.");
#endif

      /* add linear term, connecting quadratic "constraint" to objective */
#if ( SCIP_VERSION >= 800 || ( SCIP_VERSION < 800 && SCIP_APIVERSION >= 100 ) )
      SCIP_ERR( SCIPaddLinearVarNonlinear(scip, qobjc, qobj, -1.0), "Error adding quadratic objective linear term.");
#else
      SCIP_ERR( SCIPaddLinearVarQuadratic(scip, qobjc, qobj, -1.0), "Error adding quadratic objective linear term.");
#endif

      /* Begin processing Hessian (note we expect the full H, not lower/upper triangular - to allow for non-convex and other not-nice problems) */
      for (i = 0; i < ndec; i++)
      {
         /* Determine number of nz in this column */
         startRow = H_jc[i];
         stopRow = H_jc[i+1];
         no = (int)(stopRow - startRow);

         /* if we have nz in this column */
         if ( no > 0 )
         {
            /* add each coefficient */
            for (j = startRow; j < stopRow; j++)
            {
               /* check for squared term, or bilinear */
               if ( i == H_ir[j] )
               {
                  /* diagonal */
#if ( SCIP_VERSION >= 800 || ( SCIP_VERSION < 800 && SCIP_APIVERSION >= 100 ) )
                  SCIP_EXPR* varexpr;
                  SCIP_EXPR* sqrexpr;

                  SCIP_ERR( SCIPcreateExprVar(scip, &varexpr, vars[i], NULL, NULL) , "Error creating expression.");
                  SCIP_ERR( SCIPcreateExprPow(scip, &sqrexpr, varexpr, 2.0, NULL, NULL), "Error creating expression." );

                  SCIP_ERR( SCIPaddExprNonlinear(scip, qobjc, sqrexpr, 0.5 * H[j]), "Error creating expression." );

                  SCIP_ERR( SCIPreleaseExpr(scip, &sqrexpr), "Error releasing expression.");
                  SCIP_ERR( SCIPreleaseExpr(scip, &varexpr), "Error releasing expression.");
#else
                  SCIP_ERR( SCIPaddSquareCoefQuadratic(scip, qobjc, vars[i], 0.5 * H[j]), "Error adding quadratic squared term.");
#endif
               }
               else
               {
#if ( SCIP_VERSION >= 800 || ( SCIP_VERSION < 800 && SCIP_APIVERSION >= 100 ) )
                  SCIP_EXPR* varexprs[2];
                  SCIP_EXPR* prodexpr;

                  SCIP_ERR( SCIPcreateExprVar(scip, &varexprs[0], vars[H_ir[j]], NULL, NULL), "Error creating expression.");
                  SCIP_ERR( SCIPcreateExprVar(scip, &varexprs[1], vars[i], NULL, NULL), "Error creating expression.");
                  SCIP_ERR( SCIPcreateExprProduct(scip, &prodexpr, 2, varexprs, 1.0, NULL, NULL), "Error creating expression.");

                  SCIP_ERR( SCIPaddExprNonlinear(scip, qobjc, prodexpr, 0.5 * H[j]), "Error creating expression.");

                  SCIP_ERR( SCIPreleaseExpr(scip, &prodexpr), "Error releasing expression.");
                  SCIP_ERR( SCIPreleaseExpr(scip, &varexprs[1]), "Error releasing expression.");
                  SCIP_ERR( SCIPreleaseExpr(scip, &varexprs[0]), "Error releasing expression.");
#else
                  SCIP_ERR( SCIPaddBilinTermQuadratic(scip, qobjc, vars[H_ir[j]], vars[i], 0.5 * H[j]), "Error adding quadratic bilinear term.");
#endif
               }
            }
         }
      }

      /* add the quadratic constraint, then release it */
      SCIP_ERR( SCIPaddCons(scip, qobjc), "Error adding quadratic objective constraint.");
      SCIP_ERR( SCIPreleaseCons(scip, &qobjc), "Error releaseing quadratic objective constraint.");
   }

   /* add linear constraints (if they exist) */
   if ( ncon )
   {
      /* allocate memory for all constraints (we create them all now, as we have to add coefficients in column order) */
      SCIP_ERR( SCIPallocMemoryArray(scip, &cons, (int)ncon), "Error allocating constraint memory.");

      /* create each constraint and add row bounds, but leave coefficients empty */
      for (i = 0; i < ncon; i++)
      {
         (void) SCIPsnprintf(msgbuf, BUFSIZE, "lincon%d", i);
         SCIP_ERR( SCIPcreateConsBasicLinear(scip, &cons[i], msgbuf, 0, NULL, NULL, lhs[i], rhs[i]), "Error creating basic SCIP linear constraint.");
      }

      /* now for each column (variable), add coefficients */
      for (i = 0; i < ndec; i++)
      {
         /* determine number of nz in this column */
         startRow = A_jc[i];
         stopRow = A_jc[i+1];
         no = (int)(stopRow - startRow);

         /* if we have nz in this column */
         if ( no > 0 )
         {
            /* add each coefficient */
            for (j = startRow; j < stopRow; j++)
               SCIP_ERR( SCIPaddCoefLinear(scip, cons[A_ir[j]], vars[i], A[j]), "Error adding constraint linear coefficient.");
         }
      }

      /* now for each constraint, add it to the problem, then release it */
      for (i = 0; i < ncon; i++)
      {
         SCIP_ERR( SCIPaddCons(scip, cons[i]), "Error adding linear constraint.");
         SCIP_ERR( SCIPreleaseCons(scip, &cons[i]), "Error releasing linear constraint.");
      }
   }

   /* add SOS Constraints (if they exist) */
   if ( nrhs > eSOS && ! mxIsEmpty(prhs[eSOS]) )
   {
      /* determine the number of SOS to add */
      size_t no_sets = (int)mxGetNumberOfElements(mxGetField(prhs[eSOS], 0, "type"));

      if ( no_sets > 0 )
      {
         /* collect types */
         sostype = mxArrayToString(mxGetField(prhs[eSOS], 0, "type"));

         /* for each SOS constraint, create respective constraint, add it, then release it */
         SCIP_CONS* consos = NULL;
         size_t novars;

         for (i = 0; i < no_sets; i++)
         {
            /* create constraint name */
            (void) SCIPsnprintf(msgbuf, BUFSIZE, "soscon%d", i);

            /* Collect novars + ind + wt */
            if ( mxIsCell(mxGetField(prhs[eSOS], 0, "index")) )
            {
               novars = (int)mxGetNumberOfElements(mxGetCell(mxGetField(prhs[eSOS], 0, "index"), i));
               sosind = mxGetPr(mxGetCell(mxGetField(prhs[eSOS], 0, "index"), i));
            }
            else
            {
               novars = (int)mxGetNumberOfElements(mxGetField(prhs[eSOS], 0, "index"));
               sosind = mxGetPr(mxGetField(prhs[eSOS], 0, "index"));
            }
            if ( mxIsCell(mxGetField(prhs[eSOS], 0, "weight")) ) /* should check here soswt is the same length */
               soswt = mxGetPr(mxGetCell(mxGetField(prhs[eSOS], 0, "weight"), i));
            else
               soswt = mxGetPr(mxGetField(prhs[eSOS], 0, "weight"));

            /* switch based on SOS type */
            switch ( sostype[i] )
            {
            case '1':
               /* create an empty SOS1 constraint */
               SCIP_ERR( SCIPcreateConsBasicSOS1(scip, &consos, msgbuf, 0, NULL, NULL), "Error creating basic SCIP SOS1 constraint.");

               /* for each variable, add to SOS constraint */
               for (j = 0; j < novars; j++)
                  SCIP_ERR( SCIPaddVarSOS1(scip, consos, vars[(int)sosind[j]-1], soswt[j]), "Error adding SOS1 constraint."); /* remember -1 for Matlab indicies */
               break;

            case '2':
               /* Create an empty SOS2 constraint */
               SCIP_ERR( SCIPcreateConsBasicSOS2(scip, &consos, msgbuf, 0, NULL, NULL), "Error creating basic SCIP SOS2 constraint.");

               /* for each variable, add to SOS constraint */
               for (j = 0; j < novars; j++)
                  SCIP_ERR( SCIPaddVarSOS2(scip, consos, vars[(int)sosind[j]-1], soswt[j]), "Error adding SOS2 constraint."); /* remember -1 for Matlab indicies */
               break;

            default:
               sprintf(msgbuf, "Uknown SOS Type for SOS %zd", i);
               mexErrMsgTxt(msgbuf);
            }

            /* add the constraint to the problem, then release it */
            SCIP_ERR( SCIPaddCons(scip,consos), "Error adding SOS constraint.");
            SCIP_ERR( SCIPreleaseCons(scip, &consos), "Error releasing SOS constraint.");
         }
      }
   }

   /* add Quadratic Constraints (if they exist) */
   if ( nrhs > eQC && ! mxIsEmpty(prhs[eQC]) )
   {
      /* determine the number of constraints to add */
      size_t no_qc = (int)mxGetNumberOfElements(mxGetField(prhs[eQC], 0, "qrl"));

      if ( no_qc > 0 )
      {
         /* Collect l and r */
         l = mxGetPr(mxGetField(prhs[eQC], 0, "l"));
         qrl = mxGetPr(mxGetField(prhs[eQC], 0, "qrl"));
         qru = mxGetPr(mxGetField(prhs[eQC], 0, "qru"));

         /* for each QC, create respective constraint, add it, then release it */
         SCIP_CONS *conqc = NULL;
         for (i = 0; i < no_qc; i++)
         {
            /* create constraint name */
            (void) SCIPsnprintf(msgbuf, BUFSIZE, "qccon%d", i);

            /* collect Q */
            if ( mxIsCell(mxGetField(prhs[eQC], 0, "Q")))
            {
               Q = mxGetPr(mxGetCell(mxGetField(prhs[eQC], 0, "Q"), i));
               Q_ir = mxGetIr(mxGetCell(mxGetField(prhs[eQC], 0, "Q"), i));
               Q_jc = mxGetJc(mxGetCell(mxGetField(prhs[eQC], 0, "Q"), i));
            }
            else
            {
               Q = mxGetPr(mxGetField(prhs[eQC], 0, "Q"));
               Q_ir = mxGetIr(mxGetField(prhs[eQC], 0, "Q"));
               Q_jc = mxGetJc(mxGetField(prhs[eQC], 0, "Q"));
            }

            /* collect bounds */
            double lqrl;
            double lqru;
            lqrl = mxIsInf(qrl[i]) ? -SCIPinfinity(scip) : qrl[i];
            lqru = mxIsInf(qru[i]) ?  SCIPinfinity(scip) : qru[i];

            /* create an empty quadratic constraint <= r */
#if ( SCIP_VERSION >= 800 || ( SCIP_VERSION < 800 && SCIP_APIVERSION >= 100 ) )
            SCIP_ERR( SCIPcreateConsBasicQuadraticNonlinear(scip, &conqc, msgbuf, 0, NULL, NULL, 0, NULL, NULL, NULL, lqrl, lqru), "Error creating quadratic constraint.");
#else
            SCIP_ERR( SCIPcreateConsBasicQuadratic(scip, &conqc, msgbuf, 0, NULL, NULL, 0, NULL, NULL, NULL, lqrl, lqru), "Error creating quadratic constraint.");
#endif

            /* add linear terms */
            for (j = 0; j < ndec; j++)
            {
               if ( ! SCIPisFeasZero(scip, l[j+i*ndec]) )
               {
#if ( SCIP_VERSION >= 800 || ( SCIP_VERSION < 800 && SCIP_APIVERSION >= 100 ) )
                  SCIP_ERR( SCIPaddLinearVarNonlinear(scip, conqc, vars[j], l[j+i*ndec]), "Error adding quadratic objective linear term.");
#else
                  SCIP_ERR( SCIPaddLinearVarQuadratic(scip, conqc, vars[j], l[j+i*ndec]), "Error adding quadratic objective linear term.");
#endif
               }
            }

            /* begin processing Q (note we expect the full Q, not lower/upper triangular - to allow for non-convex problems) */
            for (k = 0; k < ndec; k++)
            {
               /* determine number of nz in this column */
               startRow = Q_jc[k];
               stopRow = Q_jc[k+1];
               no = (int)(stopRow - startRow);

               /* if we have nz in this column */
               if ( no > 0 )
               {
                  /* add each coefficient */
                  for (j = startRow; j < stopRow; j++)
                  {
                     /* check for squared term, or bilinear */
                     if ( k == Q_ir[j] )
                     {
                        /* diagonal */
#if ( SCIP_VERSION >= 800 || ( SCIP_VERSION < 800 && SCIP_APIVERSION >= 100 ) )
                        SCIP_EXPR* varexpr;
                        SCIP_EXPR* sqrexpr;

                        SCIP_ERR( SCIPcreateExprVar(scip, &varexpr, vars[k], NULL, NULL) , "Error creating expression.");
                        SCIP_ERR( SCIPcreateExprPow(scip, &sqrexpr, varexpr, 2.0, NULL, NULL), "Error creating expression." );

                        SCIP_ERR( SCIPaddExprNonlinear(scip, conqc, sqrexpr, Q[j]), "Error creating expression." );

                        SCIP_ERR( SCIPreleaseExpr(scip, &sqrexpr), "Error releasing expression.");
                        SCIP_ERR( SCIPreleaseExpr(scip, &varexpr), "Error releasing expression.");
#else
                        SCIP_ERR( SCIPaddSquareCoefQuadratic(scip, conqc, vars[k], Q[j]), "Error adding quadratic constraint squared term.");
#endif
                     }
                     else
                     {
#if ( SCIP_VERSION >= 800 || ( SCIP_VERSION < 800 && SCIP_APIVERSION >= 100 ) )
                        SCIP_EXPR* varexprs[2];
                        SCIP_EXPR* prodexpr;

                        SCIP_ERR( SCIPcreateExprVar(scip, &varexprs[0], vars[Q_ir[j]], NULL, NULL), "Error creating expression.");
                        SCIP_ERR( SCIPcreateExprVar(scip, &varexprs[1], vars[k], NULL, NULL), "Error creating expression.");
                        SCIP_ERR( SCIPcreateExprProduct(scip, &prodexpr, 2, varexprs, 1.0, NULL, NULL), "Error creating expression.");

                        SCIP_ERR( SCIPaddExprNonlinear(scip, conqc, prodexpr, Q[j]), "Error creating expression.");

                        SCIP_ERR( SCIPreleaseExpr(scip, &prodexpr), "Error releasing expression.");
                        SCIP_ERR( SCIPreleaseExpr(scip, &varexprs[1]), "Error releasing expression.");
                        SCIP_ERR( SCIPreleaseExpr(scip, &varexprs[0]), "Error releasing expression.");
#else
                        SCIP_ERR( SCIPaddBilinTermQuadratic(scip, conqc, vars[Q_ir[j]], vars[k], Q[j]), "Error adding quadratic constraint bilinear term.");
#endif
                     }
                  }
               }
            }

            /* add the constraint to the problem, then release it */
            SCIP_ERR( SCIPaddCons(scip,conqc), "Error adding quadratic constraint");
            SCIP_ERR( SCIPreleaseCons(scip,&conqc), "Error releasing quadratic constraint");
         }
      }
   }

   /* add nonlinear constraints and / or objective (if they exist) */
   if ( nrhs > eNLCON && ! mxIsEmpty(prhs[eNLCON]) )
   {
      double* instr;
      size_t ninstr = 0;
      double* conval;
      double cval;
      double* objval;
      double oval;
      double* xval = NULL;
      double err;

      /* check if we have constraint validation points to check against */
      if ( mxGetField(prhs[eNLCON], 0, "nlcon_val"))
      {
         conval = mxGetPr(mxGetField(prhs[eNLCON], 0, "nlcon_val"));

         /* check if we have initial guess to use */
         if ( mxGetField(prhs[eNLCON], 0, "xval") )
            xval = mxGetPr(mxGetField(prhs[eNLCON], 0, "xval"));
      }

      /* check if we have objective validation point to check against */
      if ( mxGetField(prhs[eNLCON], 0, "obj_val") )
      {
         objval = mxGetPr(mxGetField(prhs[eNLCON], 0, "obj_val"));

         /* check if we have initial guess to use */
         if ( mxGetField(prhs[eNLCON], 0, "xval") )
            xval = mxGetPr(mxGetField(prhs[eNLCON], 0, "xval")); /* same as above but may be uncon */
      }

      /* add nonlinear constraints */
      if ( mxGetField(prhs[eNLCON], 0, "instr") )
      {
         /* copy constraints bounds */
         mxArray* mcl = mxDuplicateArray(mxGetField(prhs[eNLCON], 0, "cl"));
         mxArray* mcu = mxDuplicateArray(mxGetField(prhs[eNLCON], 0, "cu"));

         /* get bounds, ensure finite */
         double* cl = mxGetPr(mcl);
         double* cu = mxGetPr(mcu);

         for (size_t i = 0; i < mxGetNumberOfElements(mcl); i++)
         {
            if ( mxIsInf(cl[i]) )
               cl[i] = -SCIPinfinity(scip);
            if ( mxIsInf(cu[i]) )
               cu[i] = SCIPinfinity(scip);
         }

         /* see if we have multiple constraints */
         if ( mxIsCell(mxGetField(prhs[eNLCON], 0, "instr")) )
         {
            /* for each cell, receive instruction list, add constraint, and verify */
            for (size_t i = 0; i < mxGetNumberOfElements(mxGetField(prhs[eNLCON], 0, "instr")); i++)
            {
               /* retrieve instructions */
               instr = mxGetPr(mxGetCell(mxGetField(prhs[eNLCON], 0, "instr"), i));
               ninstr = mxGetNumberOfElements(mxGetCell(mxGetField(prhs[eNLCON], 0, "instr"), i));

               /* add the constraint */
               cval = addNonlinearCon(scip, vars, instr, ninstr, cl[i], cu[i], xval, i, false);

               /* validate if possible */
               if ( xval != NULL )
               {
                  err = REALABS(cval - conval[i]);
                  if ( SCIPisFeasPositive(scip, err) )
                  {
                     sprintf(msgbuf, "Failed validation test on nonlinear constraint #%zd, difference: %1.6g", i, err);
                     mexWarnMsgTxt(msgbuf);
                     ts = 0;
                  }
#ifdef DEBUG
                  else
                     mexPrintf("-- Passed validation test on nonlinear constraint #%d --\n", i);
#endif
               }
            }
         }
         else /* only one constraint */
         {
            /* retrieve instructions */
            instr = mxGetPr(mxGetField(prhs[eNLCON], 0, "instr"));
            ninstr = mxGetNumberOfElements(mxGetField(prhs[eNLCON], 0, "instr"));

            /* add the constraint */
            cval = addNonlinearCon(scip, vars, instr, ninstr, *cl, *cu, xval, 0, false);

            /* Validate if possible */
            if ( xval != NULL )
            {
               err = REALABS(cval - *conval);
               if ( SCIPisFeasPositive(scip, err) )
               {
                  sprintf(msgbuf, "Failed validation test on nonlinear constraint #0, difference: %1.6g.", err);
                  mexWarnMsgTxt(msgbuf);
                  ts = 0;
               }
#ifdef DEBUG
               else
                  mexPrintf("-- Passed validation test on nonlinear constraint #0 --\n");
#endif
            }
         }
      }

      /* add nonlinear objective */
      if ( mxGetField(prhs[eNLCON], 0, "obj_instr"))
      {
         /* retrieve instructions */
         instr = mxGetPr(mxGetField(prhs[eNLCON], 0, "obj_instr"));
         ninstr = mxGetNumberOfElements(mxGetField(prhs[eNLCON], 0, "obj_instr"));

         /* add the objective as nonlinear constraint: obj(x) - nlobj = 0, and min(x) f'x + nlobj */
         oval = addNonlinearCon(scip, vars, instr, ninstr, 0, 0, xval, 0, true);

         /* validate if possible */
         if ( xval != NULL )
         {
            err = REALABS(oval - *objval);
            if ( SCIPisFeasPositive(scip, err) )
            {
               sprintf(msgbuf, "Failed validation test on nonlinear objective #0, difference: %1.6g", err);
               mexWarnMsgTxt(msgbuf);
               ts = 0;
            }
#ifdef DEBUG
            else
               mexPrintf("-- Passed validation test on nonlinear objective --\n\n\n");
#endif
         }
      }
   }

   /* process primal solution (if it exits) */
   if ( nrhs > eX0 && ! mxIsEmpty(prhs[eX0]) )
   {
      SCIP_SOL* sol;
      SCIP_Bool stored;

      x0 = mxGetPr(prhs[eX0]);
      assert( x0 != NULL );
      SCIP_ERR( SCIPcreateSol(scip, &sol, NULL), "Error creating empty solution");

      for (i = 0; i < ndec; i++)
      {
         SCIP_ERR( SCIPsetSolVal(scip, sol, vars[i], x0[i]), "Error creating setting solution value");
      }
      SCIP_ERR( SCIPaddSolFree(scip, &sol, &stored), "Error adding solution" );
   }

   /* process advanced user options (if they exist) */
   if ( nrhs > optsEntry )
   {
      /* emphasis settings */
      /* processEmphasisOptions(scip, OPTS); */

      /* process specific options (overriding emphasis options) */
      if ( mxGetField(OPTS, 0, "solverOpts") )
         processUserOpts(scip, mxGetField(OPTS, 0, "solverOpts"));
   }

   /* SCIP_ERR( SCIPwriteOrigProblem(scip, NULL, "cip", FALSE), "Error writing CIP File."); */

   /* solve problem if not in testing mode or gams writing mode */
   if ( tm == 0 && strlen(probfile) == 0 )
   {
      SCIP_RETCODE rc = SCIPsolve(scip);

      if ( rc != SCIP_OKAY )
      {
         /* clean up general SCIP memory (if possible) */
         SCIPfree(&scip);

         /* display error */
         sprintf(msgbuf, "Error Solving SCIP Problem, Error: %s (Code: %d)", scipErrCode(rc), rc);
         mexErrMsgTxt(msgbuf);
      }

      /* assign return arguments */
      if ( SCIPgetNSols(scip) > 0 )
      {
         SCIP_SOL* scipbestsol = SCIPgetBestSol(scip);

         /* assign x */
         for (i = 0; i < ndec; i++)
            x[i] = SCIPgetSolVal(scip, scipbestsol, vars[i]);

         /* assign fval */
         *fval = SCIPgetSolOrigObj(scip, scipbestsol);

         /* get solve statistics */
         *iter = (double)SCIPgetNLPIterations(scip);
         *nodes = (double)SCIPgetNTotalNodes(scip);
         *gap = SCIPgetGap(scip);
         *pbound = SCIPgetPrimalbound(scip);
         *dbound = SCIPgetDualbound(scip);
      }
      else /* no solution found */
      {
         *fval = std::numeric_limits<double>::quiet_NaN();
         *gap = std::numeric_limits<double>::infinity();
         *pbound = std::numeric_limits<double>::quiet_NaN();
      }

      /* get solution status */
      *exitflag = (double)SCIPgetStatus(scip);
   }
   /* write CIP file */
   else if ( strlen(probfile) )
   {
      /* presolve first */
      SCIP_ERR( SCIPpresolve(scip), "Error presolving SCIP problem!");

      /* now write */
      SCIP_ERR( SCIPwriteTransProblem(scip, probfile, NULL, false), "Error writing file.");
   }
   /* else return test status */
   else
   {
      *x = ts;
   }

   /* clean up memory from MATLAB mode */
   mxFree(xtype);

   if ( alhs )
      mxFree(lhs);
   alhs = 0;

   if ( arhs )
      mxFree(rhs);
   arhs = 0;

   if ( alb )
      mxFree(lb);
   alb = 0;

   if ( aub )
      mxFree(ub);
   aub = 0;

   if ( sostype )
      mxFree(sostype);
   sostype = NULL;

   /* release variables */
   for (i = 0; i < ndec; i++)
      SCIP_ERR( SCIPreleaseVar(scip, &vars[i]), "Error releasing SCIP variable.");

   if ( ! mxIsEmpty(prhs[eH]) )
      SCIP_ERR( SCIPreleaseVar(scip, &qobj), "Error releasing SCIP quadratic objective variable.");

   if ( objb != NULL )
      SCIP_ERR( SCIPreleaseVar(scip, &objb), "Error releasing SCIP objective bias variable.");

   /* now free SCIP arrays & problem */
   SCIPfreeMemoryArray(scip, &vars);

   if ( ncon )
      SCIPfreeMemoryArray(scip, &cons);

   /* clean up general SCIP memory */
   SCIP_ERR( SCIPfree(&scip), "Error releasing SCIP problem.");
}
