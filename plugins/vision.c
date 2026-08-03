// vision.c — Plugin Vision: hiển thị thông tin camera/webcam trên hệ thống
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <dirent.h>

const char *plugin_name = "Vision Camera";
const char *plugin_key  = "visioncamera";

static void trim(char *s)
{
    size_t len = strlen(s);
    while (len && (s[len - 1] == '\n' || s[len - 1] == '\r'))
        s[--len] = '\0';
}

/* Đếm số thiết bị video trong /sys/class/video4linux/ */
static int count_video_devices(void)
{
    DIR *dir = opendir("/sys/class/video4linux");
    if (!dir) return 0;

    int count = 0;
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (strncmp(ent->d_name, "video", 5) == 0)
            count++;
    }
    closedir(dir);
    return count;
}

/* Lấy tên camera đầu tiên từ /sys/class/video4linux/videoX/name */
static void get_first_camera_name(char *out, size_t max_len)
{
    DIR *dir = opendir("/sys/class/video4linux");
    if (!dir) {
        snprintf(out, max_len, "Unknown");
        return;
    }

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (strncmp(ent->d_name, "video", 5) != 0) continue;

        char path[512];
        snprintf(path, sizeof(path), "/sys/class/video4linux/%s/name", ent->d_name);

        FILE *fp = fopen(path, "r");
        if (fp) {
            if (fgets(out, (int)max_len, fp))
                trim(out);
            fclose(fp);
            closedir(dir);
            return;
        }
    }
    closedir(dir);
    snprintf(out, max_len, "Unknown");
}

void plugin_detect(char *out, size_t max_len)
{
    int count = count_video_devices();
    char name[128] = "Unknown";

    if (count > 0)
        get_first_camera_name(name, sizeof(name));

    if (count <= 0) {
        snprintf(out, max_len, "No camera detected");
        return;
    }

    if (count == 1) {
        snprintf(out, max_len, "%s (1 camera)", name);
    } else {
        snprintf(out, max_len, "%s (+%d more cameras)", name, count - 1);
    }
}