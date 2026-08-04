// vision_docker.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <unistd.h>

const char *plugin_name = "Vision Docker";
const char *plugin_key  = "vision_docker";

static void trim(char *s)
{
    size_t len = strlen(s);

    while (len && (s[len - 1] == '\n' || s[len - 1] == '\r'))
        s[--len] = '\0';
}

/* Read one line of output from a shell command, closing the pipe (and thus
   reaping the child) whether or not the read succeeded. Returns 1 on a
   successful non-empty read, 0 otherwise. */
static int run_line(const char *cmd, char *out, size_t max_len)
{
    FILE *fp = popen(cmd, "r");
    if (!fp) return 0;

    int ok = 0;
    if (fgets(out, (int)max_len, fp) != NULL) {
        trim(out);
        ok = (out[0] != '\0');
    }
    pclose(fp);
    return ok;
}

#include <sys/socket.h>
#include <sys/un.h>

static int docker_socket_query(const char *endpoint, char *out_buf, size_t out_sz) {
    const char *sock_path = "/var/run/docker.sock";
    if (access(sock_path, F_OK) != 0) sock_path = "/run/docker.sock";
    if (access(sock_path, F_OK) != 0) return 0;

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return 0;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, sock_path, sizeof(addr.sun_path) - 1);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        close(fd);
        return 0;
    }

    char req[256];
    snprintf(req, sizeof(req), "GET %s HTTP/1.0\r\nHost: localhost\r\n\r\n", endpoint);
    if (write(fd, req, strlen(req)) < 0) {
        close(fd);
        return 0;
    }

    size_t bytes = 0;
    ssize_t n;
    while ((n = read(fd, out_buf + bytes, out_sz - 1 - bytes)) > 0) {
        bytes += (size_t)n;
        if (bytes >= out_sz - 1) break;
    }
    out_buf[bytes] = '\0';
    close(fd);

    char *body = strstr(out_buf, "\r\n\r\n");
    if (body) {
        body += 4;
        memmove(out_buf, body, strlen(body) + 1);
    }
    return 1;
}

static int count_json_array_items(const char *json) {
    if (!json) return 0;
    const char *p = strchr(json, '[');
    if (!p) return 0;
    p++;
    int count = 0;
    int in_obj = 0;
    while (*p && *p != ']') {
        if (*p == '{') {
            if (!in_obj) { count++; in_obj = 1; }
        } else if (*p == '}') {
            in_obj = 0;
        }
        p++;
    }
    return count;
}

void plugin_detect(char *out, size_t max_len)
{
    if (access("/var/run/docker.sock", F_OK) != 0 &&
        access("/run/docker.sock", F_OK) != 0) {
        snprintf(out, max_len, "Not running");
        return;
    }

    char version_buf[2048] = "";
    char containers_buf[4096] = "";
    char images_buf[4096] = "";

    char ver[64] = "Unknown";
    int container_count = 0;
    int image_count = 0;

    if (docker_socket_query("/version", version_buf, sizeof(version_buf))) {
        char *vp = strstr(version_buf, "\"Version\":\"");
        if (vp) {
            vp += 11;
            char *end = strchr(vp, '"');
            if (end && (size_t)(end - vp) < sizeof(ver)) {
                strncpy(ver, vp, end - vp);
                ver[end - vp] = '\0';
            }
        }
    }

    if (docker_socket_query("/containers/json", containers_buf, sizeof(containers_buf))) {
        container_count = count_json_array_items(containers_buf);
    }

    if (docker_socket_query("/images/json", images_buf, sizeof(images_buf))) {
        image_count = count_json_array_items(images_buf);
    }

    /* Fallback to CLI if socket query was unreadable */
    if (strcmp(ver, "Unknown") == 0 && container_count == 0 && image_count == 0) {
        if (!run_line("docker version --format '{{.Server.Version}}' 2>/dev/null", ver, sizeof(ver)))
            strcpy(ver, "Unknown");
        char c_str[32] = "0", i_str[32] = "0";
        if (run_line("docker ps -q | wc -l", c_str, sizeof(c_str))) container_count = atoi(c_str);
        if (run_line("docker images -q | wc -l", i_str, sizeof(i_str))) image_count = atoi(i_str);
    }

    snprintf(
        out,
        max_len,
        "Docker %s | Containers: %d | Images: %d",
        ver,
        container_count,
        image_count
    );
}