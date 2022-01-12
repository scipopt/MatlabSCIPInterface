/* SCIPMEX - A MATLAB MEX Interface to SCIP
 * Released Under the BSD 3-Clause License.
 *
 * Based on code by Jonathan Currie, which was based in parts on matscip.c supplied with SCIP.
 *
 * Authors:
 * - Jonathan Currie
 * - Marc Pfetsch
 * - Stefan Vigerske
 */

/* If some symbols are undefined, consider undefining the following:
 * #define SCIP_EXPORT __attribute__((visibility("default")))
 */

#include "scip/def.h"
#include "scip/scip.h"
#include "scip/scipdefplugins.h"
#include "mex.h"
#include "scipnlmex.h"

/* enable for debugging: */
/* #define DEBUG 1 */

/* function buffer size [maximum expressions & variables to hold for post-processing] */
#define MAX_DEPTH 512

/* message buffer size */
#define BUFSIZE 2048

/* global message buffer */
static char msgbuf[BUFSIZE];

/* error catching macro */
#define SCIP_ERR(rc,msg) if ( rc != SCIP_OKAY ) { snprintf(msgbuf, BUFSIZE, "%s, Error Code: %d", msg, rc); mexErrMsgTxt(msgbuf);}


/** print state for debugging */
static
void debugPrintState(
   int                   state,
   int*                  args,
   int                   savexp,
   int                   savvar,
   int                   savpro,
   int                   varcnt,
   double                num
   )
{
   const char* strs[18] = {"EXIT","EMPTY","READ","NUM","VAR","EXPRSN","MUL","DIV","ADD","SUB","SQUARE","SQRT","POWER","EXPNT","LOG","SIN","COS","TAN"};

   if ( state == 99 )
      state = -3;
   mexPrintf("State: %-8s ARG 0: %-8s ARG 1: %-8s PSAVEXP: %3d PSAVVAR: %3d PSAVPRO: %3d VARCNT: %3d  num: %g\n",
      strs[state+3], strs[args[0]+3], strs[args[1]+3], savexp, savvar, savpro, varcnt, num);
}


/* Enumerations */
enum {EMPTY=-2,READ,NUM,VAR,EXP,MUL,DIV,ADD,SUB,SQUARE,SQRT,POW,EXPNT,LOG,SIN,COS,TAN,MIN,MAX,ABS,SIGN};
const int OTHER = 98, EXIT = 99;


#if ( SCIP_VERSION >= 800 || ( SCIP_VERSION < 800 && SCIP_APIVERSION >= 100 ) )

