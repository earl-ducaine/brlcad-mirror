/*                          I N I T . C
 * BRL-CAD
 *
 * Copyright (c) 2019 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file init.c
 *
 * Sole purpose of this file is to capture status and aspects of an
 * application and the application's environment when first run such
 * as initial working directory and available memory.  That is, it's
 * intended to be reflective, not generative or conditional logic.
 *
 * Please do NOT add application-specific logic here.
 *
 * NOTE: as this init is global to ALL applications before main(),
 * care must be taken to not write to STDOUT or STDERR or app output
 * may be corrupted, signals can be raised, or worse.
 *
 */

#include "common.h"

#include "bu/defines.h"
#include "bu/app.h"
#include "bu/parallel.h"


int BU_SEM_GENERAL;
int BU_SEM_SYSCALL;
int BU_SEM_BN_NOISE;
int BU_SEM_MAPPEDFILE;
int BU_SEM_THREAD;
int BU_SEM_MALLOC;
int BU_SEM_DATETIME;
int BU_SEM_DIR;


INITIALIZE(libbu)
{
    char iwd[MAXPATHLEN] = {0};

    BU_SEMAPHORE_DEFINE(BU_SEM_GENERAL);
    BU_SEMAPHORE_DEFINE(BU_SEM_SYSCALL);
    BU_SEMAPHORE_DEFINE(BU_SEM_BN_NOISE);
    BU_SEMAPHORE_DEFINE(BU_SEM_MAPPEDFILE);
    BU_SEMAPHORE_DEFINE(BU_SEM_THREAD);
    BU_SEMAPHORE_DEFINE(BU_SEM_MALLOC);
    BU_SEMAPHORE_DEFINE(BU_SEM_DATETIME);
    BU_SEMAPHORE_DEFINE(BU_SEM_DIR);

    bu_getiwd(iwd, MAXPATHLEN);
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
