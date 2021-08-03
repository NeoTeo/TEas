#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAXBIN 65535
#define MAXLAB 256 
/*
So, i need a way of deciding if a token is an opcode or a value.
To do that i need a way of quickly comparing a token with existing opcodes.
This i can do either by converting the token into an opcode or having the opcodes as strings.
*/
typedef unsigned char UInt8;
typedef signed char Int8;
typedef unsigned short UInt16;
typedef signed short Int16;

// Constants
const int labelNameMax = 64;

typedef struct Label {
	char name[labelNameMax];
	short addr;	
} Label;


// The binary can at most be the size of TEMA's RAM
UInt8 bin[MAXBIN];
UInt16 binlen = 0;

// NB: This table needs to match the number and order of the opcodes in the TEMA sources.
static char ops[][4] = {"brk","nop","lit","pop",
						"dup","ovr","rot","swp",
						"add","sub","mul","div",
						"equ","grt","neg","jmp",
						"jnz","jsr","bsi","bso"};


Label labels[MAXLAB];
int lcount = 0;


int
labelIdx(char* token) {
	for (int i=0;i<lcount;i++) {
		printf("comparing %s == %s\n",token,labels[i].name);
		if(strcmp(token, labels[i].name) == 0) {
			printf("returning %d\n",i);
			return i;
		}
	}
	return -1;
}

//int
//isLabel(char* token) {
//	for (int i=0;i<lcount;i++) {
//		if(strcmp(token, labels[i].name) == 0) return 1;
//	}
//	return 0;
//}

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
		//printf("comparing %s with %s\n",ops[op],s);
		char *opc = ops[op];
		if(opc[0] == s[0] && opc[1] == s[1] && opc[2] == s[2]) {
			printf("op found: %d\n",op);
			// add modifier flags
			int idx = 3;
			while(s[idx]) {
				switch(s[idx++]) {
				case 's': op |= 0x20; break;	// use short version of opcode
				case 'c': op |= 0x40; break;	// copy instead of pop
				case 'r': op |= 0x80; break;	// use return stack instead of parameter stack
				}
			}
			return op;
		}
	}

	return -1;
}

int
addLabel(char *name, UInt16 addr) {
	if(lcount >= labelNameMax) {
		fprintf(stderr, "addLabel: label buffer full!");
		return -1 ;
	}	
	// do i need to copy the label string here? Yes!
	memcpy(labels[lcount].name, name, strlen(name)+1);
	memcpy(&labels[lcount++].addr, &addr, 2);
	printf("added new label %s at address %d\n",labels[lcount-1].name, labels[lcount-1].addr);
	return 0;
}

// An initial pass to ensure that labels can be referenced before they have been defined.
static int
preScan(FILE *f) {
	char buf[42];
	int lidx;
	UInt16 val;

	printf("------------ PRE-SCAN START ---------------\n");
	while(fscanf(f, "%s", buf) == 1) {
		switch(buf[0]) {
			case '^':	// move to position in memory given by absolute argument
				val = hextract(&buf[1]);
				binlen = val;
				break;

			case '@':	// define label by associating a name with an absolute address 
				printf("Handling label: ");
				// if the label already exists, create a new one using the existing as a namespace.
				// NB: to implement. Needs a string concatenation function.
				// if the label does not exist, create it
				if((lidx = labelIdx(buf+1)) < 0) {
					addLabel(buf+1, binlen) ;
				}
				break;
		}
	}

	printf("------------ PRE-SCAN END ---------------\n");
	rewind(f);
	return 0;
}

static int
scanInput(FILE *f) {

	char *sourceline = NULL;
	size_t len = 0;
	ssize_t read;
	char *token;
	char buf[42];
	int op, lidx;
	UInt16 val;
	Label l;
	UInt8 inComment = 0;
	printf("------------ MAIN SCAN START ---------------\n");
	while(fscanf(f, "%s", buf) == 1) {
		token = buf;
		printf("buf %s\n", buf);

		// skip comments
		if(token[0] == '(') { inComment = 1; continue; }
		if(token[0] == ')') { inComment = 0; continue; }
		if(inComment) continue;	

		// first check for compiler symbols
		switch(token[0]) {

			case '^': // move to position in memory given by absolute argument
				val = hextract(&token[1]);
				binlen = val;
				break;

			case '#': // literal values given as hex values or as labels to be resolved. 
				if((token[1] == '@') && ((lidx = labelIdx(token+2)) >= 0)) {
					val = labels[lidx].addr;
				} else {
					val = hextract(&token[1]);
				}

				// are we writing a byte or a short?
				if(stln(token+1) < 3) { 
					writebyte(0x02);		// opcode for .lit
					writebyte(val);
				}
				else {
					writebyte(0x22) ;	// opcode for .lit16
					writeshort(val);
				}
				break;	

			case '$':	// relative address given as a label to be resolved and converted to relative distance.
				if((token[1] == '@') && ((lidx = labelIdx(token+2)) >= 0)) {
					Label tl = labels[lidx];
					bin[binlen++] = 0x2; // write the lit opcode
					writebyte(tl.addr-(binlen+1));// add one to address to account for this writebyte offset.	
					printf("relative address of label is %d\n", tl.addr-(binlen)); 
				}
				break;	

/*			case '@': // 
				printf("mainscan: label: ");
				// if the label exist use it, else create it.
				if((lidx = labelIdx(token+1)) < 0) {
					//l.name = token+1;
					//l.addr = binlen;
					//addLabel(token+1, binlen) ;
					//printf("added new label %s at address %d\n",l.name, l.addr);
					fprintf(stderr,"label not found!");
				} else {
					Label tl = labels[lidx];
					printf("found label: %s at %d. binlen is %d\n",tl.name, tl.addr,binlen);

					/// this is where we should check for modifiers to the label to decide 
					/// if we're doing an absolute or relative jump
					// write the lit opcode
					bin[binlen++] = 0x2;
					// write the offset
					writebyte(tl.addr-(binlen+1));// add one to address to account for this writebyte offset.	
				}
				break;	
*/
			default:
				if((op = str2op(token)) < 0) break;
				printf("opcode is %d\n",op);
				bin[binlen++] = op;
		}	
		
	}

	printf("------------ PRE SCAN END ---------------\n");
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

		if((err = preScan(f))) {
			fprintf(stderr, "Error %d pre-scanning input\n",err);
			return err;
		}

		if((err = scanInput(f))) {
			fprintf(stderr, "Error %d scanning input\n",err);
			return err;
		}

		// write out the bin.
		printf("the bin is:");
		for(UInt16 i=0;i<binlen;i++) {
			if(i%8 == 0) printf("\n");
			printf("0x%x ",bin[i]);
		}

		fwrite(bin, binlen, 1, fopen(outfile, "wb"));
		fclose(f);
	}
	return err;
}
