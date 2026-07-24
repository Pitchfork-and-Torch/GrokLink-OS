/**
 * GrokLink OS boot — host simulation entry.
 * Boots kernel, policy, storage, radio worker, agent, RPC TCP server.
 */
#include "glk/glk_kernel.h"
#include "glk/glk_config.h"
#include "glk_svc/glk_policy.h"
#include "glk_svc/glk_audit.h"
#include "glk_svc/glk_agent.h"
#include "glk_svc/glk_rpc.h"
#include "glk_svc/glk_skill.h"
#include "glk_svc/glk_catalog.h"
#include "glk_svc/glk_storage.h"
#include "glk_svc/glk_power.h"
#include "glk_svc/glk_ml.h"
#include "glk_drv/glk_radio.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#define SOCKET int
#define INVALID_SOCKET (-1)
#define closesocket close
#endif

static glk_policy_state_t g_policy;
static glk_agent_t g_agent;
static glk_rpc_t g_rpc;
static char g_sd[512];

static void resolve_sd_root(char* out, size_t n) {
    const char* env = getenv("GLK_SD_ROOT");
    if (env && env[0]) {
        strncpy(out, env, n - 1);
        return;
    }
    /* default: ./sd_card/groklink relative to cwd */
    strncpy(out, "sd_card/groklink", n - 1);
}

static void rpc_client_task(void* arg) {
    SOCKET cs = (SOCKET)(uintptr_t)arg;
    char line[2048];
    char resp[2048];
    size_t len = 0;
    printf("[rpc] client connected\n");
    for (;;) {
        char ch;
        int r;
#ifdef _WIN32
        r = recv(cs, &ch, 1, 0);
#else
        r = (int)read(cs, &ch, 1);
#endif
        if (r <= 0) break;
        if (ch == '\n' || ch == '\r') {
            if (len == 0) continue;
            line[len] = 0;
            glk_rpc_handle_json(&g_rpc, line, resp, sizeof(resp));
            size_t rl = strlen(resp);
            char out[2100];
            snprintf(out, sizeof(out), "%s\n", resp);
#ifdef _WIN32
            send(cs, out, (int)strlen(out), 0);
#else
            write(cs, out, strlen(out));
#endif
            (void)rl;
            len = 0;
            continue;
        }
        if (len + 1 < sizeof(line)) line[len++] = ch;
    }
#ifdef _WIN32
    closesocket(cs);
#else
    close(cs);
#endif
    printf("[rpc] client disconnected\n");
}

static void rpc_server_task(void* arg) {
    (void)arg;
    int port = 7341;
    const char* ep = getenv("GLK_RPC_PORT");
    if (ep) port = atoi(ep);

#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
    SOCKET ls = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (ls == INVALID_SOCKET) {
        printf("[rpc] socket failed\n");
        return;
    }
    int yes = 1;
    setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(yes));
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((u_short)port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(ls, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        printf("[rpc] bind failed port %d\n", port);
        closesocket(ls);
        return;
    }
    listen(ls, 4);
    printf("[rpc] GrokLink OS RPC listening on 127.0.0.1:%d\n", port);
    printf("[rpc] send JSON lines, e.g. {\"cmd\":\"ping\"}\n");

    while (glk_kernel_running()) {
        struct sockaddr_in cli;
        int clen = sizeof(cli);
#ifdef _WIN32
        SOCKET cs = accept(ls, (struct sockaddr*)&cli, &clen);
#else
        SOCKET cs = accept(ls, (struct sockaddr*)&cli, (socklen_t*)&clen);
#endif
        if (cs == INVALID_SOCKET) {
            glk_task_sleep_ms(50);
            continue;
        }
        glk_task_t* t = NULL;
        glk_task_create(&t, "rpc_cli", rpc_client_task, (void*)(uintptr_t)cs, GLK_PRIO_RPC, 2048);
    }
    closesocket(ls);
}

