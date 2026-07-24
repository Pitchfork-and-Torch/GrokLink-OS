/**
 * GrokLink OS 3.0 — global compile-time configuration and resource budgets.
 * From-scratch native OS (not a Flipper overlay).
 */
#pragma once

#define GLK_VERSION_MAJOR 3
#define GLK_VERSION_MINOR 7
#define GLK_VERSION_PATCH 1
#define GLK_VERSION_STRING "3.7.1"
#if defined(GLK_BUILD_RADIO)
#define GLK_CODENAME "GrokLink Field Research"
#else
#define GLK_CODENAME "GrokLink Field Research"
#endif

/** RPC API generation (bridge compatibility) */
#define GLK_RPC_API 5

/**
 * Personal / dedicated lab unit: power-on field research explorer.
 * edu-ack for passive only + all ROM passive missions autonomous + offline sticky.
 * Passive RX only; never auto-TX. Operator-owned authorized research device.
 */
#ifndef GLK_BOOT_FIELD_EXPLORE
#define GLK_BOOT_FIELD_EXPLORE 1
#endif

/**
 * If 1 with boot field explore: offline cannot stay disabled (re-arms).
 * Host may still observe over USB; unplug continues passive explore.
 */
#ifndef GLK_FIELD_EXPLORE_STICKY
#define GLK_FIELD_EXPLORE_STICKY 1
#endif

#define GL_EDU_ACK_PHRASE "I_WILL_USE_ONLY_AUTHORIZED_TARGETS"

/* ---- Paths (POSIX-style; host maps to workspace sd_card) ---- */
#define GLK_SD_ROOT "/groklink"
#define GLK_PATH_CONFIG GLK_SD_ROOT "/config/agent.json"
#define GLK_PATH_MISSIONS GLK_SD_ROOT "/missions"
#define GLK_PATH_SKILLS GLK_SD_ROOT "/skills"
#define GLK_PATH_LOGS GLK_SD_ROOT "/logs"
#define GLK_PATH_BLACKLIST GLK_SD_ROOT "/blacklist"
#define GLK_PATH_VAULT GLK_SD_ROOT "/vault"
#define GLK_PATH_STATE GLK_SD_ROOT "/state"
#define GLK_PATH_AUDIT GLK_PATH_LOGS "/audit.jsonl"

/* ---- Kernel ---- */
#ifndef GLK_MAX_TASKS
#define GLK_MAX_TASKS 16
#endif
#ifndef GLK_MAX_MUTEXES
#define GLK_MAX_MUTEXES 32
#endif
#ifndef GLK_MAX_QUEUES
#define GLK_MAX_QUEUES 16
#endif
#ifndef GLK_MAX_EVENTS
#define GLK_MAX_EVENTS 16
#endif
#ifndef GLK_MAX_TIMERS
#define GLK_MAX_TIMERS 24
#endif
#ifndef GLK_MAX_POOLS
#define GLK_MAX_POOLS 8
#endif
#define GLK_TICK_HZ 1000
#define GLK_TASK_NAME_MAX 16
#define GLK_DEFAULT_STACK 2048
#define GLK_IDLE_STACK 512

/* ---- Services / agent (expanded vs v2; override for device builds) ---- */
#ifndef GLK_MAX_MISSIONS
#define GLK_MAX_MISSIONS 32
#endif
#ifndef GLK_MAX_SKILLS
#define GLK_MAX_SKILLS 64
#endif
#ifndef GLK_MAX_MISSION_STEPS
#define GLK_MAX_MISSION_STEPS 64
#endif
#ifndef GLK_MAX_CONFIRM_SLOTS
#define GLK_MAX_CONFIRM_SLOTS 8
#endif
#define GLK_CONFIRM_DEFAULT_TTL_SEC 60
#define GLK_TX_MAX_MS 2000
#define GLK_TX_COOLDOWN_MS 5000
#define GLK_RX_COOLDOWN_MS 500
#define GLK_RX_DURATION_DEFAULT_MS 1000
#define GLK_RX_DURATION_MAX_MS 5000
#ifndef GLK_SPECTRUM_MAX_BANDS
#define GLK_SPECTRUM_MAX_BANDS 16
#endif
#define GLK_SPECTRUM_SETTLE_MS 2000
#define GLK_RADIO_FAULT_BREAKER 3

#define GLK_MISSION_ID_MAX 32
#define GLK_SKILL_ID_MAX 32
#define GLK_ACTION_MAX 32
#define GLK_AUDIT_LINE_MAX 512
#define GLK_REASON_MAX 96

/* ---- SubGHz policy window (CC1101-class) ---- */
#define GLK_SUBGHZ_FREQ_MIN_HZ 281000000u
#define GLK_SUBGHZ_FREQ_MAX_HZ 928000000u

/* ---- Memory arenas (bytes) ---- */
#ifndef GLK_HEAP_SIZE
#define GLK_HEAP_SIZE (24 * 1024)
#endif
#ifndef GLK_AGENT_ARENA_SIZE
#define GLK_AGENT_ARENA_SIZE (40 * 1024)
#endif
#ifndef GLK_RPC_BUF_SIZE
#define GLK_RPC_BUF_SIZE (12 * 1024)
#endif
#ifndef GLK_RADIO_BUF_SIZE
#define GLK_RADIO_BUF_SIZE (12 * 1024)
#endif
#ifndef GLK_ML_ARENA_SIZE
#define GLK_ML_ARENA_SIZE (24 * 1024)
#endif
#ifndef GLK_ML_MODEL_MAX
#define GLK_ML_MODEL_MAX (40 * 1024)
#endif

/* ---- Feature flags ---- */
#define GLK_FEATURE_STREAM 1
#define GLK_FEATURE_AUTONOMY 1
#define GLK_FEATURE_ML 1
#define GLK_FEATURE_SPECTRUM 1
#define GLK_FEATURE_IR 1
#define GLK_FEATURE_GPIO 1
#define GLK_FEATURE_NFC 1
#define GLK_FEATURE_BLE_STATUS 1
#define GLK_FEATURE_SIGNED_SKILLS 0 /* set 1 for release enforce */
#define GLK_FEATURE_SECURE_BOOT_HOOKS 1

/* ---- Priorities (higher number = higher priority) ---- */
#define GLK_PRIO_IDLE 0
#define GLK_PRIO_SHELL 1
#define GLK_PRIO_AUDIT 2
#define GLK_PRIO_AGENT 5
#define GLK_PRIO_RPC 8
#define GLK_PRIO_STORAGE 10
#define GLK_PRIO_DRIVER 13
#define GLK_PRIO_RADIO 16
#define GLK_PRIO_SAFETY 20
