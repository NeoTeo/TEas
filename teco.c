#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAXBIN 65535
/*
So, i need a way of deciding if a token is an opcode or a value.
To do that i need a way of quickly comparing a token with existing opcodes.
This i can do either by converting the token into an opcode or having the opcodes as strings.
*/
typedef unsigned char UInt8;
typedef signed char Int8;
typedef unsigned short UInt16;
typedef signed short Int16;


// The binary can at most be the size of TEMA's RAM
UInt8 bin[MAXBIN];
UInt16 binlen = 0;

// NB: This table needs to match the number and order of the opcodes in the TEMA sources.
static char ops[][4] = {"brk","nop","lit","pop",
						"dup","ovr","rot","swp",
						"add","sub","mul","div",
						"equ","grt","neg","jmp",
						"jnz","jsr","bsi","bso"};

void
lowerize(char* s) {
	while(*s != '\0') {
		*s = tolower(*s);
		s += 1;
	}

}

int
str2op(char* s) {
	lowerize(s);
	UInt8 count = sizeof(ops)/sizeof(ops[0]);
	for(int op=0;op<count;op++) {
		//printf("comparing %s with %s\n",ops[op],s);
		if(strcmp(ops[op],s) == 0)
			return op;
	}
	return -1;
}

UInt16
hextract(char *s) {
	// the string can at most be four characters
	UInt16 val = 0;

	for(int i=0;i<4;i++) {	
		char sv = s[i];

		if((sv > 47 ) && (sv < 58)) sv -= 48;
		else if((sv > 64) && (sv < 71)) sv -= 55;
		else if((sv > 96) && (sv < 103)) sv -= 87;

		val += sv << ((3-i)<<2);	
	}
	return val;
}

static int
scanInput(FILE *f) {

	char *sourceline = NULL;
	size_t len = 0;
	ssize_t read;
	char* token;

	int op;
	UInt16 val;

	//while((ch = fgetc(f)) != EOF) {
	while((read = getline(&sourceline, &len, f) != EOF)) {
		printf(":========= %s",sourceline);
		while((token = strsep(&sourceline, " "))) {
			printf("token %s\n", token);
			// first check for compiler symbols
			switch(token[0]) {
				case '#': 
					val = hextract(&token[1]);
					printf("literal! %d",val);
					
				case '@': printf("label!");
				case '.': printf("dot!");
			}	
			//if((op = str2op(token)) < 0) continue;
			
			printf("opcode is %d\n",op);
		}
	}

	// clean up.
	fclose(f);
	if(sourceline) free(sourceline);
	printf("handleSymbols done");
	return 1;
}

int
main(int argc, char **argv) {

	if (argc != 3 || strcmp(argv[1],"-f") != 0) printf("Wrong number of arguments");
	else {
		char *filepath = argv[2];	
		FILE *f;

		if(!(f = fopen(filepath, "rb"))) {
			fprintf(stderr, "Error: source file missing\n");
			return 0;
		}

		if(scanInput(f))
		// write out the bin.
		exit(EXIT_SUCCESS);
	}
}
typedef unsigned short Uint16;

typedef struct Label {
	char* label;
	short addr;
} Label;

const int labelMax = 256;
Label labels[labelMax];
int lcount = 0;

void
addLabel(Label label) {
	if(lcount >= labelMax) {
		printf("Error: label buffer full!");
		return;
	}	
	// do i need to copy the label string here?
	labels[lcount++] = label;
}

int
isLabel(char* token) {
	for (int i=0;i<lcount;i++) {
		if(strcmp(token, labels[i].label) == 0) return 1;
	}
	return 0;
}
