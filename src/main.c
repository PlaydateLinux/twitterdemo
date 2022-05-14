//
//  main.c
//  Extension
//
//  Created by Dave Hayden on 7/30/14.
//  Copyright (c) 2014 Panic, Inc. All rights reserved.
//

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#ifdef __APPLE__
#include <dlfcn.h>
#endif

#include "pd_api.h"

struct PlaydateWifiHandle;
typedef struct PlaydateWifiHandle PlaydateWifiHandle;

typedef bool (*PlaydateHttpCallback)(int http_status, void* buf, size_t length,
                                     size_t length2, void* callback_data);

struct PlaydateWifiAPI {
  /**
   * Opens an SSL connection to the given hostname.
   * SSL certificates are verified if hostname is a subdomain of panic.com or
   * play.date, otherwise SSL certificates are IGNORED!!!!!
   * Returns a handle (always 0 in 1.11) on success, and -1 on failure.
   */
  int (*open)(const char* hostname);
  /**
   * Closes an SSL connection opened by open.
   * Returns 0 on success, -1 on failure.
   */
  int (*close)(int handle);
  /**
   * Send data on the SSL connection.
   * Returns amount of data sent on success, -1 on failure.
   */
  ssize_t (*send)(int handle, const void* buf, size_t size);
  /**
   * Receive data on the SSL connection.
   * Returns amount of data received on success, -1 on failure.
   */
  ssize_t (*read)(int handle, void* buf, size_t size);
  /**
   * Perform a GET request to the given URL.
   * Note: headers should be allocated via malloc, pd->system->realloc or
   * pd->system->formatString - this function takes ownership of the buffer and
   * frees it when it completes.
   */
  bool (*get)(const char* hostname, const char* path, const char* headers,
              size_t headers_length, PlaydateHttpCallback callback,
              void* callback_data);
  /**
   * Perform a GET request to the given URL, but, like, chunked.
   */
  void* get_chunked;   // TODO(zhuowei)
  void* post;          // TODO(zhuowei)
  void* post_chunked;  // TODO(zhuowei)
};

typedef struct PlaydateWifiAPI PlaydateWifiAPI;

struct PlaydateDeviceAPI {
  noreturn void (*exit)(void);
  void (*pd_introComplete)(void);
  void (*setReduceFlashing)(bool reduce_flashing);
};

typedef struct PlaydateDeviceAPI PlaydateDeviceAPI;

struct PlaydateAPIExt {
  PlaydateAPI pd;
  PlaydateWifiAPI* wifi;
  PlaydateDeviceAPI* device;
};

typedef struct PlaydateAPIExt PlaydateAPIExt;

static int update(void* userdata);
const char* fontpath = "/System/Fonts/Asheville-Sans-14-Bold.pft";
LCDFont* font = NULL;

static char* gTweetbuf = NULL;
static size_t gTweetbufLen = 0;

static PlaydateAPI* gPdApi;

#ifdef TARGET_PLAYDATE

// 1.11: from 0x80484dc
const PlaydateWifiAPI gWifiApi = {
    .open = (void*)0x080615d1,
    .close = (void*)0x08061721,
    .send = (void*)0x08061855,
    .read = (void*)0x08061945,
    .get = (void*)0x08060b65,
    .get_chunked = (void*)0x08060bc5,
    .post = (void*)0x08060bf1,
    .post_chunked = (void*)0x08060c55,
};

static bool EnableWifiApiPlaydate(PlaydateAPI* pd) {
  // 1.11
  PlaydateAPIExt* pd_ext = (PlaydateAPIExt*)pd;
  pd_ext->wifi = (PlaydateWifiAPI*)&gWifiApi;
  return true;
}
#endif

#ifdef __APPLE__
static bool EnableWifiApiMacOSSimulator() {
  // 1.11
  static const uint64_t k_pd_capi_setUnlocked_address = 0x0000000100086c2bull;
  void* executable_base = dlsym(RTLD_DEFAULT, "_mh_execute_header");
  void (*pd_capi_setUnlocked)(bool) =
      executable_base + (k_pd_capi_setUnlocked_address - 0x0000000100000000ull);
  pd_capi_setUnlocked(true);
  return true;
}
#endif

