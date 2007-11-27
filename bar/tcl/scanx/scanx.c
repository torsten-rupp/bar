#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#include <tcl.h>

/* Version information */
static char *extensionName = "scanx";
static char *extensionVersion = "1.0.0.0";

/* Tcl_CmdProc declaration(s) */
// static char usage[] = "Usage: scanx ";

/* extended version of Tcl "scan" command */
int Tcl_ScanxObjCmd(ClientData dummy, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]);

/*---------------------------------------------------------------------*/

static int Init(Tcl_Interp *interp)
{
  static int initialized = 0;

  /* Tell tcl about this package */
  Tcl_PkgProvide(interp,extensionName,extensionVersion);

  if (initialized == 0)
  {
    initialized = 1;

    Tcl_CreateObjCommand(interp,"scanx",Tcl_ScanxObjCmd,0,0);
  }

  return 0;
}

int Scanx_Init(Tcl_Interp *interp)
{
  return Init(interp);
}

int Scanx_SafeInit(Tcl_Interp *interp)
{
  return Init(interp);
}

/* end of file */
