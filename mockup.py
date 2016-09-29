#!/usr/bin/env python3
#
# Simple functional mockup for metagranting structures for libbiggrant.
#

import sys

# Simulated system state.
last_granted_page = 0
backing_pages = {}

# Stores the number of Refs that fit in a metapage.
# Simulates real space contention.
REFS_PER_METAPAGE = 992

# Simple magic to check for a valid metapage.
METAPAGE_MAGIC = "MAGIC"


def _gnttab_alloc_pages(nr_pages):
    """
    Simulates allocation of grantable pages.

    nr_pages: The number of fake pages to be generated.
    """

    global last_granted_page

    # Create a sequence of fake grant references.
    page_numbers = list(range(last_granted_page + 1, last_granted_page + nr_pages + 1))
    last_granted_page = page_numbers[-1]

    # Initialize each page to an dictionary, which represnts our physical page.
    for page_number in page_numbers:
        backing_pages[page_number] = { "address": page_number}

    #... and return the page numbers.
    return [backing_pages[page_number] for page_number in page_numbers]


def _gnttab_free_pages(page_numbers):
    """
    Simulates allocation of freeable pages.

    pages: The pages to be freed.
    """

    for page_number in page_numbers:

        # If we have a page to free...
        if page_number in backing_pages:

            # ... check to make sure it's not granted out...
            page = backing_pages[page_number]
            if 'granted' in page:
                print("ISSUE: Freeing page {} while still granted out!".format(page_number))

            # ... and then free it.
            del backing_pages[page_number]

        else:
            print("ISSUE: Invalid free of page {}".format(page_number))


def _gnttab_end_foreign_access(refs):
    """
    Simulates ending access to a granted page.
    """

    # Convert each reference to its core page.
    pages = [backing_pages[ref] for ref in refs]

    # Mark each page as no longer granted.
    for page in pages:

        if 'granted' in page:
            del page['granted']
        else:
            print("ISSUE: Attempting to end access to page {}, which isn't currently granted!".format(page['address']))


def _gnttab_grant_foreign_access(pages):
    """
    Simulates granbting out a page by returning its address as a reference.
    """

    # Mark each page as granted.
    for page in pages:
        page['granted'] = True

    # And return simulated grefs.
    return [page["address"] for page in pages]


def _allocate_shared_pages_raw(nr_pages):
    """
    Simulates allocation and granting-out of pages.

    nr_pages: The number of fake grefs to be generated.
    """

    pages = _gnttab_alloc_pages(nr_pages)
    grefs = _gnttab_grant_foreign_access(pages)

    return pages, grefs

def _alloc_shared_page_raw():
    """
    Simulates allocation and granting-out of a single page.
    """
    pages, grefs = _allocate_shared_pages_raw(1)
    return pages[0], grefs[0]


def _gnttab_map_refs(refs):
    """
    Simulates mapping in a single page from a grant reference.
    """

    pages =  [backing_pages[ref] for ref in refs]

    # Mark each page as mapped.
    for page in pages:
        page["mapped"] = True

    return pages


def _gnttab_unmap_pages(pages):
    """
        Simulates unmapping a set of pages.
    """

    # Clear the mapped attribute from each page.
    for page in pages:
        if 'mapped' not in page:
            print('ISSUE: Trying to unmap a non-mapped page: {}'.format(page['address']))
        else:
            del page['mapped']


def _gnttab_unmap_refs(refs):
    """
        Simulates unmapping a set of refs.
    """

    pages = [backing_pages[ref] for ref in refs]

    for page in pages:
        if 'granted' not in page:
            print('ISSUE: Attempting to map in a non-granted page ({})!'.format(page['address']))
            print('--DOMAIN TERMINATED--')
            sys.exit(-1)

    _gnttab_unmap_pages(pages)


def _map_granted_page(metaref):
    """
        Simulates mapping in a single page by grant reference.
    """
    pages = _gnttab_map_refs([metaref])
    return pages[0]


def _unmap_granted_page(page):
    """
        Simulates mapping in a single page by grant reference.
    """
    _gnttab_unmap_pages([page])


def _create_metapage_for_grantrefs(refs, refs_are_metarefs):
    """
    Creates a simulated metapage for a collection of grant references.
    """

    if len(refs) <= REFS_PER_METAPAGE:

        # Create our metapage.
        metapage, metaref = _alloc_shared_page_raw()

        # Initialize it...
        metapage["magic"] = METAPAGE_MAGIC

        # Base case: our references fit in a single metapage.
        metapage["refs_are_metarefs"] = refs_are_metarefs
        metapage["refs"] = refs

    else:

        sub_metarefs = []

        # Inductive case: we need to split our references into groups,
        # each represented by their own metapage.
        for i in range(0, len(refs), REFS_PER_METAPAGE):

            # Grab a single, representable group of references, and convert it into a single metaref.
            sub_metaref = _create_metapage_for_grantrefs(refs[i:i + REFS_PER_METAPAGE], refs_are_metarefs)
            sub_metarefs.append(sub_metaref)

        return _create_metapage_for_grantrefs(sub_metarefs, True)


    # Return the grant reference for the metapage.
    assert len(metapage["refs"]) <= REFS_PER_METAPAGE
    return metaref


