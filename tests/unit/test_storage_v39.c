/**
 * v3.9 Field Card: GLKFS remount, CRC, streaming append, honest SD probe.
 */
#include "glk_svc/glk_storage.h"
#include "glk_svc/glk_fs.h"
#include "glk_svc/glk_vault.h"
#include "glk/glk_kernel.h"
#include "glk_svc/glk_audit.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

#define REQUIRE(c)                                                                 \
    do {                                                                           \
        if (!(c)) {                                                                \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);           \
            return 1;                                                              \
        }                                                                          \
    } while (0)

static void mk(const char* p) {
#ifdef _WIN32
    _mkdir(p);
#else
    mkdir(p, 0755);
#endif
}

static int xor_payload_in_file(const char* path, const char* needle, size_t nlen) {
    FILE* f = fopen(path, "rb+");
    if (!f) return -1;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return -1;
    }
    long sz = ftell(f);
    if (sz <= 0) {
        fclose(f);
        return -1;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return -1;
    }
    char* buf = (char*)malloc((size_t)sz);
    if (!buf) {
        fclose(f);
        return -1;
    }
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        free(buf);
        fclose(f);
        return -1;
    }
    long hit = -1;
    for (long i = 0; i + (long)nlen <= sz; i++) {
        if (memcmp(buf + i, needle, nlen) == 0) {
            hit = i;
            break;
        }
    }
    free(buf);
    if (hit < 0) {
        fclose(f);
        return -1;
    }
    if (fseek(f, hit, SEEK_SET) != 0) {
        fclose(f);
        return -1;
    }
    int c = fgetc(f);
    if (c == EOF) {
        fclose(f);
        return -1;
    }
    if (fseek(f, hit, SEEK_SET) != 0) {
        fclose(f);
        return -1;
    }
    unsigned char x = (unsigned char)(c ^ 0xFF);
    if (fwrite(&x, 1, 1, f) != 1) {
        fclose(f);
        return -1;
    }
    fclose(f);
    return 0;
}

