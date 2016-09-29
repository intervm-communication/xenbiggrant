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

#define REFS_PER_METAPAGE ((PAGE_SIZE - sizeof(struct metapage) / sizeof(uint32_t)))

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

/**
 * Data structure stored inside each BigGrant metapage.
 */
struct metapage {
    uint32_t api_version;
    uint32_t ref_types;
    uint32_t magic;
    uint32_t num_refs;
    grant_ref_t refs[];
} __attribute__((packed));


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
 * Allocates a single page, typically for use as a metapage.
 *
 * @param bg The BigGrant instance that should be used to allocate the given page.
 * @param domid
 * @param mapped_page Out argument. Receives a pointer to the page's contents.
 * @param reference Out argument. Receives the grant reference for the given page.
 *
 * @return 0 on success, or an error code on failure. If an error code is returned,
 *    all out arguments are invalid.
 */
static int allocate_metaref_page(xenbiggrant_instance *bg, domid_t domid,
    struct metapage **mapped_page, grant_ref_t *ref) {

    if(!bg || !mapped_page || !ref) {
        log_error(bg, EINVAL, "Passed a null argument to allocate_granted_page!");
        return EINVAL;
    }

    /* Allocate the relevant page. */
    *mapped_page = (struct metapage*)
        xengntshr_share_pages(bg->xgs, domid, 1, ref, 0);

    if(*mapped_page)
      return 0;
    else
      return ENOMEM; //XXX-- how does share_pages signal its errors?
}

/**
 * Returns the number of metarefs required to store /one level/
 * of metareferences.
 *
 * @param num_refs The number of references in question.
 * @param metarefs The number of metapages necessary to store
 */
static uint32_t metarefs_to_store(uint32_t pages)
{
    uint32_t aligned_pages = pages + (REFS_PER_METAPAGE - 1);
    return aligned_pages / REFS_PER_METAPAGE;
}


/**
 * Creates a metapage that can be used to share each of the relevant grant references.
 *
 * @param refs The list of references to be coalesced into a metapage.
 * @page refs_are_metarefs True iff the given references each point to a metapage.
 * @param metaref Out argument that returns a reference to the produced metapage.
 *
 * @return A grant reference to a metapage, which may in turn point to a tree of
 * other pages.
 */
static int create_metapage_for_grantrefs(xenbiggrant_instance *bg,
    uint32_t *refs, size_t count, int ref_type,
    domid_t otherside_domid, grant_ref_t *metaref) {

    struct metapage *metapage;
    int rc;

    if (!bg || !refs || !metaref) {
        log_error(bg, EINVAL, "Passed a null argument to %s!", __func__);
        return EINVAL;
    }

    /* Base case: our references will fit in a single metapage. */
    if (count <= REFS_PER_METAPAGE) {

        /* Allocate a metapage to store the grant references. */
        rc = allocate_metaref_page(bg, otherside_domid, &metapage, metaref);
        if (rc || !metapage) {
            log_error(bg, rc, "Could not allocate a metapage!");
            return rc;
        }

        /* Set up the metapage. */
        metapage->api_version = METAPAGE_API_VERSION;
        metapage->magic       = METAPAGE_MAGIC;

        metapage->num_refs = count;
        metapage->ref_types = ref_type;
        memcpy(metapage->refs, refs, sizeof(*refs) * count);
    }
    /*
     * Recursive case: we have more metarefs than will fit into a page;
     * we'll divide them into several metapages.
     */
    else {
      int i;
      int num_subrefs = metarefs_to_store(count);
      int remaining = count;

      /*
       * Allocate a temporary array to store any metareferences associated
       * with storing the relevant grant references. This will be freed
       * momentarily.
       */
      grant_ref_t *subrefs = malloc(sizeof(grant_ref_t) * num_subrefs);
      grant_ref_t *current_subref = subrefs;

      /*
       * For each collection of references that will fit into a single
       * metapage, allocate a new metapage, and add it to our list.
       */
      for (i = 0; i < count; i += REFS_PER_METAPAGE) {

          rc = create_metapage_for_grantrefs(bg, refs + i,
              min(remaining, REFS_PER_METAPAGE), ref_type,
              otherside_domid, current_subref);

          if(rc) {
              log_error(bg, rc, "Couldn't create a sub-metaref! Bailing.");

              // XXX CLEAN UP XXX
              return rc;
          }

          ++current_subref;
          remaining -= min(remaining, REFS_PER_METAPAGE);
      }

      /*
       * Finally, gather all of the sub-metarefs into a metapage of
       * their own.
       */
       rc= create_metapage_for_grantrefs(bg, subrefs, num_subrefs,
           REF_TYPE_METAREFS, otherside_domid, metaref);

        if(rc) {
            log_error(bg, rc, "Couldn't create a sub-meta meta-reference!"
                " Good luck with that.");

            // XXX CLEAN UP XXX
            return rc;
        }

       /* Clean up our temporary subreference array. */
       free(subrefs);
    }

    return 0;
}

void *allocate_shared_buffer(xenbiggrant_instance *bg, size_t size,
    domid_t domid, int writable, uint32_t * metaref) {

    void *mapping;
    uint32_t *refs;
    int rc;

    int actual_size = (size + (PAGE_SIZE - 1));
    int num_pages = actual_size >> PAGE_SHIFT;

    if(!bg && !metaref) {
        log_error(bg, EINVAL, "Received a null required argument!\n");
        return NULL;
    }

    /*
     * Allocate a temporary array to store the grant references for the
     * shared buffer.
     */
    refs = malloc(sizeof(uint32_t) * num_pages);
    if(!refs) {
        log_error(bg, ENOMEM, "Could not allocate a buffer for outgoing grant references!");
        return NULL;
    }

    /*
     * And create the actual share, receiving each of the relevant grant
     * references.
     */
    mapping = xengntshr_share_pages(bg->xgs, domid, num_pages, refs, writable);
    if(!mapping) {
        log_error(bg, errno, "Could not allocate a buffer for outgoing grant references!");
        goto cleanup;
    }

    /*
     * Finally, coalesce the references into a single metareference, to be
     * shared.
     */
    rc = create_metapage_for_grantrefs(bg, refs, num_pages, 0, domid, metaref);
    if(rc) {
        log_error(bg, rc, "Could not create a metapage for the references!");
        goto cleanup;
    }

    free(refs);
    return mapping;

cleanup:
    if(mapping)
        xengntshr_unshare(bg->xgs, mapping, num_pages);
    if(refs)
        free(refs);

    return NULL;
}

/**
 * Retreives all grant references pointed to by a given metapage.
 *
 * @param bg The BigGrant instance that has been or will be dealing
 *    with the given metareferences.
 * @param refs Out argument; receives a pointer to an array of
 *    all plain grant references referenced by the given metaref.
 */
static int get_grantrefs_in_metapage(xenbiggrant_instance *bg,
    uint32_t metaref, uint32_t **refs, uint32_t **metarefs)
{
    //struct metapage *metapage = 
}


void release_shared_buffer(xenbiggrant_instance *bg, void *addr, size_t size)
{

}
