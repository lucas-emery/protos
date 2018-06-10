#ifndef PC_2018_07_MESSAGE_H
#define PC_2018_07_MESSAGE_H

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

void DieWithUserMessage(const char *msg, const char *detail);
void DieWithSystemMessage(const char *msg);

#endif