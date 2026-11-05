/*
 * No libcurl in the AmigaOS 3 build: this header gives src/common/WWW.cpp
 * the names it uses, and every transfer fails at once with "online
 * features are not in the Amiga build". Web levels, themes, highscores,
 * rooms, votes, reports and replay uploads are off (they are off by default
 * here too, XMSession_default.h), so the game never waits on the network.
 * The AmiCradle Team. GPL 2 or later, like X-Moto.
 */
#ifndef AMIGA_CURL_STUB_H
#define AMIGA_CURL_STUB_H

#include <stddef.h>

#define LIBCURL_VERSION_NUM 0x080000
#define CURL_VERSION_BITS(x, y, z) ((x) << 16 | (y) << 8 | (z))
#define CURL_AT_LEAST_VERSION(x, y, z) \
  (LIBCURL_VERSION_NUM >= CURL_VERSION_BITS(x, y, z))

#define CURL_ZERO_TERMINATED ((size_t)-1)

typedef void CURL;
typedef long long curl_off_t;
typedef struct curl_mime_s curl_mime;
typedef struct curl_mimepart_s curl_mimepart;

struct curl_slist {
  char *data;
  struct curl_slist *next;
};

struct curl_httppost {
  struct curl_httppost *next;
};

typedef enum {
  CURLE_OK = 0,
  CURLE_UNSUPPORTED_PROTOCOL = 1,
  CURLE_COULDNT_CONNECT = 7,
  CURLE_ABORTED_BY_CALLBACK = 42
} CURLcode;

typedef enum {
  CURLOPT_WRITEDATA = 10001,
  CURLOPT_URL = 10002,
  CURLOPT_PROXY = 10004,
  CURLOPT_PROXYUSERPWD = 10006,
  CURLOPT_TIMEOUT = 13,
  CURLOPT_USERAGENT = 10018,
  CURLOPT_HTTPHEADER = 10023,
  CURLOPT_HTTPPOST = 10024,
  CURLOPT_NOPROGRESS = 43,
  CURLOPT_FAILONERROR = 45,
  CURLOPT_FOLLOWLOCATION = 52,
  CURLOPT_PROGRESSDATA = 10057,
  CURLOPT_PROXYPORT = 59,
  CURLOPT_CONNECTTIMEOUT = 78,
  CURLOPT_NOSIGNAL = 99,
  CURLOPT_PROXYTYPE = 101,
  CURLOPT_ENCODING = 10102,
  CURLOPT_WRITEFUNCTION = 20011,
  CURLOPT_XFERINFOFUNCTION = 20219,
  CURLOPT_MIMEPOST = 10269
} CURLoption;

typedef enum {
  CURLFORM_NOTHING = 0,
  CURLFORM_COPYNAME = 1,
  CURLFORM_PTRCONTENTS = 5,
  CURLFORM_FILE = 10,
  CURLFORM_END = 17
} CURLformoption;

#define CURLPROXY_HTTP 0
#define CURLPROXY_SOCKS4 4
#define CURLPROXY_SOCKS5 5
#define CURLPROXY_SOCKS4A 6
#define CURLPROXY_SOCKS5_HOSTNAME 7

/* Something that is not NULL, for the calls whose result is checked. */
static char amiga_curl_dummy;

static inline CURL *curl_easy_init(void) { return (CURL *)&amiga_curl_dummy; }
static inline void curl_easy_cleanup(CURL *c) { (void)c; }
#define curl_easy_setopt(handle, opt, ...) ((void)(handle), (void)(opt), CURLE_OK)
static inline CURLcode curl_easy_perform(CURL *c) {
  (void)c;
  return CURLE_UNSUPPORTED_PROTOCOL;
}
static inline const char *curl_easy_strerror(CURLcode e) {
  return e == CURLE_OK ? "No error"
                       : "online features are not in the Amiga build";
}

static inline curl_mime *curl_mime_init(CURL *c) {
  (void)c;
  return (curl_mime *)&amiga_curl_dummy;
}
static inline void curl_mime_free(curl_mime *m) { (void)m; }
static inline curl_mimepart *curl_mime_addpart(curl_mime *m) {
  (void)m;
  return (curl_mimepart *)&amiga_curl_dummy;
}
static inline CURLcode curl_mime_name(curl_mimepart *p, const char *n) {
  (void)p; (void)n;
  return CURLE_OK;
}
static inline CURLcode curl_mime_filedata(curl_mimepart *p, const char *f) {
  (void)p; (void)f;
  return CURLE_OK;
}
static inline CURLcode curl_mime_data(curl_mimepart *p, const char *d, size_t s) {
  (void)p; (void)d; (void)s;
  return CURLE_OK;
}

#define curl_formadd(first, last, ...) ((void)(first), (void)(last), 0)
static inline void curl_formfree(struct curl_httppost *p) { (void)p; }

static inline struct curl_slist *curl_slist_append(struct curl_slist *l, const char *s) {
  (void)s;
  return l ? l : (struct curl_slist *)&amiga_curl_dummy;
}
static inline void curl_slist_free_all(struct curl_slist *l) { (void)l; }

#endif
