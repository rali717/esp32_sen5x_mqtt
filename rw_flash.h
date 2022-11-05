#ifndef RW_FLASH_H
#define RW_FLASH_H

String readFile(fs::FS &fs, const char *path);

void writeFile(fs::FS &fs, const char *path, const char *message);

#endif