int main(void) {
    glk_kernel_init();
    glk_audit_init("test_audit_v39.jsonl");
    mk("glkfs_v39_scratch");
    const char* img = "glkfs_v39_scratch/field.card";
    glk_storage_shutdown();
    remove(img);

    glk_err_t ie = glk_storage_init_card(img);
    if (ie != GLK_OK) {
        glk_storage_shutdown();
        remove(img);
        ie = glk_storage_init_card(img);
    }
    REQUIRE(ie == GLK_OK);
    REQUIRE(glk_storage_backend() == GLK_STOR_BE_GLKFS);
    REQUIRE(strcmp(glk_storage_probe_str(), "host_image") == 0);
    REQUIRE(glk_storage_present());
    REQUIRE(glk_storage_usable());
    REQUIRE(glk_storage_mode() == GLK_STOR_OK || glk_storage_mode() == GLK_STOR_DEGRADED);

    const char* payload = "field-card-v39";
    REQUIRE(glk_storage_write_file("vault/probe.txt", payload, strlen(payload)) == GLK_OK);
    char buf[64];
    size_t n = 0;
    REQUIRE(glk_storage_read_file("vault/probe.txt", buf, sizeof(buf), &n) == GLK_OK);
    REQUIRE(n == strlen(payload));
    REQUIRE(memcmp(buf, payload, n) == 0);

    glk_vault_init();
    REQUIRE(glk_vault_bind_storage("vault/events.jsonl") == GLK_OK);
    glk_vault_push("card", "rx", 3, 0, 0.5f);
    REQUIRE(glk_vault_flush() == GLK_OK);
    uint32_t count1 = glk_vault_count();
    REQUIRE(count1 >= 1);

    /* streaming append past the old 2048-byte cap */
    char chunk[300];
    memset(chunk, 'A', sizeof(chunk));
    for (int i = 0; i < 12; i++) {
        REQUIRE(glk_storage_append_file("vault/big.bin", chunk, sizeof(chunk)) == GLK_OK);
    }
    char* big = (char*)malloc(4096);
    REQUIRE(big != NULL);
    size_t bn = 0;
    REQUIRE(glk_storage_read_file("vault/big.bin", big, 4096, &bn) == GLK_OK);
    REQUIRE(bn == 12 * sizeof(chunk));
    REQUIRE(big[0] == 'A' && big[bn - 1] == 'A');

    uint32_t files_a = 0, used_a = 0;
    glk_fs_info(&files_a, &used_a);
    REQUIRE(files_a >= 2);
    REQUIRE(glk_storage_write_file("vault/probe.txt", payload, strlen(payload)) == GLK_OK);
    uint32_t files_b = 0, used_b = 0;
    glk_fs_info(&files_b, &used_b);
    REQUIRE(used_b == used_a); /* rewrite reused extent */

    uint32_t h32 = 0;
    REQUIRE(glk_storage_file_hash32("vault/big.bin", &h32) == GLK_OK);
    REQUIRE(h32 != 0);

    /* unplug */
    glk_storage_shutdown();
    REQUIRE(!glk_storage_present());
    REQUIRE(glk_storage_mode() == GLK_STOR_ABSENT);
    REQUIRE(strcmp(glk_storage_probe_str(), "none") == 0);

    /* replug */
    REQUIRE(glk_storage_init_card(img) == GLK_OK);
    REQUIRE(glk_storage_backend() == GLK_STOR_BE_GLKFS);
    n = 0;
    memset(buf, 0, sizeof(buf));
    REQUIRE(glk_storage_read_file("vault/probe.txt", buf, sizeof(buf), &n) == GLK_OK);
    REQUIRE(n == strlen(payload));
    REQUIRE(memcmp(buf, payload, n) == 0);
    bn = 0;
    REQUIRE(glk_storage_read_file("vault/big.bin", big, 4096, &bn) == GLK_OK);
    REQUIRE(bn == 12 * sizeof(chunk));

    glk_vault_init();
    REQUIRE(glk_vault_bind_storage("vault/events.jsonl") == GLK_OK);
    REQUIRE(glk_vault_count() >= 1);

    char st[512];
    glk_storage_status_json(st, sizeof(st));
    REQUIRE(strstr(st, "\"backend\":\"glkfs\"") != NULL);
    REQUIRE(strstr(st, "\"probe\":\"host_image\"") != NULL);
    REQUIRE(strstr(st, "\"files\":") != NULL);
    REQUIRE(strstr(st, "\"used_blocks\":") != NULL);

    /* data CRC fail-closed on read; do not reformat */
    glk_storage_shutdown();
    REQUIRE(xor_payload_in_file(img, payload, strlen(payload)) == 0);
    REQUIRE(glk_storage_init_card(img) == GLK_OK);
    n = 0;
    REQUIRE(glk_storage_read_file("vault/probe.txt", buf, sizeof(buf), &n) == GLK_ERR_CORRUPT);

    /* super CRC fail: stay corrupt, do not wipe/reformat */
    glk_storage_shutdown();
    FILE* sf = fopen(img, "rb+");
    REQUIRE(sf != NULL);
    REQUIRE(fseek(sf, 24, SEEK_SET) == 0);
    int sc = fgetc(sf);
    REQUIRE(sc != EOF);
    REQUIRE(fseek(sf, 24, SEEK_SET) == 0);
    unsigned char sx = (unsigned char)(sc ^ 0xFF);
    REQUIRE(fwrite(&sx, 1, 1, sf) == 1);
    fclose(sf);
    REQUIRE(glk_storage_init_card(img) == GLK_ERR_CORRUPT);
    REQUIRE(glk_storage_mode() == GLK_STOR_CORRUPT);
    REQUIRE(!glk_storage_usable());
    REQUIRE(strcmp(glk_storage_probe_str(), "host_image") == 0);

    /* no card path: host sim, never a fake RAM disk */
    glk_storage_shutdown();
    glk_err_t de = glk_storage_init_device();
    REQUIRE(de == GLK_ERR_NOTFOUND || de == GLK_ERR_NOSUPPORT);
    REQUIRE(glk_storage_mode() == GLK_STOR_ABSENT);
    REQUIRE(!glk_storage_present());
    const char* pr = glk_storage_probe_str();
    REQUIRE(strcmp(pr, "host_sim") == 0 || strcmp(pr, "no_card") == 0);
    glk_storage_status_json(st, sizeof(st));
    REQUIRE(strstr(st, "\"backend\":\"absent\"") != NULL);
    REQUIRE(strstr(st, "\"probe\":\"host_sim\"") != NULL || strstr(st, "\"probe\":\"no_card\"") != NULL);

    free(big);
    glk_storage_shutdown();
    printf("test_storage_v39 ok\n");
    return 0;
}
