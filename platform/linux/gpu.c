#if defined(__linux__) || defined(__gnu_linux__)

#include "platform.h"
#include "util.h"
#include <stdio.h>
#include <string.h>

#include <dirent.h>

static int get_gpu_sysfs(char *out, size_t size) {
    DIR *dir = opendir("/sys/bus/pci/devices");
    if (!dir) return 0;

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (ent->d_name[0] == '.') continue;
        char uevent_path[512];
        snprintf(uevent_path, sizeof(uevent_path), "/sys/bus/pci/devices/%s/uevent", ent->d_name);
        FILE *f = fopen(uevent_path, "r");
        if (!f) continue;

        char line[256];
        int is_gpu = 0;
        char pci_id[64] = "";
        char driver[64] = "";

        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "PCI_CLASS=30000", 15) == 0 ||
                strncmp(line, "PCI_CLASS=30200", 15) == 0 ||
                strncmp(line, "PCI_CLASS=38000", 15) == 0) {
                is_gpu = 1;
            }
            if (strncmp(line, "PCI_ID=", 7) == 0) {
                snprintf(pci_id, sizeof(pci_id), "%s", util_trim(line + 7));
            }
            if (strncmp(line, "DRIVER=", 7) == 0) {
                snprintf(driver, sizeof(driver), "%s", util_trim(line + 7));
            }
        }
        fclose(f);

        if (is_gpu && pci_id[0] != '\0') {
            char vendor_name[64] = "Unknown Vendor";
            if (strncmp(pci_id, "8086", 4) == 0) strcpy(vendor_name, "Intel Corporation");
            else if (strncmp(pci_id, "10de", 4) == 0 || strncmp(pci_id, "10DE", 4) == 0) strcpy(vendor_name, "NVIDIA Corporation");
            else if (strncmp(pci_id, "1002", 4) == 0) strcpy(vendor_name, "AMD / ATI");
            else if (strncmp(pci_id, "15ad", 4) == 0 || strncmp(pci_id, "15AD", 4) == 0) strcpy(vendor_name, "VMware SVGA");
            else if (strncmp(pci_id, "80ee", 4) == 0 || strncmp(pci_id, "80EE", 4) == 0) strcpy(vendor_name, "VirtualBox Graphics");
            else if (strncmp(pci_id, "1b36", 4) == 0 || strncmp(pci_id, "1B36", 4) == 0) strcpy(vendor_name, "QEMU VirtIO GPU");

            /* Try reading sysfs label or device name if present */
            char label_path[512], label_str[128] = "";
            snprintf(label_path, sizeof(label_path), "/sys/bus/pci/devices/%s/label", ent->d_name);
            if (util_read_first_line(label_path, label_str, sizeof(label_str)) && label_str[0] != '\0') {
                snprintf(out, size, "%s %s", vendor_name, label_str);
            } else if (driver[0] != '\0') {
                snprintf(out, size, "%s [%s] (%s)", vendor_name, pci_id, driver);
            } else {
                snprintf(out, size, "%s [%s]", vendor_name, pci_id);
            }
            closedir(dir);
            return 1;
        }
    }
    closedir(dir);
    return 0;
}

void platform_get_gpu(char *out, size_t size) {
    if (!out || size == 0) return;

    if (get_gpu_sysfs(out, size)) {
        return;
    }

    FILE *fp = popen("lspci 2>/dev/null | grep -E 'VGA|3D|Display' | head -n 1", "r");
    if (!fp) {
        snprintf(out, size, "Unknown");
        return;
    }

    char line[512] = "";
    if (fgets(line, sizeof(line), fp) == NULL) {
        pclose(fp);
        snprintf(out, size, "Unknown");
        return;
    }
    pclose(fp);
    util_trim(line);

    char *name = NULL;
    char *p = line;
    while (*p) {
        if (p[0] == ':' && p[1] == ' ') {
            name = p + 2;
        }
        p++;
    }

    if (!name || *name == '\0') {
        snprintf(out, size, "%s", line);
        return;
    }

    char *rev = strstr(name, " (rev ");
    if (rev) *rev = '\0';

    snprintf(out, size, "%s", name);
}

#endif