static void shell_task(void* arg) {
    (void)arg;
    printf("\n");
    printf("============================================================\n");
    printf("  GrokLink OS %s — %s\n", GLK_VERSION_STRING, GLK_CODENAME);
    printf("  FROM-SCRATCH research OS (not a Flipper overlay)\n");
    printf("  Authorized educational / research use only.\n");
    printf("  NOT A MEDICAL DEVICE. No clinical / care use.\n");
    printf("============================================================\n");
    printf("SD root: %s (present=%s)\n", g_sd, glk_storage_present() ? "yes" : "no");
    printf("Heap free: %u bytes\n", (unsigned)glk_heap_free_bytes());
    printf("Missions: %u  Skills: %u\n",
           (unsigned)g_agent.mission_count,
           (unsigned)glk_skill_count());
    printf("Type is not available on device shell in host build; use RPC TCP.\n");
    printf("Ctrl+C or set GLK_RUN_MS to auto-stop.\n\n");

    const char* run_ms = getenv("GLK_RUN_MS");
    if (run_ms) {
        int ms = atoi(run_ms);
        glk_task_sleep_ms((uint32_t)(ms > 0 ? ms : 3000));
        glk_kernel_stop();
    }
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    resolve_sd_root(g_sd, sizeof(g_sd));

    glk_kernel_init();
    glk_power_init();
    glk_ml_init();
    glk_skill_init();

    glk_policy_init(&g_policy);
    glk_policy_set_global(&g_policy);

    if (glk_storage_init(g_sd) == GLK_OK) {
        glk_policy_set_sd_present(&g_policy, true);
        glk_policy_reload_blacklist(&g_policy, g_sd);
        char skills[560];
        snprintf(skills, sizeof(skills), "%s/skills", g_sd);
        glk_skill_scan(skills);

        char audit[560];
        snprintf(audit, sizeof(audit), "%s/logs/audit.jsonl", g_sd);
        glk_audit_init(audit);

        /* load missions */
        glk_agent_init(&g_agent, &g_policy);
        char mpath[560];
#ifdef _WIN32
        snprintf(mpath, sizeof(mpath), "%s\\missions", g_sd);
        char pattern[600];
        snprintf(pattern, sizeof(pattern), "%s\\*.json", mpath);
        WIN32_FIND_DATAA fd;
        HANDLE h = FindFirstFileA(pattern, &fd);
        if (h != INVALID_HANDLE_VALUE) {
            do {
                char full[600];
                snprintf(full, sizeof(full), "%s\\%s", mpath, fd.cFileName);
                glk_agent_load_mission_file(&g_agent, full);
            } while (FindNextFileA(h, &fd));
            FindClose(h);
        }
#else
        /* host non-win: try known files */
        snprintf(mpath, sizeof(mpath), "%s/missions/lab_passive_433.mission.json", g_sd);
        glk_agent_load_mission_file(&g_agent, mpath);
#endif
    } else {
        glk_agent_init(&g_agent, &g_policy);
        glk_audit_init("audit_host.jsonl");
        printf("[boot] SD root missing — degraded mode + ROM catalog\n");
    }

    /* Always ensure ROM passive catalog is present (fills gaps if SD partial). */
    (void)glk_catalog_load_defaults(&g_agent);

    glk_agent_set_global(&g_agent);
    glk_radio_init(&g_policy);
    glk_radio_start_worker();
    glk_agent_start_task(&g_agent);
    glk_rpc_init(&g_rpc, &g_policy, &g_agent, g_sd);

    glk_task_t* sh = NULL;
    glk_task_t* rp = NULL;
    glk_task_create(&sh, "shell", shell_task, NULL, GLK_PRIO_SHELL, 1024);
    glk_task_create(&rp, "rpc_srv", rpc_server_task, NULL, GLK_PRIO_RPC, 2048);

    glk_kernel_start();
    glk_radio_stop();
    glk_agent_stop(&g_agent);
    printf("[boot] GrokLink OS halted cleanly.\n");
    return 0;
}
