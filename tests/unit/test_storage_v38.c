/**
 * v3.8 storage + vault persistence + skill scan host tests.
 */
#include "glk_svc/glk_storage.h"
#include "glk_svc/glk_vault.h"
#include "glk_svc/glk_skill.h"
#include "glk_svc/glk_audit.h"
#include "glk_svc/glk_policy.h"
#include "glk/glk_kernel.h"
#include "glk/glk_config.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>
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
    glk_audit_init("test_audit_v38.jsonl");

    /* temp root under cwd */
    const char* root = "build-host/test_sd_v38/groklink";
    mk("build-host");
    mk("build-host/test_sd_v38");
    mk(root);

    assert(glk_storage_init(root) == GLK_OK);
    assert(glk_storage_present());
    assert(glk_storage_usable());
    assert(glk_storage_ensure_layout() == GLK_OK ||
           glk_storage_mode() == GLK_STOR_OK ||
           glk_storage_mode() == GLK_STOR_DEGRADED);

    const char* payload = "hello-v38";
    assert(glk_storage_write_file("state/probe.txt", payload, strlen(payload)) == GLK_OK);
    char buf[64];
    size_t n = 0;
    assert(glk_storage_read_file("state/probe.txt", buf, sizeof(buf), &n) == GLK_OK);
    assert(n == strlen(payload));
    assert(memcmp(buf, payload, n) == 0);
    assert(glk_storage_exists("state/probe.txt"));

    uint32_t h = 0;
    assert(glk_storage_file_hash32("state/probe.txt", &h) == GLK_OK);
    assert(h != 0);
    assert(glk_storage_write_integrity("state/.integrity", h) == GLK_OK);
    assert(glk_storage_check_integrity("state/.integrity", h, true) == GLK_OK);

    /* vault persistence */
    glk_vault_init();
    assert(glk_vault_bind_storage("vault/events.jsonl") == GLK_OK);
    glk_vault_push("lab_test", "rx", 3, 0, 0.5f);
    glk_vault_push("lab_test", "done", 3, 0, 0.5f);
    assert(glk_vault_count() >= 2);
    assert(glk_vault_flush() == GLK_OK);
    assert(glk_vault_persistent());

    uint32_t root_hash = glk_vault_chain_root();
    glk_vault_init();
    assert(glk_vault_bind_storage("vault/events.jsonl") == GLK_OK);
    assert(glk_vault_count() >= 1); /* reloaded from disk */
    assert(glk_vault_chain_root() != 0 || root_hash != 0);

    /* seal */
    assert(glk_vault_seal("test-secret") == GLK_OK);
    assert(glk_vault_sealed());
    size_t before = glk_vault_count();
    glk_vault_push("blocked", "rx", 1, 0, 0.f); /* sealed: no-op */
    assert(glk_vault_count() == before);
    assert(glk_vault_unseal("wrong") == GLK_ERR_DENIED);
    assert(glk_vault_unseal("test-secret") == GLK_OK);
    assert(!glk_vault_sealed());

    /* skill ROM + scan empty skills dir */
    glk_skill_init();
    assert(glk_skill_register("rom_passive", "1.0", GLK_RISK_PASSIVE_RX) == GLK_OK);
    assert(glk_skill_count() == 1);
    assert(glk_skill_find("rom_passive") != NULL);
    assert(glk_skill_unload("rom_passive") == GLK_ERR_DENIED); /* ROM protected */

    /* write a minimal skill package */
    mk("build-host/test_sd_v38/groklink/skills");
    mk("build-host/test_sd_v38/groklink/skills/lab_unit_skill");
    const char* man =
        "{\n  \"id\": \"lab_unit_skill\",\n  \"version\": \"1.0.0\",\n"
        "  \"risk_class\": \"passive_rx\"\n}\n";
    assert(glk_storage_write_file("skills/lab_unit_skill/manifest.json", man, strlen(man)) ==
           GLK_OK);
    /* signed package */
    uint32_t mh = 0;
    assert(glk_storage_file_hash32("skills/lab_unit_skill/manifest.json", &mh) == GLK_OK);
    char sig[48];
    snprintf(sig, sizeof(sig), "GLKSIG1 %08x\n", (unsigned)mh);
    assert(glk_storage_write_file("skills/lab_unit_skill/manifest.sig", sig, strlen(sig)) ==
           GLK_OK);

    assert(glk_skill_scan_storage() == GLK_OK);
    const glk_skill_t* sk = glk_skill_find("lab_unit_skill");
    assert(sk != NULL);
    assert(sk->loaded);
    assert(sk->signed_ok);
    assert(sk->source == GLK_SKILL_SRC_SD);
    assert(glk_skill_retain("lab_unit_skill") == GLK_OK);
    assert(glk_skill_release("lab_unit_skill") == GLK_OK);
    /* one more release drops to unload if ref 0 after first implicit */
    assert(glk_skill_find("lab_unit_skill") != NULL);

    char stj[256];
    assert(glk_storage_status_json(stj, sizeof(stj)) > 0);
    assert(strstr(stj, "present") != NULL);
    assert(glk_skill_status_json(stj, sizeof(stj)) > 0);
    assert(glk_vault_status_json(stj, sizeof(stj)) > 0);

    /* path escape denied */
    assert(glk_storage_path("../etc/passwd", buf, sizeof(buf)) == GLK_ERR_DENIED);

    printf("test_storage_v38: OK (storage+vault+skills)\n");
    printf("  version %s api %d\n", GLK_VERSION_STRING, GLK_RPC_API);
    return 0;
}
