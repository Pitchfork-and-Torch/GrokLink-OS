/**
 * v3.9 Field Card: GLKFS remount survives "unplug".
 */
#include "glk_svc/glk_storage.h"
#include "glk_svc/glk_vault.h"
#include "glk/glk_kernel.h"
#include "glk_svc/glk_audit.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

static void mk(const char* p) {
#ifdef _WIN32
    _mkdir(p);
#else
    mkdir(p, 0755);
#endif
}

int main(void) {
    glk_kernel_init();
    glk_audit_init("test_audit_v39.jsonl");
    mk("build-host");
    mk("build-host/test_card_v39");
    const char* img = "build-host/test_card_v39/field.card";
    remove(img);

    assert(glk_storage_init_card(img) == GLK_OK);
    assert(glk_storage_backend() == GLK_STOR_BE_GLKFS);
    assert(glk_storage_present());
    assert(glk_storage_usable());
    assert(glk_storage_mode() == GLK_STOR_OK || glk_storage_mode() == GLK_STOR_DEGRADED);

    const char* payload = "field-card-v39";
    assert(glk_storage_write_file("vault/probe.txt", payload, strlen(payload)) == GLK_OK);
    char buf[64];
    size_t n = 0;
    assert(glk_storage_read_file("vault/probe.txt", buf, sizeof(buf), &n) == GLK_OK);
    assert(n == strlen(payload));
    assert(memcmp(buf, payload, n) == 0);

    glk_vault_init();
    assert(glk_vault_bind_storage("vault/events.jsonl") == GLK_OK);
    glk_vault_push("card", "rx", 3, 0, 0.5f);
    assert(glk_vault_flush() == GLK_OK);
    uint32_t count1 = glk_vault_count();
    assert(count1 >= 1);

    /* unplug */
    glk_storage_shutdown();
    assert(!glk_storage_present());
    assert(glk_storage_mode() == GLK_STOR_ABSENT);

    /* replug */
    assert(glk_storage_init_card(img) == GLK_OK);
    assert(glk_storage_backend() == GLK_STOR_BE_GLKFS);
    n = 0;
    memset(buf, 0, sizeof(buf));
    assert(glk_storage_read_file("vault/probe.txt", buf, sizeof(buf), &n) == GLK_OK);
    assert(n == strlen(payload));
    assert(memcmp(buf, payload, n) == 0);

    glk_vault_init();
    assert(glk_vault_bind_storage("vault/events.jsonl") == GLK_OK);
    assert(glk_vault_count() >= 1);

    char st[320];
    glk_storage_status_json(st, sizeof(st));
    assert(strstr(st, "\"backend\":\"glkfs\"") != NULL);

    /* no card path */
    glk_storage_shutdown();
    glk_err_t de = glk_storage_init_device();
    assert(de == GLK_ERR_NOTFOUND || de == GLK_ERR_NOSUPPORT);
    assert(glk_storage_mode() == GLK_STOR_ABSENT);

    glk_storage_shutdown();
    printf("test_storage_v39 ok\n");
    return 0;
}
