/* SCIPMEX - A MATLAB MEX Interface to SCIP
 * Released Under the BSD 3-Clause License:
 *
 * Based on code by Jonathan Currie, which was based in parts on matscip.c supplied with SCIP.
 *
 * Authors:
 * - Jonathan Currie
 * - Marc Pfetsch
 */

#ifndef SCIPEVENTMEXINC
#define SCIPEVENTMEXINC

#include <scip/scip.h>

/** add Ctrl-C event handler */
SCIP_EXPORT
SCIP_RETCODE SCIPincludeCtrlCEventHdlr(
   SCIP*                 scip                /**< SCIP instance */
   );

#endif