/** add nonlinear constraint to problem */
double addNonlinearCon(
   SCIP*                 scip,               /**< SCIP instance */
   SCIP_VAR**            vars,               /**< variable array */
   double*               instr,              /**< array of instructions */
   size_t                no_instr,           /**< number of instructions */
   double                lhs,                /**< left hand side */
   double                rhs,                /**< right hand side */
   double*               xval,               /**< validation point for variables */
   size_t                nlno,               /**< index of nonlinear constraint */
   bool                  isObj               /**< is this the objective function */
   )
{
   /* internal variables */
   SCIP_EXPR** expvars = NULL;     /* expression array to represent variables */
   SCIP_EXPR** exp = NULL;         /* expression array, to incrementally build up full expression */
   SCIP_CONS* nlcon;               /* resulting constraint */
   SCIP_VAR* nlobj;                /* variable representing nonlinear objective (if used) */
   int state = READ;               /* state machine sate */
   int index = 0;                  /* variable index (picked up from instruction) */
   int* varind;                    /* array of all variable indicies */
   int* unqind;                    /* array of unique indicies */
   int varcnt = -1;                /* keeps track of which expression variable we're up to */
   int expno = 0;                  /* index within expression array we are up to */
   size_t no_var = 0;              /* number of variables declared in instruction list */
   size_t no_ops = 0;              /* number of operations declared in instruction list */
   size_t no_unq = 0;              /* number of unique indicies found */
   int args[2] = {EMPTY,EMPTY};    /* used to keep track of previous arguments */
   int savexp = 0;                 /* used to keep track of last expression before entering exp (op) expr */
   int savexplst[MAX_DEPTH];       /* list of saved expressions to process later */
   int psavexplst = -1;            /* position in list */
   int savvarlst[MAX_DEPTH];       /* list of saved variables to process later (may not be needed...?) */
   int psavvarlst = -1;            /* position in list */
   int vari = varcnt;              /* varible index when evaluating functions */
   int savprolst[2 * MAX_DEPTH];   /* list of exp or var to process */
   int psavprolst = -1;            /* position in list */
   double num = 0;                 /* to save constants passed */
   double fval = 0;                /* evaluation value */
   double one = 1.0;
   double mone = -1.0;
   double zero = 0.0;
   int nvars;
   size_t i;
   size_t j;

   nvars = SCIPgetNVars(scip);

   /* initialize lists */
   for (i = 0; i < MAX_DEPTH; i++)
   {
      savexplst[i] = EMPTY;
      savvarlst[i] = EMPTY;
   }

   for (i = 0; i < 2 * MAX_DEPTH; i++)
      savprolst[i] = EMPTY;

   /* determine number of variables and operations
    * (note we have to have a new variable 'expression' every time a variable appears, even if its repeated) */
   state = READ;
   for (i = 0; i < no_instr; i++)
   {
      switch ( state )
      {
      case READ:
         if ( instr[i] > EXP )
            no_ops++;
         if ( instr[i] == VAR )
            no_var++;
         state = OTHER;
         break;

      case OTHER: /* i.e. arg / misc */
         state = READ;
         break;
      }
   }

   /* create variable index vector */
   varind = (int*)mxCalloc(no_var, sizeof(int));

   /* fill with indicies from instruction list */
   j = 0;
   state = READ;
   for (i = 0; i < no_instr; i++)
   {
      switch ( state )
      {
      case READ:
         if ( instr[i] == VAR )
            state = VAR;
         else
            state = OTHER;
         break;

      case VAR:
         varind[j++] = (int)instr[i];
         state = READ;
         break;

      case OTHER:
         state = READ;
         break;
      }
   }

#ifdef DEBUG
   mexPrintf("\n---------------------------------------\nProcessing Nonlinear Expression\n---------------------------------------\n");
   mexPrintf("novar: %d, nounq: %d; no_ops: %d\n", no_var, no_unq, no_ops);

   /* print what we have found so far */
   for (i = 0; i < no_var; i++)
      mexPrintf("varind[%d] = %d\n", i, varind[i]);
   mexPrintf("\n");
#endif

   /* create expression variable memory */
   SCIP_ERR( SCIPallocMemoryArray(scip, &expvars, no_var), "Error allocating expression variable memory.");

   /* create expression memory */
   SCIP_ERR( SCIPallocMemoryArray(scip, &exp, MAX(no_ops,1) ), "Error allocating expression memory.");

   /* create expression for each variable */
   for (i = 0; i < no_var; i++)
   {
      assert( 0 <= varind[i] && varind[i] < nvars );
      SCIP_ERR( SCIPcreateExprVar(scip, &expvars[i], vars[varind[i]], NULL, NULL) , "Error creating variable expression.");
   }

   /* if objective, create an unbounded variable to add to objective, representing the nonlinear part */
   if ( isObj )
   {
      SCIP_ERR( SCIPcreateVarBasic(scip, &nlobj, "nlobj", -SCIPinfinity(scip), SCIPinfinity(scip), 1.0, SCIP_VARTYPE_CONTINUOUS), "Error adding nonlinear objective variable.");
      SCIP_ERR( SCIPaddVar(scip, nlobj), "Error adding nonlinear objective variable.");
   }

   /* check for constant number objective */
   if ( no_instr == 2 && instr[0] == NUM )
   {
      SCIP_ERR( SCIPcreateExprValue(scip, &exp[0], instr[1], NULL, NULL), "Error creating constant objective / constraint expression.");
      no_instr = 0; /* skip below */
#ifdef DEBUG
      mexPrintf("Found constant objective / constraint, skipping expression tree building.\n");
#endif
   }

   /* check for single variable objective / constraint */
   if ( no_instr == 2 && instr[0] == VAR )
   {
      SCIP_ERR( SCIPcreateExprSum(scip, &exp[0], 1, &expvars[0], &one, 0.0, NULL, NULL), "Error creating linear expression of a single variable.");
      no_instr = 0; /* skip below */
#ifdef DEBUG
      mexPrintf("Found single variable objective / constraint, skipping expression tree building.\n");
#endif
   }

   /* begin processing instruction list */
   state = READ;
   for (i = 0; i < no_instr; i++)
   {
#ifdef DEBUG
      debugPrintState(state, args, psavexplst, psavvarlst, psavprolst, varcnt, num);
#endif

      switch(state)
      {
      case READ:  /* read instruction */
         state = (int)instr[i];
         break;

      case NUM:   /* number passed, collect value */
         num = instr[i];
         state = READ;
         if ( args[0] == EMPTY ) /* all empty */
            args[0] = NUM;
         else if ( args[1] == EMPTY )  /* tag onto end */
            args[1] = NUM;
         else if ( args[0] == EXP )
         {  /* all full, save expression for use later */
            if ( psavexplst >= MAX_DEPTH )
               mexErrMsgTxt("Maximum function depth exceeded [expression list].");

            /* Save expression number in list */
            savexplst[++psavexplst] = expno-1;

            /* Update process list */
            savprolst[++psavprolst] = EXP;

            /* Correct args */
            args[0] = args[1];
            args[1] = NUM;
         }
         else if ( args[0] == VAR )
         {  /* all full, save variable for use later */
            if ( psavvarlst >= MAX_DEPTH )
               mexErrMsgTxt("Maximum function depth exceeded [variable list].");

            /* save variable number in list */
            savvarlst[++psavvarlst] = varcnt-1;

            /* update process list */
            savprolst[++psavprolst] = VAR;

            /* correct args */
            args[0] = args[1];
            args[1] = NUM;
         }
         else
            mexErrMsgTxt("Error in order of instructions.");
         break;

      case VAR:   /* variable index passed, increment our variable counter */
         varcnt++;
         state = READ;
         if ( args[0] == EMPTY ) /* all empty */
            args[0] = VAR;
         else if ( args[1] == EMPTY ) /* tag onto end */
            args[1] = VAR;
         else if ( args[0] == EXP )
         {
            /* all full */
            if ( psavexplst >= MAX_DEPTH )
               mexErrMsgTxt("Maximum function depth exceeded [expression list].");

            /* save expression number in list */
            savexplst[++psavexplst] = expno-1;

            /* update process list */
            savprolst[++psavprolst] = EXP;
            args[0] = args[1];
            args[1] = VAR;
         }
         else if ( args[0] == VAR )
         {  /* all full but with variables e.g x2 - x1^2 */
            if ( psavvarlst >= MAX_DEPTH)
               mexErrMsgTxt("Maximum function depth exceeded [variable list].");

            /* save variable number in list */
            if ( instr[i-3] == VAR && instr[i-5] == VAR ) /* check last two instructions left hand col (this will be at least the third instr) */
               savvarlst[++psavvarlst] = varcnt - 2;
            else
               savvarlst[++psavvarlst] = varcnt - 1;

            /* Update process list */
            savprolst[++psavprolst] = VAR;

            /* Correct args */
            args[0] = args[1];
            args[1] = VAR;
         }
         else
            mexErrMsgTxt("Error in order of instructions.");
         break;

         /* All two operand operators */
      case MUL:
      case DIV:
      case ADD:
      case SUB:
      case POW:

         /* read variable index */
         vari = varcnt;

         /* Check for waiting variable or expression to process */
         if ( args[1] == EMPTY && psavprolst >= 0 )
         {
            /* read if the last record is a var or exp, then check if we actually have one to process */
            int toProcess = savprolst[psavprolst--];
            if ( toProcess == VAR )
            {
               if ( psavvarlst < 0) /* none to process, error */
                  mexErrMsgTxt("Error reading variable to process - Process List indicates variable to process, but not present in variable list.");

               /* collect variable number to process */
               vari = savvarlst[psavvarlst];
               if ( vari == EMPTY || vari < 0 )
                  mexErrMsgTxt("Error processing waiting variable, found empty or negative index.");
#ifdef DEBUG
               mexPrintf("-----\nProcessing Waiting Variable: %d [Index %d, %d remaining]\n-----\n", vari, varind[vari], psavvarlst);
#endif
               /* correct args */
               args[1] = VAR;
               instr[i] = 1;  /* will flip if subtraction or division */
               psavvarlst--;
            }
            else if ( toProcess == EXP )
            {
               if ( psavexplst < 0 ) /* none to process, error */
                  mexErrMsgTxt("Error reading expression to process - Process List indicates expression to process, but not present in expression list.");
#ifdef DEBUG
               mexPrintf("-----\nProcessing Waiting Expression: %d [%d remaining]\n-----\n", savexplst[psavexplst], psavexplst);
#endif
               args[1] = EXP;
            }
            else
               mexErrMsgTxt("Unknown entry in process list.");
         }

         /* check we have two arguments */
         if ( args[0] == EMPTY || args[1] == EMPTY )
            mexErrMsgTxt("Error attempting to create nonlinear expression, operator doesn't have two operands.");

         /* NUM (op) VAR (linear except division) */
         if ( args[0] == NUM && args[1] == VAR )
         {
            switch ( state )
            {
            case ADD:
               SCIP_ERR( SCIPcreateExprSum(scip, &exp[expno], 1, &expvars[vari], &one, num, NULL, NULL), "Error creating linear add expression (num + var).");
               break;

            case SUB:
               SCIP_ERR( SCIPcreateExprSum(scip, &exp[expno], 1, &expvars[vari], &mone, num, NULL, NULL), "Error creating linear subtract expression (num - var).");
               break;

            case MUL:
               SCIP_ERR( SCIPcreateExprSum(scip, &exp[expno], 1, &expvars[vari], &num, 0.0, NULL, NULL), "Error creating linear multiply expression (num * var).");
               break;

            case DIV:
               SCIP_EXPR* divexp; /* intermediate expression */

               SCIP_ERR( SCIPcreateExprPow(scip, &divexp, expvars[vari], -1.0, NULL, NULL), "Error creating division expression." );
               SCIP_ERR( SCIPcreateExprSum(scip, &exp[expno], 1, &divexp, &num, 0.0, NULL, NULL), "Error creating linear multiply expression (num * var^-1).");

               SCIP_ERR( SCIPreleaseExpr(scip, &divexp), "Error releasing expression.");
               break;

            case POW:
               mexErrMsgTxt("You cannot use POWER with the exponent as a variable. For x^y use exp(y*log(x)).");

            default:
               mexErrMsgTxt("Operator not implemented yet for NUM (op) VAR!");
            }
            args[0] = EXP;
            args[1] = EMPTY;
         }
         /* VAR (op) NUM (linear except pow) */
         else if ( args[0] == VAR && args[1] == NUM )
         {
            switch ( state )
            {
            case ADD:
               SCIP_ERR( SCIPcreateExprSum(scip, &exp[expno], 1, &expvars[vari], &one, num, NULL, NULL), "Error creating linear add expression (var + num).");
               break;

            case SUB:
               SCIP_ERR( SCIPcreateExprSum(scip, &exp[expno], 1, &expvars[vari], &one, -num, NULL, NULL), "Error creating linear subtract expression (var - num).");
               break;

            case MUL:
               SCIP_ERR( SCIPcreateExprSum(scip, &exp[expno], 1, &expvars[vari], &num, 0.0, NULL, NULL), "Error creating linear multiply expression (var * num).");
               break;

            case DIV:
            {
               if ( num == 0.0 )
                  mexErrMsgTxt("Division by constant 0.");
               SCIP_Real coef = 1.0 / num;
               SCIP_ERR( SCIPcreateExprSum(scip, &exp[expno], 1, &expvars[vari], &coef, 0.0, NULL, NULL), "Error creating linear multiply expression (var * 1/num).");
               break;
            }
            case POW:
               SCIP_ERR( SCIPcreateExprPow(scip, &exp[expno], expvars[vari], num, NULL, NULL), "Error creating power expression." );
               break;

            default:
               mexErrMsgTxt("Operator not implemented yet for VAR (op) NUM!");
            }
            args[0] = EXP;
            args[1] = EMPTY;
         }
         /* VAR (op) VAR */
         else if ( args[0] == VAR && args[1] == VAR )
         {
            SCIP_Real coefs[2];
            switch ( state )
            {
            case ADD:
               coefs[0] = 1.0;
               coefs[1] = 1.0;
               SCIP_ERR( SCIPcreateExprSum(scip, &exp[expno], 2, &expvars[vari-1], coefs, 0.0, NULL, NULL), "Error creating  add expression (var + var).");
               break;

            case SUB:
               coefs[0] = 1.0;
               coefs[1] = -1.0;
               SCIP_ERR( SCIPcreateExprSum(scip, &exp[expno], 2, &expvars[vari-1], coefs, 0.0, NULL, NULL), "Error creating sub expression (var - var).");
               break;

            case MUL:
               SCIP_ERR( SCIPcreateExprProduct(scip, &exp[expno], 2, &expvars[vari-1], 1.0, NULL, NULL), "Error creating mul expression (var * var).");
               break;

            case DIV:
               SCIP_EXPR* divexp[2]; /* intermediate expressions */

               divexp[0] = expvars[vari-1];
               SCIP_ERR( SCIPcreateExprPow(scip, &divexp[1], expvars[vari], -1.0, NULL, NULL), "Error creating division intermediate expression.");
               SCIP_ERR( SCIPcreateExprProduct(scip, &exp[expno], 2, divexp, 1.0, NULL, NULL), "Error creating divide expression (var * 1/var).");
               SCIP_ERR( SCIPreleaseExpr(scip, &divexp[1]), "Error releasing expression.");
               break;

            case POW:
               mexErrMsgTxt("You cannot use POWER with the exponent as a variable. For x^y use exp(y*log(x)).");

            default:
               mexErrMsgTxt("Operator not implemented yet for VAR (op) VAR!");
            }
            args[0] = EXP;
            args[1] = EMPTY;
         }
         /* EXP (op) NUM */
         else if ( args[0] == EXP && args[1] == NUM )
         {
            switch ( state )
            {
            case ADD:
               SCIP_ERR( SCIPcreateExprSum(scip, &exp[expno], 1, &exp[expno-1], &one, num, NULL, NULL), "Error creating add expression (exp + num).");
               break;

            case SUB:
               /* check for flipped args */
               if ( instr[i] == 1 )
               {
                  SCIP_ERR( SCIPcreateExprSum(scip, &exp[expno], 1, &exp[expno-1], &mone, num, NULL, NULL), "Error creating sub expression (num - exp).");
               }
               else
               {
                  SCIP_ERR( SCIPcreateExprSum(scip, &exp[expno], 1, &exp[expno-1], &one, -num, NULL, NULL), "Error creating sub expression (exp - num).");
               }
               break;

            case MUL:
               SCIP_ERR( SCIPcreateExprSum(scip, &exp[expno], 1, &exp[expno-1], &num, 0.0, NULL, NULL), "Error creating linear multiply expression (exp * num).");
               break;

            case DIV:
               /* check for flipped args */
               if ( instr[i] == 1 )
               {  /* flip */
                  SCIP_EXPR* divexp; /* intermediate expression */
                  SCIP_ERR( SCIPcreateExprPow(scip, &divexp, exp[expno-1], -1.0, NULL, NULL), "Error creating division intermediate expression.");
                  SCIP_ERR( SCIPcreateExprSum(scip, &exp[expno], 1, &divexp, &num, 0.0, NULL, NULL), "Error creating linear multiply expression (num / exp).");
                  SCIP_ERR( SCIPreleaseExpr(scip, &divexp), "Error releasing expression.");
               }
               else
               {
                  if ( num == 0.0 )
                     mexErrMsgTxt("Division by constant 0.");
                  SCIP_Real coef = 1.0 / num;
                  SCIP_ERR( SCIPcreateExprSum(scip, &exp[expno], 1, &exp[expno-1], &coef, 0.0, NULL, NULL), "Error creating linear multiply expression (exp / num).");
               }
               break;

            case POW:
               SCIP_ERR( SCIPcreateExprPow(scip, &exp[expno], exp[expno-1], num, NULL, NULL), "Error creating power expression.");
               break;

            default:
               mexErrMsgTxt("Operator not implemented yet for NUM (op) VAR!");
            }
            args[0] = EXP;
            args[1] = EMPTY;
         }
         /* EXP (op) VAR */
         else if ( args[0] == EXP && args[1] == VAR )
         {
            SCIP_EXPR* termexp[2];
            SCIP_Real termcoef[2];

            termexp[0] = exp[expno-1];
            termexp[1] = expvars[vari];

            switch(state)
            {
            case ADD:
               termcoef[0] = 1.0;
               termcoef[1] = 1.0;
               SCIP_ERR( SCIPcreateExprSum(scip, &exp[expno], 2, termexp, termcoef, 0.0, NULL, NULL), "Error creating add expression (exp + var).");
               break;

            case SUB:
               /* check for flipped args */
               if ( instr[i] == 1 )
               {  /* flip */
                  termcoef[0] = -1.0;
                  termcoef[1] = 1.0;
               }
               else
               {
                  termcoef[0] = 1.0;
                  termcoef[1] = -1.0;
               }
               SCIP_ERR( SCIPcreateExprSum(scip, &exp[expno], 2, termexp, termcoef, 0.0, NULL, NULL), "Error creating subtract expression (exp - var or var - exp).");
               break;

            case MUL:
               SCIP_ERR( SCIPcreateExprProduct(scip, &exp[expno], 2, termexp, 1.0, NULL, NULL),  "Error creating mul expression (exp * var).");
               break;

            case DIV:
               /* check for flipped args */
               if ( instr[i] == 1 )
               {
                  SCIP_ERR( SCIPcreateExprPow(scip, &termexp[0], exp[expno-1], -1.0, NULL, NULL), "Error creating division intermediate expression.");
                  termexp[1] = expvars[vari];
               }
               else
               {
                  SCIP_ERR( SCIPcreateExprPow(scip, &termexp[0], expvars[vari], -1.0, NULL, NULL), "Error creating division intermediate expression.");
                  termexp[1] = exp[expno-1];
               }
               SCIP_ERR( SCIPcreateExprProduct(scip, &exp[expno], 2, termexp, 1.0, NULL, NULL), "Error creating linear multiply expression (var / exp or exp / var).");
               SCIP_ERR( SCIPreleaseExpr(scip, &termexp[0]), "Error releasing expression.");
               break;

            case POW:
               mexErrMsgTxt("You cannot use POWER with the exponent as a variable. For x^y use exp(y*log(x)).");

            default:
               mexErrMsgTxt("Operator not implemented yet for EXP (op) VAR!");
            }
            args[0] = EXP;
            args[1] = EMPTY;
         }
         /* EXP (op) EXP */
         else if ( args[0] == EXP && args[1] == EXP )
         {
            SCIP_EXPR* termexp[2];
            SCIP_Real termcoef[2];

            /* collect expression number to process */
            savexp = savexplst[psavexplst];
            if ( savexp == EMPTY || savexp < 0 )
               mexErrMsgTxt("Error processing waiting expression, found empty index.");

            termexp[0] = exp[savexp];
            termexp[1] = exp[expno-1];

            switch(state)
            {
            case ADD:
               termcoef[0] = 1.0;
               termcoef[1] = 1.0;
               SCIP_ERR( SCIPcreateExprSum(scip, &exp[expno], 2, termexp, termcoef, 0.0, NULL, NULL), "Error creating add expression (exp + exp).");
               break;

            case SUB:
               /* check for flipped args */
               if ( instr[i] == 1 )
               {  /* flip */
                  termcoef[0] = -1.0;
                  termcoef[1] = 1.0;
               }
               else
               {
                  termcoef[0] = 1.0;
                  termcoef[1] = -1.0;
               }
               SCIP_ERR( SCIPcreateExprSum(scip, &exp[expno], 2, termexp, termcoef, 0.0, NULL, NULL), "Error creating subtract expression (exp - exp or exp - exp).");
               break;

            case MUL:
               SCIP_ERR( SCIPcreateExprProduct(scip, &exp[expno], 2, termexp, 1.0, NULL, NULL),  "Error creating add expression (exp + exp).");
               break;

            case DIV:
               /* check for flipped args */
               if ( instr[i] == 1 )
               {
                  SCIP_ERR( SCIPcreateExprPow(scip, &termexp[0], exp[savexp], -1.0, NULL, NULL), "Error creating division intermediate expression.");
                  termexp[1] = exp[expno-1];
               }
               else
               {
                  SCIP_ERR( SCIPcreateExprPow(scip, &termexp[0], exp[expno-1], -1.0, NULL, NULL), "Error creating division intermediate expression.");
                  termexp[1] = exp[savexp];
               }
               SCIP_ERR( SCIPcreateExprProduct(scip, &exp[expno], 2, termexp, 1.0, NULL, NULL), "Error creating div expression (exp_old / exp_new or exp_new / exp_old).");
               SCIP_ERR( SCIPreleaseExpr(scip, &termexp[0]), "Error releasing expression.");
               break;

            default:
               mexErrMsgTxt("Unexpected operator for combining expressions, currently only +, -, * and / supported");
            }
            args[0] = EXP;
            args[1] = EMPTY;
            psavexplst--;
         }
         else
            mexErrMsgTxt("Grouping of arguments not implemented yet.");

         if ( i == (no_instr-1) )
            state = EXIT;
         else
         {
            state = READ;
            expno++; /* increment for next operator */
         }
         break;

         /* all single operand functions */
      case SQUARE:
      case SQRT:
      case EXPNT:
      case LOG:
      case ABS:
      case SIN:
      case COS:

         /* read variable index */
         vari = varcnt;

         /* check we have one argument */
         if ( args[0] == EMPTY )
            mexErrMsgTxt("Error attempting to create nonlinear expression, function doesn't have an operand.");

         /* check for unprocessed exp [e.g. (3-x(2)).*log(x(1))] */
         if ( args[0] == EXP && args[1] == VAR )
         {
            /* save expression to process later */
            if ( psavexplst >= MAX_DEPTH )
               mexErrMsgTxt("Maximum function depth exceeded [expression list].");

            /* save expression number in list */
            savexplst[++psavexplst] = expno-1;

            /* Update process list */
            savprolst[++psavprolst] = EXP;

            /* correct args */
            args[0] = VAR;
            args[1] = EMPTY;
         }

         /* check for unprocessed var [e.g. x2 - exp(x1)] */
         if ( args[0] == VAR && args[1] == VAR )
         {
            /* save variable to process later */
            if ( psavvarlst >= MAX_DEPTH )
               mexErrMsgTxt("Maximum function depth exceeded [variable list].");

            /* save variable number in list */
            savvarlst[++psavvarlst] = varcnt-1;

            /* update process list */
            savprolst[++psavprolst] = VAR;

            /* continue processing function, we will process this variable next free round */
         }

         /* FCN ( VAR ) enum {EMPTY=-2,READ,NUM,VAR,EXP,MUL,DIV,ADD,SUB,SQUARE,SQRT,POW,EXPNT,LOG,SIN,COS,TAN,MIN,MAX,ABS,SIGN}; */
         if ( args[0] == VAR )
         {
            /* check wheter variable index realistic */
            if ( vari < 0 )
               mexErrMsgTxt("Variable index is negative when creating fcn(var).");

            switch(state)
            {
            case SQRT:
               SCIP_ERR( SCIPcreateExprPow(scip, &exp[expno], expvars[vari], 0.5, NULL, NULL), "Error creating sqrt expression sqrt(var).");
               break;

            case EXPNT:
               SCIP_ERR( SCIPcreateExprExp(scip, &exp[expno], expvars[vari], NULL, NULL), "Error creating exponential expression exp(var).");
               break;

            case LOG:
               SCIP_ERR( SCIPcreateExprLog(scip, &exp[expno], expvars[vari], NULL, NULL), "Error creating logarithm expression log(var).");
               break;

            case ABS:
               SCIP_ERR( SCIPcreateExprAbs(scip, &exp[expno], expvars[vari], NULL, NULL), "Error creating absolute-value expression abs(var).");
               break;

            case SIN:
               SCIP_ERR( SCIPcreateExprSin(scip, &exp[expno], expvars[vari], NULL, NULL), "Error creating sinus expression sin(var).");
               break;

            case COS:
               SCIP_ERR( SCIPcreateExprCos(scip, &exp[expno], expvars[vari], NULL, NULL), "Error creating cosinus expression cos(var).");
                break;

            default:
               mexErrMsgTxt("Operator not implemented yet for FCN ( VAR )!");
            }
            args[0] = EXP;
            args[1] = EMPTY;
         }
         /* FCN ( EXP ) */
         else if ( args[0] == EXP )
         {
            switch ( state )
            {
            case SQRT:
               SCIP_ERR( SCIPcreateExprPow(scip, &exp[expno], exp[expno-1], 0.5, NULL, NULL), "Error creating sqrt expression sqrt(exp).");
               break;

            case EXPNT:
               SCIP_ERR( SCIPcreateExprExp(scip, &exp[expno], exp[expno-1], NULL, NULL), "Error creating exponential expression exp(exp).");
               break;

            case LOG:
               SCIP_ERR( SCIPcreateExprLog(scip, &exp[expno], exp[expno-1], NULL, NULL), "Error creating logarithm expression log(exp).");
               break;

            case ABS:
               SCIP_ERR( SCIPcreateExprAbs(scip, &exp[expno], exp[expno-1], NULL, NULL), "Error creating absolute-value expression abs(exp).");
               break;

            case SIN:
               SCIP_ERR( SCIPcreateExprSin(scip, &exp[expno], exp[expno-1], NULL, NULL), "Error creating sinus expression sin(exp).");
               break;

            case COS:
               SCIP_ERR( SCIPcreateExprCos(scip, &exp[expno], exp[expno-1], NULL, NULL), "Error creating cosinus expression cos(exp).");
               break;

            default:
               mexErrMsgTxt("Operator not implemented yet for FCN ( EXP )!");
            }
            args[0] = EXP;
            args[1] = EMPTY;
         }
         else
            mexErrMsgTxt("Unknown argument to operate function on, only options are VAR (variable) or EXP (expression).");

         if ( i == (no_instr-1) )
            state = EXIT;
         else
         {
            state = READ;
            expno++; /* increment for next operator */
         }
         break;

      case MIN:
      case MAX:
         mexErrMsgTxt("Max and Min not currently implemented (in this interface and SCIP).");
         break;
      case TAN:
         mexErrMsgTxt("Tangent function not currently implemented (in SCIP).");
         break;
      case SIGN:
         mexErrMsgTxt("Sign not currently implemented (in SCIP).");
         break;

      case EXIT:
         break; /* exit switch case */

      default:
         mexErrMsgTxt("Unknown (or out of order) instruction.");
      }
   }
#ifdef DEBUG
   debugPrintState(state, args, psavexplst, psavvarlst, psavprolst, varcnt, num);
   mexPrintf("\n---------------------------------------\nSummary at Expression Tree Create:\n");
   mexPrintf("expno:  %3d [should equal %3d]\nvarcnt: %3d [should equal %3d]\n", expno, no_ops-1, varcnt, no_var-1);
   mexPrintf("---------------------------------------\n");
#endif

   /* 'Validate' the constraint if initial guess supplied */
   if ( xval != NULL )
   {
      SCIP_SOL* sol;
      SCIP_ERR( SCIPcreateSol(scip, &sol, NULL), "Error creating solution.\n");
      SCIP_ERR( SCIPsetSolVals(scip, sol, nvars, vars, xval), "Error setting solution values.\n");

      SCIP_ERR( SCIPevalExpr(scip, exp[expno], sol, 0), "Error evaluating expression.\n");
      fval = SCIPexprGetEvalValue(exp[expno]);
      SCIP_ERR( SCIPfreeSol(scip, &sol), "Error freeing solution.\n");
   }

   /* create the nonlinear constraint, add it, then release it */
   if ( isObj )
      (void) SCIPsnprintf(msgbuf, BUFSIZE, "NonlinearObj%zd", nlno);
   else
      (void) SCIPsnprintf(msgbuf, BUFSIZE, "NonlinearExp%zd", nlno);

   SCIP_ERR( SCIPcreateConsBasicNonlinear(scip, &nlcon, msgbuf, exp[expno], lhs, rhs), "Error creating nonlinear constraint!");

   /* if nonlinear objective, add to objective */
   if ( isObj )
   {
      SCIP_ERR( SCIPaddLinearVarNonlinear(scip, nlcon, nlobj, -1.0), "Error adding nonlinear objective linear term.");
      SCIP_ERR( SCIPreleaseVar(scip, &nlobj), "Error releasing SCIP nonlinear objective variable.");
   }

   /* continue to free the constraint */
   SCIP_ERR( SCIPaddCons(scip, nlcon), "Error adding nonlinear constraint.");
   SCIP_ERR( SCIPreleaseCons(scip, &nlcon), "Error freeing nonlinear constraint.");

   /* memory clean up */
   for (int k = expno; k >= 0; --k)
   {
      SCIP_ERR( SCIPreleaseExpr(scip, &exp[k]), "Error releasing expression.");
   }

   for (int k = no_var - 1; k >= 0; --k)
   {
      SCIP_ERR( SCIPreleaseExpr(scip, &expvars[k]), "Error releasing variable expression.");
   }

   SCIPfreeMemoryArray(scip, &exp);
   SCIPfreeMemoryArray(scip, &expvars);
   mxFree(varind);

   return fval;
}

