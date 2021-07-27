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

typedef struct Label {
	char* label;
	short addr;
} Label;

// Constants
const int labelMax = 256;

// The binary can at most be the size of TEMA's RAM
UInt8 bin[MAXBIN];
UInt16 binlen = 0;

// NB: This table needs to match the number and order of the opcodes in the TEMA sources.
static char ops[][4] = {"brk","nop","lit","pop",
						"dup","ovr","rot","swp",
						"add","sub","mul","div",
						"equ","grt","neg","jmp",
						"jnz","jsr","bsi","bso"};


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

void
lowerize(char* s) {
	while(*s != '\0') {
		*s = tolower(*s);
		s += 1;
	}

}


UInt16
hextract(char *s) {
	UInt16 val = 0;
	char sv;

	while((sv = *s++)) {

		if((sv > 47 ) && (sv < 58)) sv -= 48;
		else if((sv > 64) && (sv < 71)) sv -= 55;
		else if((sv > 96) && (sv < 103)) sv -= 87;

		val = (val << 4) + sv;
	}
	return val;
}

static void
writebyte(UInt8 val) {
	bin[binlen++] = val;	
}

static void
writeshort(UInt16 val) {
	bin[binlen++] = val >> 8;	
	bin[binlen++] = val & 0xFF;	
}

static int 
stln(char *s) { 
	int i = 0;
	for(;(s[i] && s[++i]);) ;
	return i;
}

int
str2op(char* s) {
	lowerize(s);
	UInt8 count = sizeof(ops)/sizeof(ops[0]);
	for(int op=0;op<count;op++) {
		printf("comparing %s with %s\n",ops[op],s);
		char *opc = ops[op];
		if(opc[0] == s[0] && opc[1] == s[1] && opc[2] == s[2]) {
			printf("op found: %d\n",op);
			return op;
		}
	}
	return -1;
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

					// are we writing a byte or a short?
					if(stln(token+1) < 3) { 
						bin[binlen++] = 0x02 ;	// opcode for .lit
						writebyte(val);
					}
					else {
						bin[binlen++] = 0x22 ;	// opcode for .lit16
						writeshort(val);
					}
				break;	
				case '@': printf("label!");
				break;	
				case '.': printf("dot!");
				break;	
				default:
					if((op = str2op(token)) < 0) continue;
				printf("opcode is %d\n",op);
				bin[binlen++] = op;
					
			}	
			
		}
	}

	// clean up.
	if(sourceline) free(sourceline);
	return 0;
}

int
main(int argc, char **argv) {
	int err = 0;

	if (argc != 3) fprintf(stderr,"Usage: teasm source.tas target.obj");
	else {
		char *filepath = argv[1];	
		char *outfile = argv[2];
		FILE *f;

		if(!(f = fopen(filepath, "rb"))) {
			fprintf(stderr, "Error: source file missing\n");
			return 0;
		}

		if((err = scanInput(f))) {
			fprintf(stderr, "Error %d scanning input\n",err);
			return err;
		}

		// write out the bin.
		printf("the bin is:");
		for(UInt16 i=0;i<binlen;i++) printf("0x%x ",bin[i]);

		fwrite(bin, binlen, 1, fopen(outfile, "wb"));
		fclose(f);
	}
	return err;
}
