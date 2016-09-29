/*
 * biggrant: library for simple multi-page granting
 *
 * Copyright (C) 2016 Assured Information Security, Inc.
 * Author: Kyle J. Temkin <temkink@ainfosec.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef __LIBBIGGRANT_H__
#define __LIBBIGGRANT_H__

#include <xentoollog.h>
#include <xengnttab.h>

#include <xenbiggrant-osdep.h>

struct xenbiggrant_instance;
typedef struct xenbiggrant_instance xenbiggrant_instance;

/**
 * Creates a new BigGrant instance, which should be destroyed with
 * destroy_biggrant_instance.
 *
 * @param logger The Xen logger to use, or NULL to use the default logger
 *    for the given platform.
 * @return A new BigGrant instance, or NULL if one could not be created.
 */
xenbiggrant_instance *create_biggrant_instance(xentoollog_logger *logger);

/**
 * Destroys the provided BigGrant instance, cleaning up as much as is possible.
 *
 * @param bg The BigGrant instance to be destroyed.
 */
void destroy_biggrant_instance(xenbiggrant_instance *bg);

/**
 * Allocates a new buffer of shareable memory, and provides a single reference
 * that can be used to map it into another domain.
 *
 * @param bg The BigGrant instance that should be used to grant out the given
 *    memory.
 * @param count The size of the buffer to be created, in bytes.
 * @param domid The domid of the domain that will receive the share.
 * @param writable True iff the given share should be wreitable.
 * @param metaref Out argument-- on succesful grant, this will contain the
 *    'metareference' handle that can be used to map in this memory from
 *    another domain.
 * @return A pointer to the shared memory buffer, or NULL if no buffer could
 *    be allocated.
 */
void *allocate_shared_buffer(xenbiggrant_instance *bg, size_t count,
    domid_t domid, int writable, uint32_t * metaref);


#endif
