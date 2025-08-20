#ifndef LIST_H
#define LIST_H

#include <stdio.h>
#include <stdlib.h>

typedef struct {
	char* content_type;
	char* content;
	struct List* next;
} List;

List* create_list(char* content, char* content_type);
void free_list(List* list);
void append_list(List* head, char* content, char* content_type);

#endif