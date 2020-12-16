#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <ctype.h>
#include <string.h>
#include <pthread.h>
#include <math.h>
#include <errno.h>
#include "Asst2.h"
//creates a packaged structure for all the values that a thread will need to
//complete its job
threadParams* createTP(char* file, fileDataNode** fileData, pthread_mutex_t* mutexLock) {
	threadParams* params = (threadParams*) malloc( sizeof(threadParams) );
	params -> filePath = file;
	params -> head = fileData; 
	params -> lock = mutexLock;
	return params;	
}
//frees the threadParams structure that was malloc'd for a thread
int freeTP(threadParams* tp) {
	free(tp -> filePath);
	free(tp);
	return 0;
}

//counts the number of relevant files and subdirectory in a directory
//so the right number of threads can be spawned and joined
int countFiles(char* filepath) {
	DIR* derp = opendir(filepath);
	if (derp == NULL) {
		printErrors(filepath);
		return -1;
	}
	struct dirent* curr = NULL;
	int threadCount = 0;
	while ( (curr = readdir(derp)) != NULL) {
		if (curr -> d_type == DT_REG || curr -> d_type == DT_DIR) {
			++threadCount;
		}
	}
	closedir(derp);
	return threadCount;
}

//inserts a node in the linked list that stores data for all
//regular files encountered
fileDataNode* insertFDNode(char* file, fileDataNode** currHead) {
	fileDataNode* head;

	if (currHead != NULL){
		if ( (*currHead) -> currFile -> name == NULL) {
			head = *currHead;
			head -> currFile -> name = file; 
			return head;
		}
		head = (fileDataNode*) malloc( sizeof(fileDataNode) ); 
		head -> nextFile = *currHead;
		*currHead = head;
	}
	else {
		head = (fileDataNode*) malloc( sizeof(fileDataNode) ); 
		head -> nextFile = NULL;
	}

	head -> currFile = (tuple*) malloc( sizeof(tuple) );
	head -> currFile -> name = file; 
	head -> currFile -> freq = 0;
	head -> tokens = NULL;
	head -> tokenSize = 0;
	return head;
}

//searches a file's token list to see if the current word is already 
//in the list
int binSearchTuple(tuple* tokenArray, int low, int high, char* word) 
{ 
	if (high < low) 
		return -1; 
	int mid = low + (high - low) / 2;
	int cmp = strcmp(word, tokenArray[mid].name);
	if (cmp == 0) 
		return mid; 
	if (cmp > 0) 
		return binSearchTuple(tokenArray, (mid + 1), high, word); 
	return binSearchTuple(tokenArray, low, (mid - 1), word); 
}

//if a token from a file is not found in the file's token list
//it is inserted with an initial count value of 1
int insertTuple(tuple* tokenArray, int size, char* word) {
	int i;
	for (i = size - 1; (i >= 0 && strcmp( tokenArray[i].name, word) > 0); i--)
		tokenArray[i + 1] = tokenArray[i];
	tokenArray[i + 1].name = word;
	tokenArray[i + 1].freq = 1;
	return 0;
}

//used to merge sort the linked list data structure by the number
//of tokens in the file (not by unique tokens)
fileDataNode* mergeSort(fileDataNode* list1, fileDataNode* list2)
{
	fileDataNode* result = NULL;
	if (list1 == NULL)
		return (list2);
	else if (list2 == NULL)
		return (list1);
	
	if (list1->currFile->freq <= list2->currFile->freq) {
		result = list1;
		result->nextFile = mergeSort(list1->nextFile, list2);
	}
	else {
		result = list2;
		result->nextFile = mergeSort(list1, list2->nextFile);
	}
	
	return (result);
}

//finds the middle of the linked list so that it can be split into 2 for 
//merge sort
void split( fileDataNode* list, fileDataNode** first, fileDataNode** second)
{
	*first = list;
	if (list == NULL || list->nextFile == NULL)
	{
		*second = NULL;
		return;
	}
	fileDataNode* slow = list;
	fileDataNode* fast = list->nextFile;
	while (fast != NULL)
	{
		fast = fast->nextFile;
		if (fast != NULL)
		{		
			slow = slow->nextFile;
            		fast = fast->nextFile;
        	}
    	}
	*second = slow->nextFile;
	slow->nextFile = NULL;
}

//splits the linked list repeatedly into smaller halves
//sorts these halves and then merges them
void sortLL(fileDataNode** head) { 
	if (*head == NULL || (*head)->nextFile == NULL)
        	return;
	
	fileDataNode* list1;
	fileDataNode* list2;
 
	split(*head, &list1, &list2);
 
	sortLL(&list1);
	sortLL(&list2);
	
	*head = mergeSort(list1, list2);
}

//given a token's frequency and its mean value between two files,
//find the token's KLD value
float kld(float freq, float mean) {
	return (freq * log10f(freq/mean));	
}

//frees the list of tokens that was malloc'd for 
//each file
int freeArray(fileDataNode* head) {
	tuple* tokens = head -> tokens;
	for (int i = 0; i < head -> tokenSize; i++) {
		free(tokens[i].name);
	}
	return 0;
}

//free's the linked list storing data for all files
int freeLL(fileDataNode* head) {
	while(head != NULL){
   		fileDataNode* currHead = head;
   		head = head->nextFile;
		free(currHead -> currFile -> name);
		free(currHead -> currFile);
		freeArray(currHead);
		free(currHead -> tokens);
   		free(currHead);
	}
	return 0;
}

//prints the token list for debugging purposes
void printArr(tuple* tokenArray, int size, int tokenBuffer) {
	for (int i =0; i < size; i++) {
		printf("tokens[%d]: %s %f\n", i, tokenArray[i].name, tokenArray[i].freq);
	}
}

//prints the linked list storing data for all files for debugging purposes
int printLL(fileDataNode* head) {
	while(head != NULL){
   		fileDataNode* currHead = head;
   		head = head->nextFile;
		
		printf("Node: %s \n", currHead -> currFile -> name);
		printf("Total Tokens: %f \n", currHead -> currFile -> freq);
		printf("Total Tuples: %d \n", currHead -> tokenSize);
	}
	return 0;
}

//used to report errors in opening filepath's to STDOUT
void printErrors(char* filepath) {
	if (errno == EACCES) {
		printf("Permission to open filepath %s denied.\n", filepath);
	}
	else if (errno == ENOENT) {
		printf("Filepath %s does not exist.\n", filepath);
	}
	else if (errno == ENOTDIR) {
		printf("Filepath %s is not a directory.\n", filepath);
	}
	else {
		printf("Error opening filepath %s.\n", filepath);
	}
}

//next couple functions are used in coloring STDOUT outputs
//based on JSDValues
void red() {
	printf("\033[0;31m");
}

void yellow() {
	printf("\033[0;33m");
}

void green() {
	printf("\033[0;32m");
}

void cyan() {
	printf("\033[0;36m");
}

void blue() {
	printf("\033[0;34m");
}

void white() {
	printf("\033[0m");
}

//prints JSDValues in their respective colors along with the 
//file pair names
void printJSD(float jsdVal, char* f1, char* f2) {
	if (jsdVal <= .1) {
		red();
	}
        else if (jsdVal <= .15) { 
                yellow();        
        } 
        else if (jsdVal <= .2) { 
                green(); 
        } 
        else if (jsdVal <= .25) { 
                cyan(); 
        } 
        else if (jsdVal <= 3) { 
                blue(); 
       } 
       else { 
                white(); 
       } 
       printf("%.3f", jsdVal); 
       white(); 
       printf(" \"%s\" and \"%s\"\n", f1, f2); 
}
