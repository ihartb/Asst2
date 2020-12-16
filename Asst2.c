#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <ctype.h>
#include <string.h>
#include <pthread.h>
#include <math.h>
#include <errno.h>
#include "Asst2.h"

//traverses each unique pair of files and their tokens
//to find and print the JSD value between two files
void computeJSD(fileDataNode* allFilesLL) {
	fileDataNode* f1 = allFilesLL;
	fileDataNode* f2;
	while (f1 -> nextFile != NULL) {
		fileDataNode* f2 = f1 -> nextFile;
		while (f2 != NULL) { //traverses every file other than current one
			float kld1 = 0;
			int i1 = 0;
			tuple* tok1 = f1 -> tokens; 
			int size1 = f1 -> tokenSize;

			float kld2 = 0;
			int i2 = 0;
			tuple* tok2 = f2 -> tokens;
			int size2 = f2 -> tokenSize;
			float mean = 0;
			while (i1 != size1 && i2 != size2) { //using two pointers to traverse each token value within the files
				int cmp = strcmp(tok1[i1].name, tok1[i2].name);
				if (cmp == 0) { //if both files contain the token, then we add the token's KLD value  to each file's KLD total
					mean = ( tok1[i1].freq + tok2[i2].freq ) / 2;
					kld1 += kld(tok1[i1].freq, mean);
					kld2 += kld(tok2[i2].freq, mean);
					i1++;
					i2++;
				}
				//if only one file contains the token, we add the token's KLD value to that file's KLD total
				else if (cmp < 0) { 
					mean = (tok1[i1].freq)/2;
					kld1 += kld(tok1[i1].freq, mean);
					i1++;
				}
				else {
					mean = (tok2[i2].freq)/2;
					kld2 += kld(tok2[i2].freq, mean);
					i2++;
				}
			}
			
			//finish traversing the token list if it if previous while loop did not
			while (i1 < size1) {
				mean = (tok1[i1].freq)/2;
				kld1 += kld(tok1[i1].freq, mean);
				i1++;
			}
			
			while (i2 < size2) {
				mean = (tok2[i2].freq)/2;
				kld2 += kld(tok2[i2].freq, mean);
				i2++;
			}
			
			//average the kld values of each file together to give us the JSD value	
			float jsd = (kld1 + kld2) / 2;

			//print out the JSD value with its respective color
			printJSD(jsd, f1 -> currFile -> name, f2 -> currFile -> name);
			
			//iterate to next file to compare with f1
			f2 = f2 -> nextFile;
		}
		
		//iterate to next file that needs to be compared with the others
		f1 = f1 -> nextFile;
	}
	
}

//receives a file name to iterate through and tokenize, store data in shared linked list
void* tokenizer(void* args) {
	threadParams* params = (threadParams*) args;
	
	FILE* f = fopen(params -> filePath, "r"); 
	if (f == NULL) { //if file can't be opened report error and exit
		printErrors(params -> filePath);
		freeTP(params);
		pthread_exit(0);
	}
	
	//insert node into shared data structure for this file
	pthread_mutex_lock(params -> lock);
	fileDataNode* currNode = insertFDNode(params -> filePath, params -> head);
	pthread_mutex_unlock(params -> lock);
	
	//initialize a list of tuple<name, freq> to store tokens in
	int tokenBuffer = 500;
	tuple* tokens = (tuple*) malloc(tokenBuffer * sizeof(tuple) );
	//keep track of the number of distinct tokens and total tokens
	int numTuples = 0;
	int numTokens = 0;
	//save each words in a char* pointer
	char c;
	int wordBuffer = 1000;
	char* word = (char*) calloc(wordBuffer, 1);	
	int iWord = -1;
	//iterate through the file character by character 
	while (1) {
		c = tolower(fgetc(f));
		//if character c is a space or eof, then we have reached the end of the word
		if ( isspace(c) || feof(f) ) {
			//add NUL to end the word
			word[++iWord] = '\0';
			//free extraneous space from the wordBuffer
			word = (char*) realloc(word, iWord+1);

			//if the length of the word is greater than 0, 
			//increase numTokens, then binary search for the word in tuple list,
			//if it exists, free the word, and simply increase its frequency
			//if it does not, insert the new word while maintaining alphabetical order between tokens 
			//and increment numTuples
			if (iWord != 0) {
				++numTokens;
				int index = binSearchTuple(tokens, 0, numTuples-1, word);
				if (index == -1) {
					if (numTuples == tokenBuffer) {
						tokenBuffer *=2;
						tokens = (tuple*) realloc(tokens, tokenBuffer * sizeof(tuple));
					} 
					insertTuple(tokens, numTuples, word);
					++numTuples;
				}
				else {
					++(tokens[index].freq); 
					free(word);
				}
			}
			else
			{	
				free(word);
			}
			
			if (feof(f))
				break;
			
			//reinitialize word pointer to start tokenizing the next word.
			wordBuffer= 1000;
			word = (char*) calloc(wordBuffer, 1);
			iWord = -1;
		}
		else if (isalpha(c) || c == '-') { //acceptable characters are written to word, others ignored
			if (iWord == wordBuffer-2) { //if word is longer than wordBuffer, realloc to be larger
				word[iWord+1] = '\0';
				wordBuffer *= 2;
				word = (char*) realloc(word, wordBuffer);
			}
			word[ ++iWord ] = c;
		}       
	}
			
	if( ferror(f) ){
		perror( "The following error occurred" );
        }
	
	fclose(f);

	//write data to node within shared data structure
	//currFile -> freq tells us the total number of tokens found in the file
	currNode -> currFile -> freq = numTokens;

	tokens = (tuple*) realloc(tokens, numTuples * sizeof(tuple));
	//convert token counts to frequency values
	for (int i = 0; i < numTuples; i++) {
		tokens[i].freq = tokens[i].freq/numTokens;
	}
	//currNode -> tokens is the list of tuple<name, freq> or tuple<unique token, freq> found in the file
	currNode -> tokens = tokens;
	//numTuples is the size of the tokens list, and the number of unique tokens in the file
	currNode -> tokenSize = numTuples;
	free(params);
}