#else /* consexpr */

/** add nonlinear constraint to problem */
double addNonlinearCon(
   SCIP*                 scip,               /**< SCIP instance */
   SCIP_VAR**            vars,               /**< variable array */
   double*               instr,              /**< array of instructions */
   size_t                no_instr,           /**< number of instructions */
   double                lhs,                /**< left hand side */
   double                rhs,                /**< right hand side */
   double*               xval,               /**< validation solution for variables */
   size_t                nlno,               /**< index of nonlinear constraint */
   bool                  isObj               /**< is this the objective function */
   )
{
   /* internal variables */
   SCIP_EXPR** expvars = NULL;     /* expression array to represent variables */
   SCIP_EXPR** exp = NULL;         /* expression array, to incrementally build up full expression */
   SCIP_EXPRTREE *exprtree = NULL; /* resulting expression tree */
   SCIP_CONS* nlcon;               /* resulting constraint */
   SCIP_VAR* nlobj;                /* variable representing nonlinear objective (if used) */
   int state = READ;               /* state machine sate */
   int index = 0;                  /* variable index (picked up from instruction) */
   int* varind;                    /* array of all variable indicies */
   int* unqind;                    /* array of unique indicies */
   int* expind;                    /* array of expression variable indicies, matching to actual variables */
   int varcnt = -1;                /* keeps track of which expression variable we're up to */
   int expno = 0;                  /* index within expression array we are up to */
   size_t no_var = 0;              /* number of variables declared in instruction list */
   size_t no_ops = 0;              /* number of operations declared in instruction list */
   size_t no_unq = 0;              /* number of unique indicies found */
   int args[2] = {EMPTY,EMPTY};    /* used to keep track of previous arguments */
   int savexp = 0;                 /* used to keep track of last expression before entering exp (op) expr */
   int savexplst[MAX_DEPTH];       /* list of saved expressions to process later */
   int psavexplst = -1;            /* position in list */
   int savvarlst[MAX_DEPTH];       /* list of saved variables to process later (may not be needed...?) */
   int psavvarlst = -1;            /* position in list */
   int vari = varcnt;              /* varible index when evaluating functions */
   int savprolst[2 * MAX_DEPTH];   /* list of exp or var to process */
   int psavprolst = -1;            /* position in list */
   double num = 0;                 /* to save constants passed */
   double fval = 0;                /* evaluation value */
   double *lxval = NULL;           /* local xval with just variables present in expression copied (and correct order) */
   double one = 1.0;
   double mone = -1.0;
   double zero = 0.0;
   bool isunq = true;
   size_t i;
   size_t j;

   /* initialize lists */
   for (i = 0; i < MAX_DEPTH; i++)
   {
      savexplst[i] = EMPTY;
      savvarlst[i] = EMPTY;
   }

   for (i = 0; i < 2 * MAX_DEPTH; i++)
      savprolst[i] = EMPTY;

   /* determine number of variables and operations
    * (note we have to have a new variable 'expression' every time a variable appears, even if its repeated) */
   state = READ;
   for (i = 0; i < no_instr; i++)
   {
      switch(state)
      {
      case READ:
         if ( instr[i] > EXP )
            no_ops++;
         if ( instr[i] == VAR )
            no_var++;
         state = OTHER;
         break;

      case OTHER: /* i.e. arg / misc */
         state = READ;
         break;
      }
   }

   /* create variable index vector */
   varind = (int*)mxCalloc(no_var, sizeof(int));

   /* fill with indicies from instruction list */
   j = 0;
   state = READ;
   for (i = 0; i < no_instr; i++)
   {
      switch(state)
      {
      case READ:
         if ( instr[i] == VAR )
            state = VAR;
         else
            state = OTHER;
         break;

      case VAR:
         varind[j++] = (int)instr[i];
         state = READ;
         break;

      case OTHER:
         state = READ;
         break;
      }
   }

   /* find unique indicies to determine unique variables contained within the expression, for adding to the tree at the end */
   unqind = (int*)mxCalloc(no_var, sizeof(int)); /* make enough room for all to be unqiue */

   /* fill in unique */
   for (i = 0; i < no_var; i++)
   {
      /* no entries yet */
      if ( ! no_unq )
      {
         unqind[i] = varind[i];
         no_unq++;
      }
      else
      {
         /* see if entry already exists */
         isunq = true;
         for (j = 0; j < no_unq; j++)
         {
            if ( unqind[j] == varind[i] )
            {
               isunq = false;
               break;
            }
         }

         /* if is unique, add it */
         if ( isunq )
            unqind[no_unq++] = varind[i];
      }
   }

   /* allocate unique indicies a variable number (in this case just 0 to n), then allocate to expression index list */
   expind = (int*)mxCalloc(no_var, sizeof(int));

   /* fill in unique indicies (to use in SCIPexprCreate() for variables) */
   for (i = 0; i < no_var; i++)
   {
      for (j = 0; j < no_unq; j++)
      {
         if ( varind[i] == unqind[j] )
         {
            expind[i] = j;
            break;
         }
      }
   }

#ifdef DEBUG
   mexPrintf("\n---------------------------------------\nProcessing Nonlinear Expression\n---------------------------------------\n");
   mexPrintf("novar: %d, nounq: %d; no_ops: %d\n",no_var,no_unq,no_ops);

   /* print what we have found so far */
   for (i = 0; i < no_var; i++)
      mexPrintf("varind[%d] = %d\n", i, varind[i]);
   mexPrintf("\n");

   for (i = 0; i < no_unq; i++)
      mexPrintf("unqind[%d] = %d\n", i, unqind[i]);
   mexPrintf("\n");

   for (i = 0; i < no_var; i++)
      mexPrintf("expind[%d] = %d\n", i, expind[i]);
   mexPrintf("\n");
#endif

   /* create expression variable memory */
   SCIP_ERR( SCIPallocMemoryArray(scip, &expvars, no_var), "Error allocating expression variable memory.");

   /* create expression memory */
   SCIP_ERR( SCIPallocMemoryArray(scip, &exp, MAX(no_ops,1) ), "Error allocating expression memory.");

   /* for each expression variable, create and index */
   for (i = 0; i < no_var; i++)
   {
      SCIPexprCreate(SCIPblkmem(scip), &expvars[i], SCIP_EXPR_VARIDX, expind[i]); /* note we may have repeated indicies, as detailed above */
   }

   /* if objective, create an unbounded variable to add to objective, representing the nonlinear part */
   if ( isObj )
   {
      SCIP_ERR( SCIPcreateVarBasic(scip, &nlobj, "nlobj", -SCIPinfinity(scip), SCIPinfinity(scip), 1.0, SCIP_VARTYPE_CONTINUOUS), "Error adding nonlinear objective variable.");
      SCIP_ERR( SCIPaddVar(scip, nlobj), "Error adding nonlinear objective variable.");
   }

   /* check for constant number objective */
   if ( no_instr == 2 && instr[0] == NUM )
   {
      SCIP_ERR( SCIPexprCreate(SCIPblkmem(scip), &exp[0], SCIP_EXPR_CONST, instr[1]), "Error creating constant objective / constraint expression.");
      no_instr = 0; /* skip below */
#ifdef DEBUG
      mexPrintf("Found constant objective / constraint, skipping expression tree building.\n");
#endif
   }

   /* check for single variable objective / constraint */
   if ( no_instr == 2 && instr[0] == VAR )
   {
      SCIP_ERR( SCIPexprCreateLinear(SCIPblkmem(scip), &exp[0], 1, &expvars[0], &one, 0.0), "Error creating linear expression of a single variable.");
      no_instr = 0; /* skip below */
#ifdef DEBUG
      mexPrintf("Found single variable objective / constraint, skipping expression tree building.\n");
#endif
   }

   /* begin processing instruction list */
   state = READ;
   for (i = 0; i < no_instr; i++)
   {
#ifdef DEBUG
      debugPrintState(state, args, psavexplst, psavvarlst, psavprolst, varcnt, num);
#endif

      switch(state)
      {
      case READ:  /* read instruction */
         state = (int)instr[i];
         break;

      case NUM:   /* number passed, collect value */
         num = instr[i];
         state = READ;
         if ( args[0] == EMPTY ) /* all empty */
            args[0] = NUM;
         else if ( args[1] == EMPTY)  /* tag onto end */
            args[1] = NUM;
         else if ( args[0] == EXP )
         {  /* all full, save expression for use later */
            if ( psavexplst >= MAX_DEPTH )
               mexErrMsgTxt("Maximum function depth exceeded [expression list].");

            /* Save expression number in list */
            savexplst[++psavexplst] = expno-1;

            /* Update process list */
            savprolst[++psavprolst] = EXP;

            /* Correct args */
            args[0] = args[1];
            args[1] = NUM;
         }
         else if ( args[0] == VAR )
         {  /* all full, save variable for use later */
            if ( psavvarlst >= MAX_DEPTH )
               mexErrMsgTxt("Maximum function depth exceeded [variable list].");

            /* save variable number in list */
            savvarlst[++psavvarlst] = varcnt-1;

            /* update process list */
            savprolst[++psavprolst] = VAR;

            /* correct args */
            args[0] = args[1];
            args[1] = NUM;
         }
         else
            mexErrMsgTxt("Error in order of instructions.");
         break;

      case VAR:   /* variable index passed, increment our variable counter */
         varcnt++;
         state = READ;
         if ( args[0] == EMPTY ) /* all empty */
            args[0] = VAR;
         else if ( args[1] == EMPTY ) /* tag onto end */
            args[1] = VAR;
         else if ( args[0] == EXP )
         {
            /* all full */
            if ( psavexplst >= MAX_DEPTH )
               mexErrMsgTxt("Maximum function depth exceeded [expression list].");

            /* save expression number in list */
            savexplst[++psavexplst] = expno-1;

            /* update process list */
            savprolst[++psavprolst] = EXP;
            args[0] = args[1];
            args[1] = VAR;
         }
         else if ( args[0] == VAR )
         {  /* all full but with variables e.g x2 - x1^2 */
            if ( psavvarlst >= MAX_DEPTH)
               mexErrMsgTxt("Maximum function depth exceeded [variable list].");

            /* save variable number in list */
            if ( instr[i-3] == VAR && instr[i-5] == VAR ) /* check last two instructions left hand col (this will be at least the third instr) */
               savvarlst[++psavvarlst] = varcnt - 2;
            else
               savvarlst[++psavvarlst] = varcnt - 1;

            /* Update process list */
            savprolst[++psavprolst] = VAR;

            /* Correct args */
            args[0] = args[1];
            args[1] = VAR;
         }
         else
            mexErrMsgTxt("Error in order of instructions.");
         break;

         /* All two operand operators */
      case MUL:
      case DIV:
      case ADD:
      case SUB:
      case POW:

         /* read variable index */
         vari = varcnt;

         /* Check for waiting variable or expression to process */
         if ( args[1] == EMPTY && psavprolst >= 0 )
         {
            /* read if the last record is a var or exp, then check if we actually have one to process */
            int toProcess = savprolst[psavprolst--];
            if ( toProcess == VAR )
            {
               if ( psavvarlst < 0) /* none to process, error */
                  mexErrMsgTxt("Error reading variable to process - Process List indicates variable to process, but not present in variable list.");

               /* collect variable number to process */
               vari = savvarlst[psavvarlst];
               if ( vari == EMPTY || vari < 0 )
                  mexErrMsgTxt("Error processing waiting variable, found empty or negative index.");
#ifdef DEBUG
               mexPrintf("-----\nProcessing Waiting Variable: %d [Index %d, %d remaining]\n-----\n", vari, varind[vari], psavvarlst);
#endif
               /* correct args */
               args[1] = VAR;
               instr[i] = 1;  /* will flip if subtraction or division */
               psavvarlst--;
            }
            else if ( toProcess == EXP )
            {
               if ( psavexplst < 0 ) /* none to process, error */
                  mexErrMsgTxt("Error reading expression to process - Process List indicates expression to process, but not present in expression list.");
#ifdef DEBUG
               mexPrintf("-----\nProcessing Waiting Expression: %d [%d remaining]\n-----\n", savexplst[psavexplst], psavexplst);
#endif
               args[1] = EXP;
            }
            else
               mexErrMsgTxt("Unknown entry in process list.");
         }

         /* check we have two arguments */
         if ( args[0] == EMPTY || args[1] == EMPTY )
            mexErrMsgTxt("Error attempting to create nonlinear expression, operator doesn't have two operands.");

         /* NUM (op) VAR (linear except division) */
         if ( args[0] == NUM && args[1] == VAR )
         {
            switch(state)
            {
            case MUL:
               SCIP_ERR( SCIPexprCreateLinear(SCIPblkmem(scip), &exp[expno], 1, &expvars[vari], &num, 0.0), "Error creating linear multiply expression (num * var).");
               break;

            case ADD:
               SCIP_ERR( SCIPexprCreateLinear(SCIPblkmem(scip), &exp[expno], 1, &expvars[vari], &one, num), "Error creating linear add expression (num + var).");
               break;

            case SUB:
               SCIP_ERR( SCIPexprCreateLinear(SCIPblkmem(scip), &exp[expno], 1, &expvars[vari], &mone, num), "Error creating linear subtract expression (num - var).");
               break;

            case DIV:
               SCIP_EXPR* divexp; /* intermediate expression */
               SCIP_ERR( SCIPexprCreate(SCIPblkmem(scip), &divexp, SCIP_EXPR_CONST, num), "Error creating division intermediate expression.");
               SCIP_ERR( SCIPexprCreate(SCIPblkmem(scip), &exp[expno], SCIP_EXPR_DIV, divexp, expvars[vari]), "Error creating divide expression (num / var).");
               break;

            case POW:
               mexErrMsgTxt("You cannot use POWER with the exponent as a variable. For x^y use exp(y*log(x)).");

            default:
               mexErrMsgTxt("Operator not implemented yet for NUM (op) VAR!");
            }
            args[0] = EXP;
            args[1] = EMPTY;
         }
         /* VAR (op) NUM (linear except pow) */
         else if ( args[0] == VAR && args[1] == NUM )
         {
            switch(state)
            {
            case MUL:
               SCIP_ERR( SCIPexprCreateLinear(SCIPblkmem(scip), &exp[expno], 1, &expvars[vari], &num, 0.0), "Error creating linear multiply expression (var * num).");
               break;

            case ADD:
               SCIP_ERR( SCIPexprCreateLinear(SCIPblkmem(scip), &exp[expno], 1, &expvars[vari], &one, num), "Error creating linear add expression (var + num).");
               break;

            case SUB:
               SCIP_ERR( SCIPexprCreateLinear(SCIPblkmem(scip), &exp[expno], 1, &expvars[vari], &one, -num), "Error creating linear subtract expression (var - num).");
               break;

            case DIV:
               SCIP_ERR( SCIPexprCreateLinear(SCIPblkmem(scip), &exp[expno], 1, &expvars[vari], &(num = 1/num), 0.0), "Error creating linear divide expression (var * 1/num).");
               break;

            case POW:
               /* check for integer power (may need to think of a better way) */
               if ( (double)((int)num) == num )
               {
                  if ( num == 2 )
                  {  /* squared */
                     SCIP_ERR( SCIPexprCreate(SCIPblkmem(scip), &exp[expno], SCIP_EXPR_SQUARE, expvars[vari], (int)num), "Error creating squared expression.");
                  }
                  else
                  {
                     SCIP_ERR( SCIPexprCreate(SCIPblkmem(scip), &exp[expno], SCIP_EXPR_INTPOWER, expvars[vari], (int)num), "Error creating integer power expression (var ^ inum).");
                  }
               }
               else
               {
                  /* if ( num < 0 )
                     mexErrMsgTxt("This interface does not support a variable raised to a negative real power.");
                  else */
                  SCIP_ERR( SCIPexprCreate(SCIPblkmem(scip), &exp[expno], SCIP_EXPR_REALPOWER, expvars[vari], num), "Error creating power expression (var ^ num).");
               }
               break;

            default:
               mexErrMsgTxt("Operator not implemented yet for VAR (op) NUM!");
            }
            args[0] = EXP;
            args[1] = EMPTY;
         }
         /* VAR (op) VAR */
         else if ( args[0] == VAR && args[1] == VAR )
         {
            switch ( state )
            {
            case MUL:
               SCIP_ERR( SCIPexprCreate(SCIPblkmem(scip), &exp[expno], SCIP_EXPR_MUL, expvars[vari-1], expvars[vari]), "Error creating mul expression (var * var).");
               break;

            case DIV:
               SCIP_ERR( SCIPexprCreate(SCIPblkmem(scip), &exp[expno], SCIP_EXPR_DIV, expvars[vari-1], expvars[vari]), "Error creating div expression (var / var).");
               break;

            case ADD:
               SCIP_ERR( SCIPexprCreate(SCIPblkmem(scip), &exp[expno], SCIP_EXPR_PLUS, expvars[vari-1], expvars[vari]), "Error creating add expression (var + var).");
               break;

            case SUB:
               SCIP_ERR( SCIPexprCreate(SCIPblkmem(scip), &exp[expno], SCIP_EXPR_MINUS, expvars[vari-1], expvars[vari]), "Error creating sub expression (var - var).");
               break;

            case POW:
               mexErrMsgTxt("You cannot use POWER with the exponent as a variable. For x^y use exp(y*log(x)).");

            default:
               mexErrMsgTxt("Operator not implemented yet for VAR (op) VAR!");
            }
            args[0] = EXP;
            args[1] = EMPTY;
         }
         /* EXP (op) NUM */
         else if ( args[0] == EXP && args[1] == NUM )
         {
            switch ( state )
            {
            case MUL:
               SCIP_ERR( SCIPexprCreateLinear(SCIPblkmem(scip), &exp[expno], 1, &exp[expno-1], &num, 0.0), "Error creating linear multiply expression (exp * num).");
               break;

            case ADD:
               SCIP_ERR( SCIPexprCreateLinear(SCIPblkmem(scip), &exp[expno], 1, &exp[expno-1], &one, num), "Error creating linear add expression (exp + num).");
               break;

            case SUB:
               /* check for flipped args */
               if ( instr[i] == 1 )
               {  /* flip */
                  SCIP_EXPR* divexp; /* intermediate expression */
                  SCIP_ERR( SCIPexprCreate(SCIPblkmem(scip), &divexp, SCIP_EXPR_CONST, num), "Error creating subtraction intermediate expression.");
                  SCIP_ERR( SCIPexprCreate(SCIPblkmem(scip), &exp[expno], SCIP_EXPR_MINUS, divexp, exp[expno-1]), "Error creating subtract expression (num - exp).");
               }
               else
                  SCIP_ERR( SCIPexprCreateLinear(SCIPblkmem(scip), &exp[expno], 1, &exp[expno-1], &one, -num), "Error creating linear subtract expression (exp - num).");
               break;

            case DIV:
               /* check for flipped args */
               if ( instr[i] == 1 )
               {  /* flip */
                  SCIP_EXPR* divexp; /* intermediate expression */
                  SCIP_ERR( SCIPexprCreate(SCIPblkmem(scip), &divexp, SCIP_EXPR_CONST, num), "Error creating division intermediate expression.");
                  SCIP_ERR( SCIPexprCreate(SCIPblkmem(scip), &exp[expno], SCIP_EXPR_DIV, divexp, exp[expno-1]), "Error creating divide expression (num / exp).");
               }
               else
                  SCIP_ERR( SCIPexprCreateLinear(SCIPblkmem(scip), &exp[expno], 1, &exp[expno-1], &(num = 1/num), 0.0), "Error creating linear divide expression (exp * 1/num).");
               break;

            case POW:
               /* check for integer power (may need to think of a better way) */
               if ( (double)((int)num) == num )
               {
                  SCIP_ERR( SCIPexprCreate(SCIPblkmem(scip), &exp[expno], SCIP_EXPR_INTPOWER, exp[expno-1], (int)num), "Error creating integer power expression (exp ^ inum).");
               }
               else
               {
                  /*if ( num < 0 )
                    mexErrMsgTxt("This interface does not support an expression raised to a negative real power");
                    else*/
                  SCIP_ERR( SCIPexprCreate(SCIPblkmem(scip), &exp[expno], SCIP_EXPR_REALPOWER, exp[expno-1], num), "Error creating power expression (exp ^ num).");
               }
               break;

            default:
               mexErrMsgTxt("Operator not implemented yet for NUM (op) VAR!");
            }
            args[0] = EXP;
            args[1] = EMPTY;
         }
         /* EXP (op) VAR */
         else if ( args[0] == EXP && args[1] == VAR )
         {
            switch(state)
            {
            case MUL:
               SCIP_ERR( SCIPexprCreate(SCIPblkmem(scip), &exp[expno], SCIP_EXPR_MUL, exp[expno-1], expvars[vari]), "Error creating mul expression (exp * var).");
               break;

            case ADD:
               SCIP_ERR( SCIPexprCreate(SCIPblkmem(scip), &exp[expno], SCIP_EXPR_PLUS, exp[expno-1], expvars[vari]), "Error creating add expression (exp + var).");
               break;

            case SUB:
               /* check for flipped args */
               if ( instr[i] == 1 )
               {  /* flip */
                  SCIP_ERR( SCIPexprCreate(SCIPblkmem(scip), &exp[expno], SCIP_EXPR_MINUS, expvars[vari], exp[expno-1]), "Error creating subtract expression (var - exp).");
               }
               else
               {
                  SCIP_ERR( SCIPexprCreate(SCIPblkmem(scip), &exp[expno], SCIP_EXPR_MINUS, exp[expno-1], expvars[vari]), "Error creating subtract expression (exp - var).");
               }
               break;

            case DIV:
               /* check for flipped args */
               if ( instr[i] == 1 )
               {  /* flip */
                  SCIP_ERR( SCIPexprCreate(SCIPblkmem(scip), &exp[expno], SCIP_EXPR_DIV, expvars[vari], exp[expno-1]), "Error creating div expression (var / exp).");
               }
               else
               {
                  SCIP_ERR( SCIPexprCreate(SCIPblkmem(scip), &exp[expno], SCIP_EXPR_DIV, exp[expno-1], expvars[vari]), "Error creating div expression (exp / var).");
               }
               break;

            case POW:
               mexErrMsgTxt("You cannot use POWER with the exponent as a variable. For x^y use exp(y*log(x)).");

            default:
               mexErrMsgTxt("Operator not implemented yet for EXP (op) VAR!");
            }
            args[0] = EXP;
            args[1] = EMPTY;
         }
         /* EXP (op) EXP */
         else if ( args[0] == EXP && args[1] == EXP )
         {
            /* collect expression number to process */
            savexp = savexplst[psavexplst];
            if ( savexp == EMPTY || savexp < 0 )
               mexErrMsgTxt("Error processing waiting expression, found empty index.");

            switch(state)
            {
            case ADD:
               SCIP_ERR( SCIPexprCreate(SCIPblkmem(scip), &exp[expno], SCIP_EXPR_PLUS, exp[savexp], exp[expno-1]), "Error creating add expression (exp + exp).");
               break;

            case SUB:
               /* check for flipped args */
               if ( instr[i] == 1 )
               {  /* flip */
                  SCIP_ERR( SCIPexprCreate(SCIPblkmem(scip), &exp[expno], SCIP_EXPR_MINUS, exp[expno-1], exp[savexp]), "Error creating sub expression (exp_old - exp_new).");
               }
               else
               {
                  SCIP_ERR( SCIPexprCreate(SCIPblkmem(scip), &exp[expno], SCIP_EXPR_MINUS, exp[savexp], exp[expno-1]), "Error creating sub expression (exp_new - exp_old).");
               }
               break;

            case DIV:
               /* check for flipped args */
               if ( instr[i] == 1 )
               {  /* flip */
                  SCIP_ERR( SCIPexprCreate(SCIPblkmem(scip), &exp[expno], SCIP_EXPR_DIV, exp[expno-1], exp[savexp]), "Error creating div expression (exp_old / exp_new).");
               }
               else
               {
                  SCIP_ERR( SCIPexprCreate(SCIPblkmem(scip), &exp[expno], SCIP_EXPR_DIV, exp[savexp], exp[expno-1]), "Error creating div expression (exp_new / exp_old).");
               }
               break;

            case MUL:
               SCIP_ERR( SCIPexprCreate(SCIPblkmem(scip), &exp[expno], SCIP_EXPR_MUL, exp[savexp], exp[expno-1]), "Error creating add expression (exp * exp).");
               break;

            default:
               mexErrMsgTxt("Unexpected operator for combining expressions, currently only +, -, * and / supported");
            }
            args[0] = EXP;
            args[1] = EMPTY;
            psavexplst--;
         }
         else
            mexErrMsgTxt("Grouping of arguments not implemented yet.");

         if ( i == (no_instr-1) )
            state = EXIT;
         else
         {
            state = READ;
            expno++; /* increment for next operator */
         }
         break;

         /* all single operand functions */
      case SQUARE:
      case SQRT:
      case EXPNT:
      case LOG:
      case ABS:

         /* read variable index */
         vari = varcnt;

         /* check we have one argument */
         if ( args[0] == EMPTY )
            mexErrMsgTxt("Error attempting to create nonlinear expression, function doesn't have an operand.");

         /* check for unprocessed exp [e.g. (3-x(2)).*log(x(1))] */
         if ( args[0] == EXP && args[1] == VAR )
         {
            /* save expression to process later */
            if ( psavexplst >= MAX_DEPTH )
               mexErrMsgTxt("Maximum function depth exceeded [expression list].");

            /* save expression number in list */
            savexplst[++psavexplst] = expno-1;

            /* Update process list */
            savprolst[++psavprolst] = EXP;

            /* correct args */
            args[0] = VAR;
            args[1] = EMPTY;
         }

         /* check for unprocessed var [e.g. x2 - exp(x1)] */
         if ( args[0] == VAR && args[1] == VAR )
         {
            /* save variable to process later */
            if ( psavvarlst >= MAX_DEPTH )
               mexErrMsgTxt("Maximum function depth exceeded [variable list].");

            /* save variable number in list */
            savvarlst[++psavvarlst] = varcnt-1;

            /* update process list */
            savprolst[++psavprolst] = VAR;

            /* continue processing function, we will process this variable next free round */
         }

         /* FCN ( VAR ) enum {EMPTY=-2,READ,NUM,VAR,EXP,MUL,DIV,ADD,SUB,SQUARE,SQRT,POW,EXPNT,LOG,SIN,COS,TAN,MIN,MAX,ABS,SIGN}; */
         if ( args[0] == VAR )
         {
            /* check wheter variable index realistic */
            if ( vari < 0 )
               mexErrMsgTxt("Variable index is negative when creating fcn(var).");

            switch(state)
            {
            case SQRT:
               SCIP_ERR( SCIPexprCreate(SCIPblkmem(scip), &exp[expno], SCIP_EXPR_SQRT, expvars[vari]), "Error creating sqrt expression sqrt(var).");
               break;

            case EXPNT:
               SCIP_ERR( SCIPexprCreate(SCIPblkmem(scip), &exp[expno], SCIP_EXPR_EXP, expvars[vari]), "Error creating exponential expression exp(var).");
               break;

            case LOG:
               SCIP_ERR( SCIPexprCreate(SCIPblkmem(scip), &exp[expno], SCIP_EXPR_LOG, expvars[vari]), "Error creating natural log expression log(var).");
               break;

            case ABS:
               SCIP_ERR( SCIPexprCreate(SCIPblkmem(scip), &exp[expno], SCIP_EXPR_ABS, expvars[vari]), "Error creating absolute value expression abs(var).");
               break;

            default:
               mexErrMsgTxt("Operator not implemented yet for FCN ( VAR )!");
            }
            args[0] = EXP;
            args[1] = EMPTY;
         }
         /* FCN ( EXP ) */
         else if ( args[0] == EXP )
         {
            switch ( state )
            {
            case SQRT:
               SCIP_ERR( SCIPexprCreate(SCIPblkmem(scip), &exp[expno], SCIP_EXPR_SQRT, exp[expno-1]), "Error creating sqrt expression sqrt(exp).");
               break;

            case EXPNT:
               SCIP_ERR( SCIPexprCreate(SCIPblkmem(scip), &exp[expno], SCIP_EXPR_EXP, exp[expno-1]), "Error creating exponential expression exp(exp).");
               break;

            case LOG:
               SCIP_ERR( SCIPexprCreate(SCIPblkmem(scip), &exp[expno], SCIP_EXPR_LOG, exp[expno-1]), "Error creating natural log expression log(exp).");
               break;

            case ABS:
               SCIP_ERR( SCIPexprCreate(SCIPblkmem(scip), &exp[expno], SCIP_EXPR_ABS, exp[expno-1]), "Error creating absolute value expression abs(exp)");
               break;

            default:
               mexErrMsgTxt("Operator not implemented yet for FCN ( EXP )!");
            }
            args[0] = EXP;
            args[1] = EMPTY;
         }
         else
            mexErrMsgTxt("Unknown argument to operate function on, only options are VAR (variable) or EXP (expression).");

         if ( i == (no_instr-1) )
            state = EXIT;
         else
         {
            state = READ;
            expno++; /* increment for next operator */
         }
         break;

      case MIN:
      case MAX:
         mexErrMsgTxt("Max and Min not currently implemented (in this interface and SCIP).");
         break;

      case SIN:
      case COS:
      case TAN:
         mexErrMsgTxt("Trigonometric functions not currently implemented (in SCIP).");
         break;
      case SIGN:
         mexErrMsgTxt("Sign not currently implemented (in SCIP).");
         break;

      case EXIT:
         break; /* exit switch case */

      default:
         mexErrMsgTxt("Unknown (or out of order) instruction.");
      }
   }
#ifdef DEBUG
   debugPrintState(state, args, psavexplst, psavvarlst, psavprolst, varcnt, num);
   mexPrintf("\n---------------------------------------\nSummary at Expression Tree Create:\n");
   mexPrintf("expno:  %3d [should equal %3d]\nvarcnt: %3d [should equal %3d]\n", expno, no_ops-1, varcnt, no_var-1);
   mexPrintf("---------------------------------------\n");
#endif

   /* create Expression Tree to hold our final expression */
   SCIP_ERR( SCIPexprtreeCreate(SCIPblkmem(scip), &exprtree, exp[expno], no_unq, 0, NULL), "Error creating expression tree.");

   /* create variable array of variables to add (can't seem to find a method to does this one by one) */
   SCIP_VAR** unqvars = NULL;
   SCIP_ERR( SCIPallocMemoryArray(scip,&unqvars,no_unq), "Error allocating unique variable array.");

   for (i = 0; i < no_unq; i++)
      unqvars[i] = vars[unqind[i]]; /* store address of variable into double pointer array */

   /* set the variables within the tree */
   SCIP_ERR( SCIPexprtreeSetVars(exprtree, no_unq, unqvars), "Error setting expression tree var.");

   /* 'Validate' the constraint if initial guess supplied */
   if ( xval != NULL )
   {
      /* copy in xval variables used in the expression */
      lxval = (double*)mxCalloc(no_var, sizeof(double));

      /* note variables appear in the same order as found in expression */
      for (i = 0; i < no_unq; i++)
         lxval[i] = xval[unqind[i]];

      SCIPexprtreeEval(exprtree, lxval, &fval);

      /* free memory */
      mxFree(lxval);
   }

   /* create the nonlinear constraint, add it, then release it */
   if ( isObj )
      sprintf(msgbuf, "NonlinearObj%zd", nlno);
   else
      sprintf(msgbuf,"NonlinearExp%zd", nlno);

   SCIP_ERR( SCIPcreateConsBasicNonlinear(scip, &nlcon, msgbuf, 0, NULL, NULL, 1, &exprtree, &one, lhs, rhs), "Error creating nonlinear constraint!");
   SCIPexprtreeFree(&exprtree);

   /* if nonlinear objective, add to objective */
   if ( isObj )
   {
      SCIP_ERR( SCIPaddLinearVarNonlinear(scip, nlcon, nlobj, -1.0), "Error adding nonlinear objective linear term.");
      SCIP_ERR( SCIPreleaseVar(scip, &nlobj), "Error releasing SCIP nonlinear objective variable.");
   }

   /* continue to free the constraint */
   SCIP_ERR( SCIPaddCons(scip, nlcon), "Error adding nonlinear constraint.");
   SCIP_ERR( SCIPreleaseCons(scip, &nlcon), "Error freeing nonlinear constraint.");

   /* memory clean up */
   SCIPfreeMemoryArray(scip, &exp);
   SCIPfreeMemoryArray(scip, &expvars);
   SCIPfreeMemoryArray(scip, &unqvars);
   mxFree(expind);
   mxFree(varind);
   mxFree(unqind);

   return fval;
}

#endif  /* old expressions */
