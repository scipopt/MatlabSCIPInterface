/* SCIPMEX - A MATLAB MEX Interface to SCIP
 * Released Under the BSD 3-Clause License:
 *
 * Based on code by Jonathan Currie, which was based in parts on matscip.c supplied with SCIP.
 *
 * Authors:
 * - Jonathan Currie
 * - Marc Pfetsch
 */

#ifndef SCIPMEXINC
#define SCIPMEXINC

#include <scip/scip.h>
#include "mex.h"

/** add nonlinear constraint to problem */
SCIP_EXPORT
double addNonlinearCon(
   SCIP*                 scip,               /**< SCIP instance */
   SCIP_VAR**            vars,               /**< variable array */
   double*               instr,              /**< array of instructions */
   size_t                no_instr,           /**< number of instructions */
   double                lhs,                /**< left hand side */
   double                rhs,                /**< right hand side */
   double*               x0,                 /**< initial guess for variables */
   size_t                nlno,               /**< index of nonlinear constraint */
   bool                  isObj               /**< is this the objective function */
   );

#endif
