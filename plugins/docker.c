// vision_docker.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

const char *plugin_name = "Vision Docker";
const char *plugin_key  = "vision_docker";

static void trim(char *s)
{
    size_t len = strlen(s);

    while (len && (s[len - 1] == '\n' || s[len - 1] == '\r'))
        s[--len] = '\0';
}

void plugin_detect(char *out, size_t max_len)
{
    FILE *fp;
    char version[128] = "Unknown";
    char containers[64] = "?";
    char images[64] = "?";
    char line[256];

    /* Docker Version */
    fp = popen("docker version --format '{{.Server.Version}}' 2>/dev/null", "r");
    if (fp)
    {
        if (fgets(version, sizeof(version), fp))
            trim(version);
        pclose(fp);
    }

    /* Running Containers */
    fp = popen("docker ps -q | wc -l", "r");
    if (fp)
    {
        fgets(containers, sizeof(containers), fp);
        trim(containers);
        pclose(fp);
    }

    /* Images */
    fp = popen("docker images -q | wc -l", "r");
    if (fp)
    {
        fgets(images, sizeof(images), fp);
        trim(images);
        pclose(fp);
    }

    snprintf(
        out,
        max_len,
        "Docker %s | Containers: %s | Images: %s",
        version,
        containers,
        images
    );
}