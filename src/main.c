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

#ifdef TARGET_PLAYDATE
static bool EnableWifiApiPlaydate() { return false; }
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
    return true;
  }
#ifdef TARGET_PLAYDATE
  if (EnableWifiApiPlaydate()) {
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

bool GetCallback(int http_status, void* buf, size_t length, size_t length2,
                 void* callback_data) {
  PlaydateAPI* pd = callback_data;
  pd->system->logToConsole("GetCallback %d %p %ld %ld %p\n", http_status, buf,
                           length, length2, callback_data);
  return true;
}

#ifdef _WINDLL
__declspec(dllexport)
#endif
    int eventHandler(PlaydateAPI* pd, PDSystemEvent event, uint32_t arg) {
  (void)arg;  // arg is currently only used for event = kEventKeyPressed

  if (event == kEventInit) {
    const char* err;
    font = pd->graphics->loadFont(fontpath, &err);

    if (font == NULL)
      pd->system->error("%s:%i Couldn't load font %s: %s", __FILE__, __LINE__,
                        fontpath, err);

    // Note: If you set an update callback in the kEventInit handler, the system
    // assumes the game is pure C and doesn't run any Lua code in the game
    pd->system->setUpdateCallback(update, pd);
    PlaydateAPIExt* pd_ext = (PlaydateAPIExt*)pd;
    if (!EnableWifiApi(pd_ext)) {
      pd->system->error("Can't enable wifi API.");
      return 0;
    }
#ifdef __APPLE__
    fprintf(stderr, "%p\n", pd_ext->wifi->get);
#endif
    pd_ext->wifi->get("example.com", "/", NULL, 0, GetCallback, pd);
  }

  return 0;
}

#define TEXT_WIDTH 86
#define TEXT_HEIGHT 16

int x = (400 - TEXT_WIDTH) / 2;
int y = (240 - TEXT_HEIGHT) / 2;
int dx = 1;
int dy = 2;

static int update(void* userdata) {
  PlaydateAPI* pd = userdata;

  pd->graphics->clear(kColorWhite);
  pd->graphics->setFont(font);
  pd->graphics->drawText("Hello World!", strlen("Hello World!"), kASCIIEncoding,
                         x, y);

  x += dx;
  y += dy;

  if (x < 0 || x > LCD_COLUMNS - TEXT_WIDTH) dx = -dx;

  if (y < 0 || y > LCD_ROWS - TEXT_HEIGHT) dy = -dy;

  pd->system->drawFPS(0, 0);

  return 1;
}
