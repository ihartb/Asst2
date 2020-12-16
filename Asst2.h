#ifndef _ASST2_H_
#define _ASST2_H_

#define DELIMS " \n\t\r"

typedef struct tuple {
	char* name;
	float freq;
} tuple;

typedef struct fileDataNode {
	tuple* currFile;
	struct fileDataNode* nextFile; 
	tuple* tokens;
	int tokenSize;
} fileDataNode;

typedef struct jsdVal {
	char* f1;
	char* f2;
	float jsd;
} jsdVal;

typedef struct threadParams {
	char* filePath;
	fileDataNode** head;
	pthread_mutex_t* lock;	
} threadParams;

//directories
threadParams* createTP(char* file, fileDataNode** fileData, pthread_mutex_t* mutexLock);
int freeTP(threadParams* tp);
int countFiles(char* filepath);

//tokenizer
fileDataNode* insertFDNode(char* file, fileDataNode** currHead);
int binSearchTuple(tuple* tokenArray, int low, int high, char* word);
int insertTuple(tuple* tokenArray, int size, char* word);

//calculations
void sortLL(fileDataNode** head);
float kld(float freq, float mean);

//main
int freeLL(fileDataNode* head);
void printArr(tuple* tokenArray, int size, int tokenBuffer);
int printLL(fileDataNode* head);
void printErrors(char* filepath);
void red();
void yellow();
void green();
void cyan();
void blue();
void white();
void printJSD(float jsdVal, char* f1, char* f2);
#endif
