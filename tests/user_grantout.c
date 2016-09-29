/**
 * Simple evaluation of the userland granting libraries.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include <xentoollog.h>
#include <xengnttab.h>

#define PAGE_SIZE 4096


/* Global referenece to our grant table interface. */
xengntshr_handle *gntshr;

/* Global reference to our logger. */
xentoollog_logger_stdiostream *logger;

/* True when we should continue running. */
int continue_running = 1;

/* Stores a buffer to notify on buffer changes. */
char last_buffer[PAGE_SIZE];

uint32_t grant_out_page(int domid, char ** mapping)
{
    uint32_t granted_page[1];
    char * result = xengntshr_share_pages(gntshr, domid, 1, granted_page, 1);

    if(!mapping || !result)
      return -1;

    *mapping = result;
    return granted_page[0];
}


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

    /* Bring up our main logger... */
    logger = xtl_createlogger_stdiostream(stdin, 0, XTL_STDIOSTREAM_SHOW_DATE);

    /* Bring up the granting interface. */
    gntshr = xengntshr_open((xentoollog_logger *)logger, 0);
    if(!gntshr) {
        printf("Failed to open gntshr!\n");
        exit(-1);
    }

    /* Grant out a page, and then wait. */
    printf("Granting a page to domid %d.\n", domid);
    gref = grant_out_page(domid, &mapping);
    printf("Granted; new gref is %" PRIu32 ", mapped to %p.\n", gref, mapping);

    /* Waits until we receive our terminal signal. */
    while(continue_running) {

        if(memcmp(last_buffer, mapping, PAGE_SIZE)) {
            printf("Buffer changed! First five chars: %c%c%c%c%c", mapping[0], mapping[1], mapping[2], mapping[3], mapping[4]);
            memcpy(last_buffer, mapping, PAGE_SIZE);
        }

    }

    /* Bring down the granting interface. */
    xengntshr_close(gntshr);
}
