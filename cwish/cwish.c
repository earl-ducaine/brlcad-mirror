/*
 *				C W I S H . C
 *
 *  This is yet another version of the WISH program for BRL-CAD (maybe it
 *  should be called YAW :-)). CWISH initializes the Tcl interfaces of the
 *  BRL-CAD libraries as well as [incr Tcl] and [incr Tk]. This version also
 *  provides command history and command line editing.
 *
 *  Author -
 *	  Robert G. Parker
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Notice -
 *	Re-distribution of this software is restricted, as described in
 *	your "Statement of Terms and Conditions for the Release of
 *	The BRL-CAD Package" license agreement.
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1998 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 *
 */

#include "conf.h"
#include "itk.h"

#include "machine.h"
#include "externs.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"

extern int cmdInit();
extern void Cad_Main();

HIDDEN int Cad_AppInit();
HIDDEN Tk_Window tkwin;

Tcl_Interp *interp;

int
main(argc, argv)
     int argc;
     char **argv;
{
	if (bu_avail_cpus() > 1) {
		rt_g.rtg_parallel = 1;
		bu_semaphore_init(RT_SEM_LAST);
	}

	/* initialize RT's global state */
	BU_LIST_INIT(&rt_g.rtg_vlfree);

	/* Create the interpreter */
	interp = Tcl_CreateInterp();
	Cad_Main(argc, argv, Cad_AppInit, interp);

	return 0;
}

HIDDEN int
Cad_AppInit(interp)
     Tcl_Interp *interp;
{
	struct bu_vls vls;
	char *pathname;

	/* Initialize Tcl */
	if (Tcl_Init(interp) == TCL_ERROR) {
		bu_log("Tcl_Init error %s\n", interp->result);
		return TCL_ERROR;
	}

	/* Initialize Tk */
	if (Tk_Init(interp) == TCL_ERROR) {
		bu_log("Tk_Init error %s\n", interp->result);
		return TCL_ERROR;
	} 

	Tcl_StaticPackage(interp, "Tk", Tk_Init, Tk_SafeInit);

	/* Initialize [incr Tcl] */
	if (Itcl_Init(interp) == TCL_ERROR) {
		bu_log("Itcl_Init error %s\n", interp->result);
		return TCL_ERROR;
	}

	/* Initialize [incr Tk] */
	if (Itk_Init(interp) == TCL_ERROR) {
		bu_log("Itk_Init error %s\n", interp->result);
		return TCL_ERROR;
	}

	/* Import [incr Tcl] commands into the global namespace. */
	if (Tcl_Import(interp, Tcl_GetGlobalNamespace(interp),
		       "::itcl::*", /* allowOverwrite */ 1) != TCL_OK) {
		bu_log("Tcl_Import error %s\n", interp->result);
		return TCL_ERROR;
	}

	/* Import [incr Tk] commands into the global namespace */
	if (Tcl_Import(interp, Tcl_GetGlobalNamespace(interp),
		       "::itk::*", /* allowOverwrite */ 1) != TCL_OK) {
		bu_log("Tcl_Import error %s\n", interp->result);
		return TCL_ERROR;
	}

	Tcl_StaticPackage(interp, "Itcl", Itcl_Init, Itcl_SafeInit);
	Tcl_StaticPackage(interp, "Itk", Itk_Init, (Tcl_PackageInitProc *) NULL);

	/* Initialize the Iwidgets package */
	if (Tcl_Eval(interp, "package require Iwidgets") != TCL_OK) {
		bu_log("Tcl_Eval error %s\n", interp->result);
		return TCL_ERROR;
	}

	/* Import iwidgets into the global namespace */
	if (Tcl_Import(interp, Tcl_GetGlobalNamespace(interp),
		       "::iwidgets::*", /* allowOverwrite */ 1) != TCL_OK) {
		bu_log("Tcl_Import error %s\n", interp->result);
		return TCL_ERROR;
	}

	/* Initialize libdm */
	if (Dm_Init(interp) == TCL_ERROR) {
		bu_log("Dm_Init error %s\n", interp->result);
		return TCL_ERROR;
	}

	/* Initialize libfb */
	if (Fb_Init(interp) == TCL_ERROR) {
		bu_log("Fb_Init error %s\n", interp->result);
		return TCL_ERROR;
	}

	/* Initialize libbu */
	bu_tcl_setup(interp);

	/* Initialize libbn */
	bn_tcl_setup(interp);

	/* Initialize librt */
	rt_tcl_setup(interp);

	if ((tkwin = Tk_MainWindow(interp)) == NULL)
		return TCL_ERROR;


	/* Locate the BRL-CAD-specific Tcl scripts */
	pathname = bu_brlcad_path( "" );

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "lappend auto_path %stclscripts", pathname);
	(void)Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	/* register cwish commands */
	cmdInit(interp);

	return TCL_OK;
}
