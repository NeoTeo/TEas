#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
main(int argc, char** argv) {

	if (argc != 3 || strcmp(argv[1],"-f") != 0) printf("Wrong number of arguments");
	else {
		char* filepath = argv[2];	
		FILE *f;
		char* sourceline = NULL;
		size_t len = 0;
		ssize_t read;
		char* token;

		if(!(f = fopen(filepath, "rb"))) {
			fprintf(stderr, "Error: source file missing\n");
			return 0;
		}
		//while((ch = fgetc(f)) != EOF) {
		while((read = getline(&sourceline, &len, f) != EOF)) {
			printf(":========= %s",sourceline);
			while((token = strsep(&sourceline, " "))) {
				printf("token %s\n", token);
						
			}
		}


		// clean up.
		fclose(f);
		if(sourceline) free(sourceline);
		printf("done");
		exit(EXIT_SUCCESS);
	}
}

typedef unsigned short Uint16;

typedef struct Label {
	char* label;
	short addr;
} Label;

Label labels[256];
int lcount = 0;

void
addLabel(Label label) {
	if(lcount >= sizeof(labels)/sizeof(labels[0])) {
		printf("Error: label buffer full!");
		return;
	}	
	// do i need to copy the label string here?
	labels[lcount++] = label;
}

int
isWord(char* token) {
	for (int i=0;i<lcount;i++) {
		if(strcmp(token, labels[i].label) == 0) return 1;
	}
	return 0;
}
