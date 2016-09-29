/**
 * Simple evaluation of the userland granting libraries.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include <xentoollog.h>
#include <xengnttab.h>
#include <xenbiggrant.h>

#define PAGE_SIZE 4096

/* Global referenece to our grant table interface. */
xenbiggrant_instance *bg;

int main(int argc, char **argv)
{
    int domid;
    uint32_t gref;

    char * mapping;

    /* Ensure we have our arguments. */
    if(argc != 2) {
        printf("usage: %s [otherside_domid]\n\n", argv[0]);
        exit(0);
    }

    /* Fetch the domid to be read. */
    domid = atoi(argv[1]);

    /* Bring up the granting interface. */
    bg = create_biggrant_instance(NULL);
    if(!bg) {
        printf("Failed to open gntshr!\n");
        exit(-1);
    }

    /* Grant out a page, and then wait. */
    printf("Granting a page to domid %d.\n", domid);
    mapping = allocate_shared_buffer(bg, 16 * 1024, domid, 1, &gref);
    if(mapping)
        printf("Granted; new gref is %" PRIu32 ", mapped to %p.\n", gref, mapping);

    /* TODO: Test more! */

    /* Bring down the granting interface. */
    destroy_biggrant_instance(bg);
}
