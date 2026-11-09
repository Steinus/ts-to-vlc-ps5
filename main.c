// vlc-bridge: tiny HTTP bridge on the PS5 that launches VLC PS5 (PPSA85300)
// and hands it a stream URL. Also serves the Lampa plugin at /lampa.js.
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "plugin.h" // generated from plugin.js (plugin_js, plugin_js_len)

#define PORT 8765
#define TITLE_ID "PPSA85300"
#define AUTOPLAY_FILE "/data/homebrew/PPSA85300/autoplay.txt"

typedef struct {
  uint32_t size;
  int32_t userId;
  int32_t appAttr;
  int32_t enableCrashReport;
  int32_t checkFlag;
} LaunchAppParam;

int sceUserServiceInitialize(void *);
int sceUserServiceGetForegroundUser(int *);
int sceSystemServiceLaunchApp(const char *, char **, LaunchAppParam *);

static void send_resp(int fd, int code, const char *ctype, const char *body,
                      size_t len) {
  char hdr[256];
  int n = snprintf(hdr, sizeof(hdr),
                   "HTTP/1.1 %d OK\r\nContent-Type: %s\r\n"
                   "Content-Length: %zu\r\nAccess-Control-Allow-Origin: *\r\n"
                   "Access-Control-Allow-Headers: *\r\nConnection: close\r\n\r\n",
                   code, ctype, len);
  write(fd, hdr, n);
  if (len) write(fd, body, len);
}

static int hexv(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

static void url_decode(const char *src, size_t n, char *dst, size_t cap) {
  size_t o = 0;
  for (size_t i = 0; i < n && o + 1 < cap; i++) {
    if (src[i] == '%' && i + 2 < n && hexv(src[i + 1]) >= 0 &&
        hexv(src[i + 2]) >= 0) {
      dst[o++] = (char)(hexv(src[i + 1]) * 16 + hexv(src[i + 2]));
      i += 2;
    } else if (src[i] == '+') {
      dst[o++] = ' ';
    } else {
      dst[o++] = src[i];
    }
  }
  dst[o] = 0;
}

// get query parameter `key` from the request line into out
static int get_param(const char *req, const char *key, char *out, size_t cap) {
  char pat[32];
  snprintf(pat, sizeof(pat), "%s=", key);
  const char *eol = strstr(req, "\r\n");
  const char *q = strchr(req, '?');
  if (!q || (eol && q > eol)) return 0;
  const char *p = strstr(q, pat);
  if (!p || (eol && p > eol)) return 0;
  p += strlen(pat);
  const char *e = p;
  while (*e && *e != '&' && *e != ' ' && *e != '\r') e++;
  url_decode(p, (size_t)(e - p), out, cap);
  return 1;
}

static int launch_vlc(void) {
  int uid = -1;
  sceUserServiceGetForegroundUser(&uid);
  LaunchAppParam param = {sizeof(LaunchAppParam), uid, 0, 0, 0};
  char *argv[] = {NULL};
  return sceSystemServiceLaunchApp(TITLE_ID, argv, &param);
}

static void handle(int fd) {
  char req[8192];
  ssize_t n = read(fd, req, sizeof(req) - 1);
  if (n <= 0) return;
  req[n] = 0;

  if (!strncmp(req, "OPTIONS ", 8)) {
    send_resp(fd, 204, "text/plain", "", 0);
  } else if (!strncmp(req, "GET /ping", 9)) {
    send_resp(fd, 200, "text/plain", "ok", 2);
  } else if (!strncmp(req, "GET /lampa.js", 13)) {
    send_resp(fd, 200, "application/javascript", (const char *)plugin_js,
              plugin_js_len);
  } else if (!strncmp(req, "GET /play", 9)) {
    char url[4096];
    if (!get_param(req, "url", url, sizeof(url)) || strncmp(url, "http", 4)) {
      send_resp(fd, 400, "text/plain", "bad url", 7);
      return;
    }
    FILE *f = fopen(AUTOPLAY_FILE, "w");
    if (f) {
      fprintf(f, "%s\n", url);
      fclose(f);
    }
    int r = launch_vlc();
    char msg[64];
    int m = snprintf(msg, sizeof(msg), "launch=%d file=%d", r, f ? 1 : 0);
    send_resp(fd, r == 0 ? 200 : 500, "text/plain", msg, m);
  } else {
    send_resp(fd, 404, "text/plain", "not found", 9);
  }
}

int main(void) {
  signal(SIGPIPE, SIG_IGN);
  sceUserServiceInitialize(0);

  int srv = socket(AF_INET, SOCK_STREAM, 0);
  int one = 1;
  setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
  struct sockaddr_in a = {0};
  a.sin_family = AF_INET;
  a.sin_addr.s_addr = htonl(INADDR_ANY);
  a.sin_port = htons(PORT);
  if (bind(srv, (struct sockaddr *)&a, sizeof(a)) || listen(srv, 8)) return 1;

  for (;;) {
    int c = accept(srv, NULL, NULL);
    if (c < 0) continue;
    handle(c);
    close(c);
  }
}
