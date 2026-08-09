#if defined(__linux__) || defined(__gnu_linux__)

#include "platform.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#define SECURE_BOOT_ENABLED   1
#define SECURE_BOOT_DISABLED  0
#define SECURE_BOOT_UNKNOWN  -1

#define LOCKDOWN_ENABLED      1
#define LOCKDOWN_DISABLED     0

#define FW_ACTIVE             1
#define FW_INACTIVE           0
#define FW_UNKNOWN            -1

#define MAC_ACTIVE            1
#define MAC_INACTIVE          0

#define ASLR_ENABLED          2
#define ASLR_PARTIAL          1
#define ASLR_DISABLED         0
#define ASLR_UNKNOWN         -1

#define CORE_RESTRICTED       0
#define CORE_PIPED            1

typedef struct {
    int secure_boot;      /* 1=enabled, 0=disabled, -1=unknown */
    int lockdown;         /* 1=enabled, 0=disabled */
    int firewall;         /* 1=active, 0=inactive, -1=unknown (no root) */
    int mac;              /* 1=active (SELinux/AppArmor), 0=inactive */
    int aslr;             /* 2=enabled, 1=partial, 0=disabled, -1=unknown */
    int core_dumps;       /* 0=restricted, 1=piped/enabled */
    int open_ports;       /* number of listening ports */
} SecurityAudit;

static int path_exists(const char *path) {
    return access(path, F_OK) == 0;
}

static int read_first_byte(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    unsigned char byte = 0;
    if (fread(&byte, 1, 1, f) != 1) {
        fclose(f);
        return -1;
    }
    fclose(f);
    return (int)byte;
}

/* EFI variables have a 4-byte attributes header before the value */
static int read_efi_var(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    unsigned char buf[5] = {0};
    if (fread(buf, 1, 5, f) != 5) {
        fclose(f);
        return -1;
    }
    fclose(f);
    return (int)buf[4];
}

static int read_proc_int(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    int val = -1;
    if (fscanf(f, "%d", &val) != 1) val = -1;
    fclose(f);
    return val;
}

/*
 * /proc/net/tcp and /proc/net/udp store IPv4 addresses as an 8-hex-digit
 * little-endian word: for address a.b.c.d the string is "%02X%02X%02X%02X"
 * of (d, c, b, a) — i.e. the FIRST octet 'a' ends up as the LAST byte pair
 * in the hex string. Examples:
 *   127.0.0.1   -> "0100007F"  (last pair "7F" = 127)
 *   127.0.0.53  -> "3500007F"  (last pair "7F" = 127)
 *   127.0.2.2   -> "0202007F"  (last pair "7F" = 127)
 *   10.0.3.1    -> "0103000A"  (last pair "0A" = 10, not loopback)
 *
 * The entire 127.0.0.0/8 range is loopback, not just 127.0.0.1 — and
 * systemd-resolved in particular listens on several distinct addresses in
 * that range (127.0.0.53, 127.0.0.54, 127.0.2.2, 127.0.2.3, ...) for its
 * stub resolvers. Matching only the exact string "0100007F" (127.0.0.1)
 * missed all of those, silently over-counting "open" ports that are in
 * fact loopback-only. Checking the last byte pair for "7F" instead covers
 * the whole 127.0.0.0/8 range correctly.
 */
static int is_loopback_addr(const char *local_address_field) {
    /* local_address_field looks like "0100007F:0050" (ADDR:PORT). We only
     * need the address part before the colon. */
    char addr[16] = "";
    const char *colon = strchr(local_address_field, ':');
    size_t len = colon ? (size_t)(colon - local_address_field) : strlen(local_address_field);
    if (len >= sizeof(addr)) len = sizeof(addr) - 1;
    memcpy(addr, local_address_field, len);
    addr[len] = '\0';

    if (strlen(addr) != 8) return 0; /* not a well-formed IPv4 hex address */
    return strcasecmp(addr + 6, "7F") == 0;
}

