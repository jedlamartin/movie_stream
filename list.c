#include "list.h"

List* create_list(char* content_type, char* content) {
    List* new_list = (List*)malloc(sizeof(List));
    if (new_list == NULL) {
		fprintf(stderr, "Memory allocation failed for List\n");
		return NULL;
    }

    new_list->content_type = (char*)malloc(strlen(content_type) + 1);
    if (new_list->content_type == NULL) {
        fprintf(stderr, "Memory allocation failed for content_type\n");
        free(new_list);
        return NULL;
    }

	new_list->content = (char*)malloc(strlen(content) + 1);
    if (new_list->content == NULL) {
        fprintf(stderr, "Memory allocation failed for content\n");
        free(new_list->content_type);
        free(new_list);
        return NULL;
	}

	strcpy(new_list->content, content);
	strcpy(new_list->content_type, content_type);
    new_list->next = NULL;
    return new_list;
}

void free_list(List* list){
    List* current = list;
    List* next_node;
    while (current != NULL) {
        next_node = current->next;
        free(current->content);
        free(current->content_type);
        free(current);
        current = next_node;
	}
}

void append_list(List* head, char* content, char* content_type){
    List* new_node = create_list(content, content_type);
    if (new_node == NULL) {
        return; // Memory allocation failed
    }
    
    List* current = head;
    while (current->next != NULL) {
        current = current->next;
    }
	current->next = new_node;
}
