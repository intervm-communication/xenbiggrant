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

#include <xenbiggrant.h>

// XXX: Convert these to proper functions?
#define log_error(_l, err, _f...) xtl_log((_l)->logger, XTL_ERROR, err, "biggrant", _f)
#define log_warning(_l, err, _f...) xtl_log((_l)->logger, XTL_ERROR, err, "biggrant", _f)

/**
 * Data structure representing an BigGrant instance.
 */
struct xenbiggrant_instance {

    /* The logger used to report status and issues. */
    xentoollog_logger *logger, *logger_tofree;

    /* Our internal connections to gnttab and gntshr. */
    xengntshr_handle *xgs;
    xengnttab_handle *xgt;
};


xenbiggrant_instance *create_biggrant_instance(xentoollog_logger *logger)
{
    xenbiggrant_instance * bg;

    /* Allocate the storage for our new biggrant instance. */
    bg = malloc(sizeof(xenbiggrant_instance));
    if(!bg) {
        log_error(bg, ENOMEM, "Could not create a BigGrant instance!");
        return NULL;
    }

    bg->logger = logger;
    bg->logger_tofree = NULL;

    /* If a logger wasn't provided, create a default logger. */
    if(!bg->logger) {
        bg->logger = bg->logger_tofree = xtl_createlogger_default();

        /* If we couldn't create the default, bail out. */
        if(!bg->logger) {
            log_error(bg, ENOMEM, "Could not create a logger!");

            destroy_biggrant_instance(bg);
            return NULL;
        }
    }

    /* Create our internal connections to gntshr... */
    bg->xgs = xengntshr_open(bg->logger, 0);
    if(!bg->xgs) {
        log_error(bg, 0, "Could not open the Xen grant sharing interface!");
        destroy_biggrant_instance(bg);
        return NULL;
    }

    /* ... and gnttab. */
    bg->xgt = xengnttab_open(bg->logger, 0);
    if(!bg->xgt) {
        log_error(bg, 0, "Could not open the Xen grant mapping interface!");
        destroy_biggrant_instance(bg);
        return NULL;
    }

    /* Finally, return our new BigGrant instance. */
    return bg;
}


void destroy_biggrant_instance(xenbiggrant_instance *bg) {
    if(!bg)
      return;

    if(bg->xgs)
      xengntshr_close(bg->xgs);

    if(bg->xgt)
      xengnttab_close(bg->xgt);

    if(bg->logger_tofree)
      free(bg->logger_tofree);

    if(bg)
      free(bg);
}

/**
 * Creates a metapage that can be used to share each of the relevant grant references.
 *
 * @param refs The list of references to be coalesced into a metapage.
 * @page refs_are_metarefs True iff the given references each point to a metapage.
 * @param metapage Out argument that returns the produced metapage,
 *
 * @return A grant reference to a metapage, which may in turn point to a tree of
 * other pages.
 */
static int create_metapage_for_grantrefs(uint32_t *refs,
    int refs_are_metarefs, grant_ref_t *metapaage) {

    /* XXX TODO XXX */
    return -ENOSYS;
}

void *allocate_shared_buffer(xenbiggrant_instance *bg, size_t count,
    domid_t domid, int writable, uint32_t * metaref) {

    void *mapping;
    uint32_t *refs;
    uint32_t metapage;
    int rc;

    if(!bg && !metaref) {
        log_error(bg, EINVAL, "Received a null required argument!\n");
        return NULL;
    }

    /*
     * Allocate a temporary array to store the grant references for the
     * shared buffer.
     */
    refs = malloc(sizeof(uint32_t) * count);
    if(!refs) {
        log_error(bg, ENOMEM, "Could not allocate a buffer for outgoing grant references!");
        return NULL;
    }

    /*
     * And create the actual share, receiving each of the relevant grant
     * references.
     */
    mapping = xengntshr_share_pages(bg->xgs, domid, count, refs, writable);
    if(!mapping) {
        log_error(bg, errno, "Could not allocate a buffer for outgoing grant references!");
        goto cleanup;
    }

    /*
     * Finally, coalesce the references into a single metareference, to be
     * shared.
     */
    rc = create_metapage_for_grantrefs(refs, 0, metaref);
    if(rc) {
        log_error(bg, rc, "Could not create a metapage for the references!");
        goto cleanup;
    }

    free(refs);
    return mapping;

cleanup:
    if(mapping)
        xengntshr_unshare(bg->xgs, mapping, count);
    if(refs)
        free(refs);
}
