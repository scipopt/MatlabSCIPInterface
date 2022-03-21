/* SCIPSDPMEX - A MATLAB MEX Interface to SCIP-SDP
 * Released Under the BSD 3-Clause License.
 *
 * Based on code by Jonathan Currie.
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
#include <scipsdp/scipsdpdef.h>
#include <scipsdp/scipsdpdefplugins.h>
#include <scipsdp/cons_sdp.h>
#include <scip/cons_linear.h>
#include <scip/pub_paramset.h>
#include "scipeventmex.h"
#include "opti_build_utils.h"
#include "scipnlmex.h"

using namespace std;

/* enable for Debug print out */
/* #define DEBUG */

/** argument enumeration (in expected order of arguments) */
enum
{
   eF     = 0,           /**< f: objective function */
   eA     = 1,           /**< A: linear constraint matrix */
   eLHS   = 2,           /**< lhs: lhs of linear constraints */
   eRHS   = 3,           /**< rhs: rhs of linear constraints */
   eLB    = 4,           /**< lb: lower bounds */
   eUB    = 5,           /**< ub: upper bounds */
   eSDP   = 6,           /**< SDP constraints */
   eXTYPE = 7,           /**< xtype: variable types */
   eX0    = 8,           /**< x0: primal solution */
   eOPTS  = 9            /**< opts: SCIP options */
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

/* check all inputs for size and type errors */
static
void checkInputs(
   const mxArray*        prhs[],             /* array of pointers to input arguments */
   int                   nrhs                /* number of inputs */
   )
{
   size_t ndec;
   size_t ncon;

   /* correct number of inputs */
   if ( nrhs <= eUB )
      mexErrMsgTxt("You must supply at least 6 arguments to scipsdp (f, A, lhs, rhs, lb, ub, sdcone)");

   /* check we have an objective */
   if ( mxIsEmpty(prhs[eF]) )
      mexErrMsgTxt("You must supply a linear objective function via f (all zeros if not required)!");

   /* check options is a structure */
   if ( nrhs > eOPTS && ! mxIsEmpty(prhs[eOPTS]) && ! mxIsStruct(prhs[eOPTS]) )
      mexErrMsgTxt("The options argument must be a structure!");

   /* get Sizes */
   ndec = mxGetNumberOfElements(prhs[eF]);
   ncon = mxGetM(prhs[eA]);

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

   /* check sizes */
   if ( ncon )
   {
      if ( mxGetN(prhs[eA]) != ndec )
         mexErrMsgTxt("A has incompatible dimensions.");

      if ( ! mxIsEmpty(prhs[eLHS]) && mxGetNumberOfElements(prhs[eLHS]) != ncon )
         mexErrMsgTxt("lhs has incompatible dimensions.");

      if ( ! mxIsEmpty(prhs[eRHS]) &&mxGetNumberOfElements(prhs[eRHS]) != ncon )
         mexErrMsgTxt("rhs has incompatible dimensions.");

      if ( ! mxIsEmpty(prhs[eLB]) && mxGetNumberOfElements(prhs[eLB]) != ndec )
         mexErrMsgTxt("lb has incompatible dimensions");

      if ( ! mxIsEmpty(prhs[eUB]) && mxGetNumberOfElements(prhs[eUB]) != ndec )
         mexErrMsgTxt("ub has incompatible dimensions");

      if ( nrhs > eXTYPE && ! mxIsEmpty(prhs[eXTYPE]) && mxGetNumberOfElements(prhs[eXTYPE]) != ndec )
         mexErrMsgTxt("xtype has incompatible dimensions");
   }
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

/* add SDP constraint */
static
void addSDPConstraint(
   SCIP*                 scip,
   SCIP_VAR**            scipvars,
   const mxArray*        cone,
   int                   block
   )
{
   double* SDP_pr  = mxGetPr(cone);
   mwIndex* SDP_ir = mxGetIr(cone);
   mwIndex* SDP_jc = mxGetJc(cone);
   int SDP_M       = (int)mxGetM(cone);
   int SDP_N       = (int)mxGetN(cone); /* remember [C A0 A1 A2...] so non-square */
   int SDP_C_nnz   = 0; /* nnz in C */
   int SDP_A_nnz   = 0; /* nnz in current A */
   int SDP_DIM     = (int)(sqrt((double)SDP_M)); /* calculate dimension */
   int rind;
   int cind;
   size_t i;
   size_t j;
   size_t idx;
   size_t midx;

   /* data for SDP constraint */
   SCIP_VAR** vars = NULL;
   int* nvarnonz;
   int** col = NULL;
   int** row = NULL;
   SCIP_Real** val = NULL;
   int* const_col = NULL;
   int* const_row = NULL;
   SCIP_Real* const_val = NULL;
   int nnza = 0;
   int nnzc = 0;
   int nzerocoef = 0;

   /* determine nnz */
   SDP_C_nnz = (int)(SDP_jc[1] - SDP_jc[0]);
   SDP_A_nnz = (int)(SDP_jc[SDP_N] - SDP_C_nnz);

#ifdef DEBUG
   mexPrintf("SDP_DIM [block %d]: %d, M: %d, N: %d\n", block, SDP_DIM, SDP_M, SDP_N);
   mexPrintf("C nnz: %d, all A nnz: %d\n", SDP_C_nnz, SDP_A_nnz);
#endif

   /* allocate constraint memory (we allocate too much here ...) */
   SCIP_ERR( SCIPallocBlockMemoryArray(scip, &vars, SDP_N - 1), "Error Allocating SCIP-SDP Variable Memory.");
   SCIP_ERR( SCIPallocBlockMemoryArray(scip, &nvarnonz, SDP_N - 1), "Error Allocating SCIP-SDP Variable Memory.");
   SCIP_ERR( SCIPallocBlockMemoryArray(scip, &col, SDP_N - 1), "Error Allocating SCIP-SDP Column Memory.");
   SCIP_ERR( SCIPallocBlockMemoryArray(scip, &row, SDP_N - 1), "Error Allocating SCIP-SDP Row Memory.");
   SCIP_ERR( SCIPallocBlockMemoryArray(scip, &val, SDP_N - 1), "Error Allocating SCIP-SDP Values Memory.");
   for (i = 1; i < SDP_N; ++i)
   {
      SCIP_ERR( SCIPallocBlockMemoryArray(scip, &col[i-1], SDP_A_nnz), "Error Allocating SCIP-SDP Column Memory.");
      SCIP_ERR( SCIPallocBlockMemoryArray(scip, &row[i-1], SDP_A_nnz), "Error Allocating SCIP-SDP Row Memory.");
      SCIP_ERR( SCIPallocBlockMemoryArray(scip, &val[i-1], SDP_A_nnz), "Error Allocating SCIP-SDP Values Memory.");
   }
   SCIP_ERR( SCIPallocBlockMemoryArray(scip, &const_col, SDP_C_nnz), "Error Allocating SCIP-SDP Constant Column Memory.");
   SCIP_ERR( SCIPallocBlockMemoryArray(scip, &const_row, SDP_C_nnz), "Error Allocating SCIP-SDP Constant Row Memory.");
   SCIP_ERR( SCIPallocBlockMemoryArray(scip, &const_val, SDP_C_nnz), "Error Allocating SCIP-SDP Constant Value Memory.");

   /* copy in C */
   idx = 0;
   for (i = 0; i < SDP_C_nnz; i++)
   {
      rind = SDP_ir[i] % SDP_DIM;
      cind = (int)((SDP_ir[i] - rind)/SDP_DIM);
      assert( 0 <= rind < SDP_DIM );
      assert( 0 <= cind < SDP_DIM );

      if ( rind >= cind )
      {
         if ( SCIPisZero(scip, SDP_pr[i]) )
            ++nzerocoef;
         else
         {
            const_row[idx] = rind;
            const_col[idx] = cind;
            const_val[idx] = SDP_pr[i];
            nnzc++;
#ifdef DEBUG
            mexPrintf("(%zd) - C[%d,%d] = %f\n", idx, const_row[idx], const_col[idx], const_val[idx]);
#endif
            idx++;
         }
      }
   }

   /* copy in all A_i */
   idx = 0;
   midx = SDP_C_nnz;
   for (i = 1; i < SDP_N; i++)
   {
      vars[i-1] = scipvars[i-1];
      idx = 0;
      for (j = SDP_jc[i]; j < SDP_jc[i+1]; j++)
      {
         rind = SDP_ir[midx] % SDP_DIM;
         cind = (int)((SDP_ir[midx] - rind)/SDP_DIM);
         assert( 0 <= rind < SDP_DIM );
         assert( 0 <= cind < SDP_DIM );

         if ( rind >= cind )
         {
            if ( SCIPisZero(scip, SDP_pr[midx]) )
               ++nzerocoef;
            else
            {
               row[i-1][idx] = rind;
               col[i-1][idx] = cind;
               val[i-1][idx] = SDP_pr[midx];
               nnza++;
#ifdef DEBUG
               mexPrintf("(%zd) - A[%zd][%d,%d] = %f.\n", idx, i - 1, row[i-1][idx], col[i-1][idx], val[i-1][idx]);
#endif
               idx++;
            }
         }
         midx++;
      }
      nvarnonz[i-1] = idx;
      assert( idx <= SDP_A_nnz );
      assert( midx <= SDP_A_nnz + SDP_C_nnz );
   }
   assert( nnza <= SDP_A_nnz );

   /* Create SCIP Constraint */
   SCIP_CONS* sdpcon;
   SCIPsnprintf(msgbuf, BUFSIZE, "SDP-%d", block);
   SCIP_ERR( SCIPcreateConsSdp(scip, &sdpcon, msgbuf, SDP_N - 1, nnza, SDP_DIM, nvarnonz, col, row, val, vars, nnzc, const_col, const_row, const_val, TRUE), "Error Creating SDP Constraint." );
   SCIP_ERR( SCIPaddCons(scip, sdpcon), "Error Adding SDP Constraint." );
   SCIP_ERR( SCIPreleaseCons(scip, &sdpcon), "Error Releasing SDP Constraint." );
#ifdef DEBUG
   mexPrintf("Added SDP constraint %d.\n",block);
#endif
   if ( nzerocoef > 0 )
      mexPrintf("Found %d coefficients with absolute value less than epsilon = %g.\n", nzerocoef, SCIPepsilon(scip));

   for (i = SDP_N - 1; i > 0; --i)
   {
      SCIPfreeBlockMemoryArray(scip, &col[i-1], SDP_A_nnz);
      SCIPfreeBlockMemoryArray(scip, &row[i-1], SDP_A_nnz);
      SCIPfreeBlockMemoryArray(scip, &val[i-1], SDP_A_nnz);
   }
   SCIPfreeBlockMemoryArray(scip, &col, SDP_N-1);
   SCIPfreeBlockMemoryArray(scip, &row, SDP_N-1);
   SCIPfreeBlockMemoryArray(scip, &val, SDP_N-1);
   SCIPfreeBlockMemoryArray(scip, &const_col, SDP_C_nnz);
   SCIPfreeBlockMemoryArray(scip, &const_row, SDP_C_nnz);
   SCIPfreeBlockMemoryArray(scip, &const_val, SDP_C_nnz);

   SCIPfreeBlockMemoryArray(scip, &vars, SDP_N - 1);
   SCIPfreeBlockMemoryArray(scip, &nvarnonz, SDP_N - 1);
}

/** main function */
void mexFunction(
   int                   nlhs,               /* number of expected outputs */
   mxArray*              plhs[],             /* array of pointers to output arguments */
   int                   nrhs,               /* number of inputs */
   const mxArray*        prhs[]              /* array of pointers to input arguments */
   )
{
   /* input arguments */
   double* f;
   double* A;
   double* lhs;
   double* rhs;
   double* lb;
   double* ub;
   char* xtype;

   /* output arguments */
   double* x;
   double* fval;
   double* exitflag;
   double* nodes;
   double* gap;
   double* pbound;
   double* dbound;
   double* x0 = NULL;
   const char* fnames[4] = {"BBnodes", "BBgap", "PrimalBound", "DualBound"};

   /* common options */
   SCIP_Longint maxnodes = -1LL;
   double maxtime = 1e20;
   double primtol = SCIP_DEFAULT_FEASTOL;
   double objbias = 0.0;
   int maxpresolve = -1;
   char printlevelstr[BUFSIZE]; printlevelstr[0] = '\0';
   int printLevel = 0;
   int optsEntry = 0;
   char probfile[BUFSIZE]; probfile[0] = '\0';
   char presolvedfile[BUFSIZE]; presolvedfile[0] = '\0';
   mxArray* OPTS;

   /* internal vars */
   size_t ncones = 0;
   size_t ndec = 0;
   size_t ncon = 0;
   size_t ncnt = 0;
   size_t nint = 0;
   size_t nbin = 0;
   size_t i;
   size_t j;
   int alhs = 0;
   int arhs = 0;
   int alb = 0;
   int aub = 0;
   int no = 0;

   /* sparse indexing */
   mwIndex* A_ir;
   mwIndex* A_jc;
   mwIndex startRow;
   mwIndex stopRow;

   /* SCIPSDP objects */
   SCIP* scip;
   SCIP_VAR** vars = NULL;
   SCIP_CONS** cons = NULL;
   SCIP_VAR* objb = NULL;

   /* return version string if there are no inputs */
   if ( nrhs < 1 )
   {
      if ( nlhs >= 1 )
      {
         /* we currently do not have a SCIPSDP version */
         sprintf(msgbuf, "%d.%d.%d", SCIPSDPmajorVersion, SCIPSDPminorVersion, SCIPSDPtechVersion);
         plhs[0] = mxCreateString(msgbuf);
         plhs[1] = mxCreateDoubleScalar(3.00);
      }
      return;
   }

   /* check inputs */
   checkInputs(prhs, nrhs);

   /* create SCIP object */
   SCIP_ERR( SCIPcreate(&scip) , "Error creating SCIP object.");

   /* add SCIP-SDP plugins */
   SCIP_ERR( SCIPSDPincludeDefaultPlugins(scip), "Error including SCIP-SDP default plugins.");

   /* add Ctrl-C event handler */
   SCIP_ERR( SCIPincludeCtrlCEventHdlr(scip), "Error adding Ctrl-C Event Handler.");

   /* options in normal places */
   optsEntry = eOPTS;
   OPTS = (mxArray*)prhs[eOPTS];

   /* get common options if specified */
   if ( nrhs > optsEntry )
   {
      getLongIntOption(OPTS, "maxnodes", maxnodes);
      getIntOption(OPTS, "maxpresolve", maxpresolve);
      getDblOption(OPTS, "maxtime", maxtime);
      getDblOption(OPTS, "tolrfun", primtol);
      getDblOption(OPTS, "objbias", objbias);
      getStrOption(OPTS, "display", printlevelstr);
      /* determine print level */
      if ( strcmp(printlevelstr, "iter") == 0 )
         printLevel = 5;
      if ( strcmp(printlevelstr, "final") == 0 )
         printLevel = 3;

      /* Check for writing problem */
      getStrOption(OPTS, "probfile", probfile);

      /* Check for writing presolved problem */
      getStrOption(OPTS, "presolvedfile", presolvedfile);

      CheckOptiVersion(OPTS);

      /* set common options */
      if ( ! SCIPisInfinity(scip, maxtime) )
      {
         SCIP_ERR( SCIPsetRealParam(scip, "limits/time", maxtime), "Error setting maxtime.");
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
   f = mxGetPr(prhs[eF]);
   A = mxGetPr(prhs[eA]);
   A_ir = mxGetIr(prhs[eA]);
   A_jc = mxGetJc(prhs[eA]);
   lhs = mxGetPr(prhs[eLHS]);
   rhs = mxGetPr(prhs[eRHS]);
   lb = mxGetPr(prhs[eLB]);
   ub = mxGetPr(prhs[eUB]);

   if ( nrhs > eSDP && ! mxIsEmpty(prhs[eSDP]) )
   {
      if ( mxIsCell(prhs[eSDP]) )
         ncones = mxGetNumberOfElements(prhs[eSDP]);
      else
         ncones = 1;
   }

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
   plhs[3] = mxCreateStructMatrix(1, 1, 4, fnames);
   mxSetField(plhs[3], 0, fnames[0], mxCreateDoubleMatrix(1, 1, mxREAL));
   mxSetField(plhs[3], 0, fnames[1], mxCreateDoubleMatrix(1, 1, mxREAL));
   mxSetField(plhs[3], 0, fnames[2], mxCreateDoubleMatrix(1, 1, mxREAL));
   mxSetField(plhs[3], 0, fnames[3], mxCreateDoubleMatrix(1, 1, mxREAL));

   nodes = mxGetPr(mxGetField(plhs[3], 0, fnames[0]));
   gap   = mxGetPr(mxGetField(plhs[3], 0, fnames[1]));
   pbound = mxGetPr(mxGetField(plhs[3], 0, fnames[2]));
   dbound = mxGetPr(mxGetField(plhs[3], 0, fnames[3]));

   /* create empty problem */
   SCIP_ERR( SCIPcreateProbBasic(scip, "OPTI Problem"), "Error creating basic SCIP-SCP problem");

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
   if ( objbias != 0 )
   {
      SCIP_ERR( SCIPcreateVarBasic(scip, &objb, "objbiasterm", objbias, objbias, 1.0, SCIP_VARTYPE_CONTINUOUS), "Error adding objective bias variable.");
      SCIP_ERR( SCIPaddVar(scip, objb), "Error adding objective bias variable.");
   }

   /* add linear constraints (if they exist) */
   if ( ncon )
   {
      /* allocate memory for all constraints (we create them all now, as we have to add coefficients in column order) */
      SCIP_ERR( SCIPallocMemoryArray(scip, &cons, (int)ncon), "Error allocating constraint memory.");

      /* create each constraint and add row bounds, but leave coefficients empty */
      for (i = 0; i < ncon; i++)
      {
         SCIPsnprintf(msgbuf, BUFSIZE, "lincon%d", i); /* appears constraints require a name */
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

   /* add semidefinite constraints */
   for (i = 0; i < ncones; i++)
   {
      if ( ncones == 1 && ! mxIsCell(prhs[eSDP]) )
         addSDPConstraint(scip,vars,prhs[eSDP],(int)i);
      else
         addSDPConstraint(scip,vars,mxGetCell(prhs[eSDP],i),(int)i);
   }

   /* SCIP_ERR( SCIPwriteOrigProblem(scip, NULL, "cip", FALSE), "error"); */

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
      /* process specific options (overriding emphasis options) */
      if ( mxGetField(OPTS, 0, "solverOpts") )
         processUserOpts(scip, mxGetField(OPTS, 0, "solverOpts"));
   }

   /* possibly write file */
   if ( strlen(probfile) > 0 )
   {
      SCIP_ERR( SCIPwriteOrigProblem(scip, probfile, NULL, FALSE), "Error writing file.");
   }

   /* possibly write presolved file */
   if ( strlen(presolvedfile) > 0 )
   {
      /* presolve first */
      SCIP_ERR( SCIPpresolve(scip), "Error presolving SCIP problem!");

      /* now write */
      SCIP_ERR( SCIPwriteTransProblem(scip, presolvedfile, NULL, FALSE), "Error writing presolved file.");
   }

   /* solve problem */
   SCIP_RETCODE rc = SCIPsolve(scip);

   if ( rc != SCIP_OKAY )
   {
      /* clean up general SCIP memory (if possible) */
      SCIPfree(&scip);

      /* display error */
      sprintf(msgbuf, "Error Solving SCIP-SDP Problem, Error: %s (Code: %d)", scipErrCode(rc), rc);
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
      *dbound = std::numeric_limits<double>::quiet_NaN();
   }

   /* get solution status */
   *exitflag = (double)SCIPgetStatus(scip);

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

   /* release variables */
   for (i = 0; i < ndec; i++)
      SCIP_ERR( SCIPreleaseVar(scip, &vars[i]), "Error releasing SCIP-SDP variable.");

   if ( objb != NULL )
      SCIP_ERR( SCIPreleaseVar(scip, &objb), "Error releasing SCIP-SDP objective bias variable.");

   /* now free SCIP arrays & problem */
   SCIPfreeMemoryArray(scip, &vars);

   if ( ncon )
      SCIPfreeMemoryArray(scip, &cons);

   /* clean up general SCIP memory */
   SCIP_ERR( SCIPfree(&scip), "Error releasing SCIP-SDP problem.");
}