//traverses the current directory for subdirectories and files, 
//creates threads accordingly to resolve each
void* traverseDirectory(void* args){
	threadParams* params = (threadParams*) args;
	char* filepath =  params -> filePath;
	//first we count the total number of relevant files and subdirectories
	//within the directory to initialize and join the correct number of threads
	int threadCount = countFiles( filepath );

	//if there's an error opening the file, threadCount will be less than 0
	//error is reported, and function exited
	if (threadCount < 0) {
		freeTP(params);
		return 0;
	}

	//we start spawning threads for each subdirectory and regular file found
	DIR* derp = opendir(filepath);
	struct dirent* curr = NULL;
	pthread_t threads[threadCount];
	pthread_attr_t threadAttrs;
	pthread_attr_init(&threadAttrs);
	int i = 0;
	while ( (curr = readdir(derp)) != NULL) {
		char* currFile = curr->d_name;
		int type = curr->d_type;
		if ( strcmp(currFile, ".") == 0 || strcmp(currFile, "..") == 0 || !(type == DT_DIR || type == DT_REG) ) {
			continue;
		}
		//we malloc the filepath properly so threads can open them 
		char* file;	
		if ( filepath[strlen(filepath)-1] != '/') {
			file = (char*) malloc(strlen( filepath )+strlen(currFile)+2);
			strcpy(file, filepath);
			strcat(file, "/");
		}
		else {
			file = (char*) malloc(strlen( filepath )+strlen(currFile)+1);
			strcpy(file, filepath );
		}
		strcat(file, currFile);

		//we create a threadParam structure which packages everything a spawning 
		//thread will need to complete its job
		threadParams* newParams = createTP(file, params -> head, params -> lock);
		if (type == DT_DIR) {
			pthread_create(&threads[i], &threadAttrs, traverseDirectory, (void*) newParams);
		}
		else if (type == DT_REG){ 
			pthread_create(&threads[i], &threadAttrs, tokenizer, (void*) newParams);
		}
		i++;
	}
	closedir(derp);
	pthread_attr_destroy(&threadAttrs);
	for (int j = 0; j < i; j++) //we join all the threads we spawned
		pthread_join(threads[j], NULL);
	freeTP(params);
}

//traverses each directory, tokenizes files, and calculates JSD values
int main(int argc, char** argv){
	if (argc != 2) {
		printf("Incorrect number of arguments\n");
		return 0;
	}

	//first create the shared data structure node
	fileDataNode* allFilesLL = insertFDNode(NULL, NULL);

	//malloc the filepath for threads
	char* file = malloc( strlen(argv[1])+1);
	strcpy(file, argv[1]);

	//initialize the lock needed for allFilesLL
	pthread_mutex_t lock;
	pthread_mutex_init(&lock, NULL);

	//we create a threadParam structure which packages everything a spawning
	//thread will need
	threadParams* params = createTP(file, &allFilesLL, &lock);

	//use traverseDirectory to get token data for each file in the directory 
	traverseDirectory( (void*) params);
	pthread_mutex_destroy(&lock);
	//if the shared data structure is not written to or only has one node, there are not
	//enough files to compare, so print error and return
	if (allFilesLL -> currFile -> name == NULL || allFilesLL -> nextFile == NULL) {
		printf("Not enough files to compare.\n");
	}
       //otherwise sort the linked list by number of total tokens, and compute the JSD value between
       //each unique file pair	
	else {
		sortLL(&allFilesLL);
		computeJSD(allFilesLL);
	}
	//free the linked list
	freeLL(allFilesLL);
	return 0;
}