/*
 * Count listening sockets in /proc/net/tcp or /proc/net/udp that are bound
 * to a non-loopback address (i.e. actually reachable from outside the
 * machine, either via 0.0.0.0 or a specific external interface). Sockets
 * bound only to 127.0.0.1 (local-only services like some DNS resolvers or
 * dev servers) are not a meaningful attack surface and are excluded so the
 * count reflects real exposure rather than every socket on the box.
 *
 * Column layout (whitespace separated):
 *   1: sl
 *   2: local_address   <-- "ADDR:PORT" hex, checked for loopback
 *   3: rem_address
 *   4: st              <-- connection state, "0A" = TCP_LISTEN
 *   5: tx_queue:rx_queue
 *   6: tr:tm->when
 *   ...
 *
 * NOTE: previously this skipped 5 fields (%*s x5) and captured field 6
 * (tx_queue:rx_queue, e.g. "00000000:00000000") into `state` instead of
 * field 4 (the actual st column). strcmp(state, "0A") then almost never
 * matched, so open_ports was silently stuck at 0 regardless of what was
 * actually listening. Fixed by skipping only 2 fields before capturing
 * local_address, then 1 more before capturing the state column.
 */
static int count_listening(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;

    char line[256];
    int count = 0;

    /* skip header line */
    if (!fgets(line, sizeof(line), f)) {
        fclose(f);
        return 0;
    }

    while (fgets(line, sizeof(line), f)) {
        char local_addr[32] = "";
        char state[8] = "";
        if (sscanf(line, "%*s %31s %*s %7s", local_addr, state) == 2) {
            if (strcmp(state, "0A") == 0 && !is_loopback_addr(local_addr))
                count++;
        }
    }

    fclose(f);
    return count;
}

