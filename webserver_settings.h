
#ifndef Arduino_h
#define Arduino_h
#include <arduino.
#endif

#ifndef WEBSERVER_SETTINGS_H
#define WEBSERVER_SETTINGS_H

#include "rw_flash.h"
#include <ESPAsyncWebServer.h>

String readFile(fs::FS &fs, const char *path);

void writeFile(fs::FS &fs, const char *path, const char *message);

void notFound(AsyncWebServerRequest *request);

String readFile(fs::FS &fs, const char *path);

void writeFile(fs::FS &fs, const char *path, const char *message);

String processor(const String &var);

void start_webserver();

void loop2();

#endif
