#ifndef _CONFIG_H_
#define _CONFIG_H_

#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "av_header.h"

int GetCurrentPath(char buf[], char *pFileName);

char *GetIniKeyString(char *title,char *key,char *filename);

int GetIniKeyInt(char *title,char *key,char *filename);

#endif