def _get_grantrefs_in_metapage(metaref):
    """
        Returns a list of grantrefs represented by the given metapage.
    """

    # Get a handle on the root metapage for the distribution.
    metapage = _map_granted_page(metaref)
    refs = None
    touched_metarefs = [metaref]

    # Sanity check our page.
    if metapage["magic"] != METAPAGE_MAGIC:
        raise ValueError("Tried to map in a non-metapage!")

    if not metapage["refs_are_metarefs"]:

        # Base case: we have raw grant references. Map them in directly.
        refs = metapage["refs"]

    else:

        # Inductive case: we have metarefs. We'll map each of those in.
        sub_metarefs = metapage["refs"]
        refs = []

        assert len(metapage["refs"]) <= REFS_PER_METAPAGE

        # Parse each of the metapages.
        for sub_metaref in sub_metarefs:
            subrefs, touched_submetarefs = _get_grantrefs_in_metapage(sub_metaref)

            # Store the page references we'll want to resolve, and any metapages
            # we've touched.
            refs.extend(subrefs)
            touched_metarefs.extend(touched_submetarefs)


    # Unmap the mapped in metapage.
    _unmap_granted_page(metapage)

    # And return the final collection of pages.
    return refs, touched_metarefs


def map_remote_buffer(metaref):
    """
        Simulates mapping in of a remote buffer by metaref.
    """

    refs, _ = _get_grantrefs_in_metapage(metaref)
    return _gnttab_map_refs(refs)


def unmap_remote_buffer(metaref):
    """
        Simulates mapping in of a remote buffer by metaref.
    """

    refs, _ = _get_grantrefs_in_metapage(metaref)
    return _gnttab_unmap_refs(refs)


def allocate_shared_buffer(page_count):
    """
    Public API: Allocates a shared buffer, and returns a meta-reference that
    can be used to map it in.
    """

    pages, grefs = _allocate_shared_pages_raw(page_count)
    metaref = _create_metapage_for_grantrefs(grefs, False)

    return metaref, pages


def free_shared_buffer(metaref):

    # Get a list of all shared pages and currently allocated metapages.
    shared_pages, all_metapages = _get_grantrefs_in_metapage(metaref)

    # Create a list of the pages we'll need to free: both the granted pages
    # and our descriptive metapages need to be freed.
    pages_to_free = shared_pages + all_metapages

    # End foreign access to our pages, and abort.
    _gnttab_end_foreign_access(pages_to_free)
    _gnttab_free_pages(pages_to_free)



def main():

    # Ensure we have a sane-ish argument.
    if len(sys.argv) != 2:
        print("Usage: {} <pages_to_grant>".format(sys.argv[0]))
        sys.exit(-1)

    # Accept the number of pages to map from the command line.
    count = int(sys.argv[1])

    # Allocate a simulated buffer.
    metaref, pages = allocate_shared_buffer(count)
    granted_addresses = [page["address"] for page in pages]
    print("")
    print("--")
    print("Granted out the following pages: {}".format(granted_addresses))
    print("Mapped all pages into metaref: {}".format(metaref))

    # Map in our simulated buffer. This wouldn't work on real Xen,
    # but we allow this in simulation for testing.
    mapped_pages = map_remote_buffer(metaref)
    mapped_addresses = [page["address"] for page in mapped_pages]
    print("")
    print("--")
    print("Mapping in metaref: {}".format(metaref))
    print("Granted in the following pages: {}".format(mapped_addresses))

    # Unmap the remote buffer.
    unmap_remote_buffer(metaref)
    print("")
    print("--")
    print("Unmapping all pages in metaref: {}".format(metaref))

    # Check to make sure all pages have been unmapped.
    success = True
    for page_number, page in backing_pages.items():
        if 'mapped' in page:
            print("ISSUE: Page {} remains mapped!".format(page['address']))
            success = False

    if success:
        print("All pages unmapped okay!")


    # Unshare the simulated buffer.
    print("")
    print("--")
    print("Unmapping all pages in shared buffer...")
    free_shared_buffer(metaref)

    if backing_pages:
        for page_number in backing_pages:
            print("ISSUE: Page {} was leaked!".format(page_number))
    else:
        print("All pages free'd okay!")


if __name__ == "__main__":
    main()