static bool EnableWifiApi(PlaydateAPIExt* pd_ext) {
  if (pd_ext->wifi) {
    // Wi-Fi's already unlocked: we're running as a system app (.pdx located on
    // the System partition)
    return true;
  }
  // Pretend to be a system app
#ifdef TARGET_PLAYDATE
  if (EnableWifiApiPlaydate(&pd_ext->pd)) {
    return true;
  }
#endif
#ifdef __APPLE__
  if (EnableWifiApiMacOSSimulator()) {
    return true;
  }
#endif
  return false;
}

// both strings are owned by callback data
struct LuaCallbackData {
  char* lua_function_name;
  char* lua_argument;
};

bool GetCallback(int http_status, void* buf, size_t length, size_t length2,
                 void* callback_data) {
  PlaydateAPI* pd = gPdApi;
  struct LuaCallbackData* lua_data = callback_data;
  pd->system->logToConsole("GetCallback %d %p %ld %ld %p\n", http_status, buf,
                           length, length2, callback_data);
  if (http_status != 200) {
    pd->system->error("Failed to fetch: status = %d", http_status);
    return true;
  }
  gTweetbuf = pd->system->realloc(NULL, length);
  gTweetbufLen = length;
  char* src_buf = buf;
  for (int i = 0; i < length; i++) {
    gTweetbuf[i] = src_buf[i];
  }
  return true;
}

static int LuaInit(lua_State* L) {
  PlaydateAPI* pd = gPdApi;
  PlaydateAPIExt* pd_ext = (PlaydateAPIExt*)pd;
  if (!EnableWifiApi(pd_ext)) {
    pd->system->error("Can't enable wifi API.");
    return 0;
  }
  return 0;
}

static int LuaGet(lua_State* L) {
  PlaydateAPI* pd = gPdApi;
  PlaydateAPIExt* pd_ext = (PlaydateAPIExt*)pd;
  size_t headers_length = 0;
  const char* headers_blob_src = pd->lua->getArgBytes(3, &headers_length);
  char* headers_blob = NULL;
  if (headers_length) {
    headers_blob = pd->system->realloc(NULL, headers_length + 1);
    for (int i = 0; i < headers_length; i++) {
      headers_blob[i] = headers_blob_src[i];
    }
    headers_blob[headers_length] = 0;
  }

  struct LuaCallbackData* callback_data =
      pd->system->realloc(NULL, sizeof(struct LuaCallbackData));
  pd->system->formatString(&callback_data->lua_function_name, "%s",
                           pd->lua->getArgString(4));
  pd->system->formatString(&callback_data->lua_argument, "%s",
                           pd->lua->getArgString(5));
  // pd->system->logToConsole("%s %d", headers_blob, headers_length);
  pd_ext->wifi->get(pd->lua->getArgString(1), pd->lua->getArgString(2),
                    headers_blob, headers_length, GetCallback, callback_data);
  return 0;
}

static const lua_reg kApiRegs[] = {
    {.name = "init", .func = &LuaInit},
    {.name = "get", .func = &LuaGet},
    {.name = NULL, .func = NULL},
};

void RegisterLuaApi(PlaydateAPI* pd) {
  const char* err = NULL;
  if (!pd->lua->registerClass("wdb_pdwifi", kApiRegs, /*vals=*/NULL,
                              /*isstatic=*/true, &err)) {
    pd->system->error("Register Lua API failed: %s", err);
  }
}

#ifdef _WINDLL
__declspec(dllexport)
#endif
    int eventHandler(PlaydateAPI* pd, PDSystemEvent event, uint32_t arg) {
  (void)arg;  // arg is currently only used for event = kEventKeyPressed

  if (event == kEventInitLua) {
    gPdApi = pd;
    RegisterLuaApi(pd);
  }
  return 0;
}
