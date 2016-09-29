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
xengnttab_handle *gnttab;

/* Global reference to our logger. */
xentoollog_logger_stdiostream *logger;




int main(int argc, char **argv)
{
    int domid;
    uint32_t gref;
    int position;

    char * mapping;

    /* Ensure we have our arguments. */
    if(argc != 3) {
        printf("usage: %s [otherside_domid] [gref]\n\n", argv[0]);
        exit(0);
    }

    /* Fetch the domid to be read. */
    domid = atoi(argv[1]);
    gref  = atoi(argv[2]);

    /* Bring up our main logger... */
    logger = xtl_createlogger_stdiostream(stdin, 0, XTL_STDIOSTREAM_SHOW_DATE);

    /* Bring up the granting interface. */
    gntshr = xengnttab_open((xentoollog_logger *)logger, 0);
    if(!gntshr) {
        printf("Failed to open gnttab!\n");
        exit(-1);
    }

    /* Grant out a page, and then wait. */
    printf("Granting in reference %" PRIu32 " to domid %d.\n", gref, domid);
    mapping = xengnttab_map_grant_refs(gnttab, 1, &domid, &gref, PROT_READ | PROT_WRITE);
    printf("Granted in reference to address %p.", mapping)

    if(!mapping) {
        printf("Something's not right, bailing.\n");
        exit(-1);
    }

    /* Waits until we receive our terminal signal. */
    while(continue_running) {
        int ch = fgetc(stdin);

        if(ch == EOF)
          break;

        /* For testing. */
        mapped[position++ % 5] = ch;
    }

    /* Bring down the granting interface. */
    xengntshr_close(gntshr);
}