static void security_gather(SecurityAudit *a) {
    /* Secure Boot */
    a->secure_boot = read_efi_var("/sys/firmware/efi/efivars/SecureBoot-8be4df61-93ca-11d2-aa0d-00e098032b8c");

    /* Kernel lockdown */
    a->lockdown = LOCKDOWN_DISABLED;
    if (path_exists("/sys/kernel/security/lockdown")) {
        FILE *f = fopen("/sys/kernel/security/lockdown", "r");
        if (f) {
            char line[256];
            if (fgets(line, sizeof(line), f))
                if (strstr(line, "[integrity]") || strstr(line, "[confidentiality]"))
                    a->lockdown = LOCKDOWN_ENABLED;
            fclose(f);
        }
    }

    /*
     * Firewall — try systemd first (no root needed), then iptables/nft.
     * iptables -L and nft list ruleset typically require root/CAP_NET_ADMIN;
     * without it the command can fail silently and we'd previously report
     * a false "Disabled". Now: if none of the systemd checks find an active
     * service AND we're not root, report FW_UNKNOWN instead of guessing.
     */
    a->firewall = FW_INACTIVE;
    char fw_buf[64] = "";
    int fw_found_active = 0;
    const char *fw_services[] = { "ufw", "firewalld", "iptables", "nftables", NULL };
    for (int i = 0; fw_services[i] && !fw_found_active; i++) {
        char cmd[128];
        snprintf(cmd, sizeof(cmd), "systemctl is-active %s 2>/dev/null", fw_services[i]);
        fw_buf[0] = '\0';
        if (util_execute_cmd(cmd, fw_buf, sizeof(fw_buf)) == 0)
            if (strncmp(fw_buf, "active", 6) == 0) fw_found_active = 1;
    }

    if (fw_found_active) {
        a->firewall = FW_ACTIVE;
    } else if (geteuid() != 0) {
        /* Can't reliably probe iptables/nft rule counts without root —
         * don't claim "inactive" when we simply couldn't check. */
        a->firewall = FW_UNKNOWN;
    } else {
        fw_buf[0] = '\0';
        if (util_execute_cmd("iptables -L -n 2>/dev/null | tail -n +3 | grep -c .", fw_buf, sizeof(fw_buf)) == 0)
            if (atoi(fw_buf) > 0) a->firewall = FW_ACTIVE;
        if (a->firewall != FW_ACTIVE) {
            fw_buf[0] = '\0';
            if (util_execute_cmd("nft list ruleset 2>/dev/null | grep -c 'rule'", fw_buf, sizeof(fw_buf)) == 0)
                if (atoi(fw_buf) > 0) a->firewall = FW_ACTIVE;
        }
    }

    /*
     * SELinux / AppArmor (Mandatory Access Control).
     *
     * SELinux: /sys/fs/selinux/enforce is world-readable on distros that ship
     * it and holds '1' while enforcing, '0' while permissive.
     *
     * AppArmor: previously this only tried /sys/kernel/security/apparmor/profiles,
     * which lists every loaded profile — but that file is root-only (mode 0400)
     * on Ubuntu/Debian. A normal user's fopen() hits EACCES, returns NULL, and
     * the code silently fell through to MAC_INACTIVE even on a stock Ubuntu
     * install where AppArmor is enabled by default and enforcing dozens of
     * profiles. That produced a false "Not active" whenever nexfetch ran
     * without root.
     *
     * Fix: check /sys/module/apparmor/parameters/enabled first. This is a
     * kernel-exposed flag ('Y' or 'N'), world-readable, and requires no
     * profile enumeration — it directly answers "is AppArmor enabled in this
     * kernel" without needing root. The profiles file is still tried as a
     * fallback (useful when actually running as root, since it also confirms
     * profiles are loaded, not just that the LSM is compiled in) but is no
     * longer required for a correct answer.
     */
    a->mac = MAC_INACTIVE;
    if (path_exists("/sys/fs/selinux/enforce")) {
        if (read_first_byte("/sys/fs/selinux/enforce") == 1) a->mac = MAC_ACTIVE;
    }
    if (!a->mac && path_exists("/sys/module/apparmor/parameters/enabled")) {
        FILE *f = fopen("/sys/module/apparmor/parameters/enabled", "r");
        if (f) {
            char c = 0;
            if (fread(&c, 1, 1, f) == 1 && (c == 'Y' || c == 'y'))
                a->mac = MAC_ACTIVE;
            fclose(f);
        }
    }
    if (!a->mac && path_exists("/sys/kernel/security/apparmor/profiles")) {
        FILE *f = fopen("/sys/kernel/security/apparmor/profiles", "r");
        if (f) {
            char line[256];
            if (fgets(line, sizeof(line), f)) a->mac = MAC_ACTIVE;
            fclose(f);
        }
    }

    /* ASLR */
    a->aslr = ASLR_UNKNOWN;
    if (path_exists("/proc/sys/kernel/randomize_va_space"))
        a->aslr = read_proc_int("/proc/sys/kernel/randomize_va_space");

    /* Core dumps */
    a->core_dumps = CORE_RESTRICTED;
    if (path_exists("/proc/sys/kernel/core_pattern")) {
        FILE *f = fopen("/proc/sys/kernel/core_pattern", "r");
        if (f) {
            char line[256];
            if (fgets(line, sizeof(line), f) && (line[0] == '|' || (line[0] != '\0' && line[0] != '\n')))
                a->core_dumps = CORE_PIPED;
            fclose(f);
        }
    }

    /* Open ports (TCP + UDP listening) */
    a->open_ports = count_listening("/proc/net/tcp") + count_listening("/proc/net/udp");
}

typedef struct {
    int score;        /* passed checks */
    int verified;     /* checks that could be determined (excludes unknown) */
    int unavailable;  /* checks that returned unknown/not found */
} SecurityScore;

static SecurityScore security_score(const SecurityAudit *a) {
    SecurityScore s = {0, 0, 0};

    if (a->secure_boot == -1)      s.unavailable++;
    else { s.verified++; if (a->secure_boot == 1) s.score++; }

    if (a->lockdown == -1)         s.unavailable++;
    else { s.verified++; if (a->lockdown == 1) s.score++; }

    if (a->firewall == FW_UNKNOWN) s.unavailable++;
    else { s.verified++; if (a->firewall == 1) s.score++; }

    if (a->mac == -1)              s.unavailable++;
    else { s.verified++; if (a->mac == 1) s.score++; }

    if (a->aslr == -1)             s.unavailable++;
    else { s.verified++; if (a->aslr == 2) s.score++; }

    if (a->core_dumps == -1)       s.unavailable++;
    else { s.verified++; if (a->core_dumps == 0) s.score++; }

    /* open_ports is a count, never "unknown" — 0 or more is always a
     * determinable result once /proc/net/{tcp,udp} parsing runs. */
    s.verified++;
    if (a->open_ports == 0) s.score++;

    return s;
}

