#ifndef LIB_MESSAGESYNC_H
#define LIB_MESSAGESYNC_H 1
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define BUFFER_SIZE 300

typedef struct MessageSync {
  bool hasMessage;
  char buffer[BUFFER_SIZE];
  int length;
} MessageSync;

MessageSync *MessageSync_malloc() {
  MessageSync *self = (MessageSync *)malloc(sizeof(MessageSync));
  if (self != NULL) {
    self->hasMessage = false;
    self->length = 0;
    self->buffer[0] = '\0';  // Initialize buffer with null terminator
  }
  return self;
}

void MessageSync_hasMessage(MessageSync *self) { return self->hasMessage; }

void MessageSync_free(MessageSync *self) { free(self); }

void MessageSync_clear(MessageSync *self) {
  self->length = 0;
  self->hasMessage = false;
}

void MessageSync_append(MessageSync *self, const char *text) {
  if (self->hasMessage) {
    return;
  }
  int textLength = strlen(text);
  if (self->length + textLength < BUFFER_SIZE) {
    memcpy(self->buffer + self->length, text, textLength);
    self->length += textLength;
  }
}

void MessageSync_print(MessageSync *self) {
  if (self->length > 0) {
    fwrite(self->buffer, sizeof(char), self->length, stdout);
  }
}

#endif
