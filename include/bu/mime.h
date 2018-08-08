/*                         M I M E . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2018 United States Government as represented by
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

#ifndef BU_MIME_H
#define BU_MIME_H

#include "common.h"

#include "bu/defines.h"
#include "bu/mime_types.h"


__BEGIN_DECLS


/**
 * @addtogroup bu_mime
 *
 * @brief Functions generated by the mime.cmake script - see the
 * generated files mime.c and mime_types.h for the resulting C code
 * and mime type declarations
 *
 * Standard types are maintained in misc/mime.types - that file
 * is a copy of the Apache file and is not changed locally:
 * http://svn.apache.org/repos/asf/httpd/httpd/trunk/docs/conf/mime.types
 *
 * Customizations for BRL-CAD are maintained in misc/mime_cad.types
 * and any types not covered by the standard mime set (such as
 * the majority of geometry formats) are added there instead of
 * changing mime.types.
 */
/** { */


/**
 * Use the file extension to attempt to determine the media type
 * of the file within the specified context.
 *
 * returns -1 if no match was found, or an integer if a result
 * was found.  It is the responsibility of the caller to cast
 * the return int to the correct mime_CONTEXT_t type.
 */
BU_EXPORT extern int bu_file_mime(const char *ext, bu_mime_context_t context);


/**
 * Given a mime type and a context, return the file extension(s)
 * associated with that type.
 *
 * returns NULL if no match was found, or a comma separated string
 * containing the extensions if a result was found.
 * It is the responsibility of the caller to free the returned string.
 */
BU_EXPORT extern const char *bu_file_mime_ext(int t, bu_mime_context_t context);


/**
 * Given a mime type and a context, return a human readable string
 * spelling out the type (corresponding to the enum string in
 * source code.)
 *
 * returns NULL if no match was found, or a string if a result was found.
 * It is the responsibility of the caller to free the returned string.
 */
BU_EXPORT extern const char *bu_file_mime_str(int t, bu_mime_context_t context);


/**
 * Given a string produced by bu_file_mime_str, convert it back into
 * integer form.
 *
 * returns -1 if no match was found, or an integer if a result
 * was found.  It is the responsibility of the caller to cast
 * the return int to the correct mime_CONTEXT_t type.
 */
BU_EXPORT extern int bu_file_mime_int(const char *str);


/** @} */

__END_DECLS

#endif  /* BU_MIME_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