/* Compact mode: "Security: 4/6 verified [GOOD]" */
void platform_get_security(char *out, size_t size) {
    if (!out || size == 0) return;

    SecurityAudit a;
    security_gather(&a);

    SecurityScore s = security_score(&a);
    const char *label;
    if (s.unavailable == 0 && s.score == 7) label = "\033[32mEXCELLENT\033[0m";
    else if (s.score >= 5)                  label = "\033[32mGOOD\033[0m";
    else if (s.score >= 3)                  label = "\033[33mFAIR\033[0m";
    else                                    label = "\033[31mPOOR\033[0m";

    if (s.unavailable > 0)
        snprintf(out, size, "%d/%d verified [%s]", s.score, s.verified, label);
    else
        snprintf(out, size, "%d/7 [%s]", s.score, label);
}

/* Detailed audit mode: standalone table */
void platform_security_audit(void) {
    SecurityAudit a;
    security_gather(&a);
    SecurityScore s = security_score(&a);

    printf("\n\033[1mNexfetch Security Audit\033[0m\n\n");

    /* Secure Boot */
    if (a.secure_boot == 1)
        printf("  \033[32m\xe2\x9c\x93\033[0m Secure Boot       \033[32mEnabled\033[0m\n");
    else if (a.secure_boot == 0)
        printf("  \033[31m\xe2\x9c\x97\033[0m Secure Boot       \033[31mDisabled\033[0m\n");
    else
        printf("  \033[33m?\033[0m Secure Boot       \033[33mNot found\033[0m\n");

    /* Kernel Lockdown */
    if (a.lockdown == 1)
        printf("  \033[32m\xe2\x9c\x93\033[0m Kernel Lockdown   \033[32mEnabled\033[0m\n");
    else
        printf("  \033[31m\xe2\x9c\x97\033[0m Kernel Lockdown   \033[31mDisabled\033[0m\n");

    /* Firewall */
    if (a.firewall == FW_ACTIVE)
        printf("  \033[32m\xe2\x9c\x93\033[0m Firewall          \033[32mActive\033[0m\n");
    else if (a.firewall == FW_UNKNOWN)
        printf("  \033[33m?\033[0m Firewall          \033[33mUnknown (run as root to check)\033[0m\n");
    else
        printf("  \033[31m\xe2\x9c\x97\033[0m Firewall          \033[31mDisabled\033[0m\n");

    /* MAC */
    if (a.mac == 1)
        printf("  \033[32m\xe2\x9c\x93\033[0m MAC               \033[32mActive\033[0m\n");
    else
        printf("  \033[31m\xe2\x9c\x97\033[0m MAC               \033[31mNot active\033[0m\n");

    /* ASLR */
    if (a.aslr == 2)
        printf("  \033[32m\xe2\x9c\x93\033[0m ASLR              \033[32mEnabled\033[0m\n");
    else if (a.aslr == 1)
        printf("  \033[33m?\033[0m ASLR              \033[33mPartial\033[0m\n");
    else
        printf("  \033[31m\xe2\x9c\x97\033[0m ASLR              \033[31mDisabled\033[0m\n");

    /* Core Dumps */
    if (a.core_dumps == 0)
        printf("  \033[32m\xe2\x9c\x93\033[0m Core Dumps        \033[32mRestricted\033[0m\n");
    else
        printf("  \033[33m\xe2\x9a\xa0\033[0m Core Dumps        \033[33mEnabled (piped)\033[0m\n");

    /* Open Ports */
    if (a.open_ports == 0)
        printf("  \033[32m\xe2\x9c\x93\033[0m Open Ports        \033[32m%d\033[0m\n", a.open_ports);
    else
        printf("  \033[33m\xe2\x9a\xa0\033[0m Open Ports        \033[33m%d\033[0m\n", a.open_ports);

    printf("\n  \033[1mSecurity Score:\033[0m ");
    if (s.unavailable > 0)
        printf("\033[1m%d/%d verified\033[0m", s.score, s.verified);
    else
        printf("\033[1m%d/7\033[0m", s.score);
    if (s.unavailable > 0)
        printf("  \033[33m\xe2\x9a\xa0 %d check unavailable\033[0m", s.unavailable);
    printf("\n\n");
}

#endif