/**
 * Device SD probe for Field Card.
 * Honest: no card or no SPI response -> absent. Does not fake a RAM disk.
 */
#include "glk_svc/glk_blockdev.h"

#include <string.h>

#if defined(GLK_PLATFORM_STM32)

glk_err_t glk_blockdev_sd_probe(glk_blockdev_t* d) {
    if (d) {
        memset(d, 0, sizeof(*d));
        d->present = false;
        d->block_count = 0;
    }
    /* SPI SD CMD0/CMD8 is human-gated hardware work. Until a card ACKs,
     * stay absent so ROM-passive remains the truth. */
    return GLK_ERR_NOTFOUND;
}

void glk_blockdev_file_close(glk_blockdev_t* d) {
    if (d) memset(d, 0, sizeof(*d));
}

glk_err_t glk_blockdev_file_open(glk_blockdev_t* d, const char* path, uint32_t block_count, bool create) {
    (void)path;
    (void)block_count;
    (void)create;
    if (d) memset(d, 0, sizeof(*d));
    return GLK_ERR_NOSUPPORT;
}

#endif
