#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAXBIN 65535
#define MAXLAB 256 
#define MAXMAC 64 
#define MAXDEF 32 

#define STRLEN 64

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
	char name[STRLEN];
	short addr;	
} Label;

typedef struct Macro {
	char name[STRLEN];
	char defs[MAXDEF][42];
	int defcount;
} Macro;

// The binary can at most be the size of TEMA's RAM
UInt8 bin[MAXBIN];
UInt16 binlen = 0;


// NB: This table needs to match the number and order of the opcodes in the TEMA sources.
static char ops[][4] = {"brk","nop","lit","pop",
						"dup","ovr","rot","swp","sts",
						"add","sub","mul","div",
						"and","ior","xor","shi",
						"equ","neq","grt","lst",
						"jmp","jnz","jsr",
						"lda","sta","ldr","str",
						"bsi","bso"};


Label labels[MAXLAB];
int lcount = 0;

Macro macros[MAXMAC];
int mcount = 0;

int
labelIdx(char* token) {
	for (int i=0;i<lcount;i++) {
		//printf("comparing %s == %s\n",token,labels[i].name);
		if(strcmp(token, labels[i].name) == 0) {
			//printf("returning %d\n",i);
			return i;
		}
	}
	return -1;
}

void
lowerize(char* s) {
	while(*s != '\0') {
		*s = tolower(*s);
		s += 1;
	}
}

// returns 0 if the given string is not a valid hexadecimal value
// returns !0 otherwise
static int
ishex(char *s) {
	char sv;
	while((sv = *s++)) {
		if(sv >= '0' && sv <= '9' || sv >= 'a' && sv < 'g' || sv >= 'A' && sv < 'G')
			continue;
		return 0;
	}
	return 1;
}

// returns the converted value of a hexadecimal string
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

/* consider making writebyte and writeshort take and return the binlen to avoid external variables */
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
stlen(char *s) { 
	int i = 0;
	for(;(s[i] && s[++i]);) ;
	return i;
}

char *
stcopy(char *src, char *tar, int len) {
	int i=0;
	for(;i<len;i++) tar[i] = src[i];
	src[i+1] = '\0';
	return tar;
}

int
str2op(char* s) {
	lowerize(s);
	UInt8 count = sizeof(ops)/sizeof(ops[0]);
	for(int op=0;op<count;op++) {
		//printf("comparing %s with %s at idx %d\n",ops[op],s,op);
		char *opc = ops[op];
		if(opc[0] == s[0] && opc[1] == s[1] && opc[2] == s[2]) {
			//printf("op found: %d\n",op);
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
addMacro(char *name, FILE *f) {

	int inMacro = 0;
	UInt8 defpos = 0;
	char buf[42];

	if( mcount >= MAXMAC ) {
		fprintf(stderr, "addMacro: Macro buffer full!");
		return -1 ;
	}	

	lowerize(name);
	// add an instance of the macro struct
	memcpy(macros[mcount].name, name, strlen(name)+1);

	// extract each word between the macro delimiters
	while(fscanf(f, "%s", buf) == 1) {
		switch(buf[0]) {
		case '{': inMacro = 1; break;
		case '}': mcount++; return 0;
		default:
			if( !inMacro ) break;
			if( defpos >= MAXDEF ) { 
				fprintf(stderr, "addMacro: Macro definitions buffer full!");
				return -1;
 			}
			// add to macro definition
			memcpy(macros[mcount].defs[defpos++],buf, strlen(buf)+1);
			macros[mcount].defcount++;
			printf("macro %s added %s\n",name, buf);
		}
	}
	return -1;
}

int
macroIdx(char* name) {
	for (int i=0;i<mcount;i++) {
		//printf("comparing %s == %s\n",name,macros[i].name);
		if(strcmp(name, macros[i].name) == 0) {
			//printf("returning %d\n",i);
			return i;
		}
	}
	return -1;
}

char* concat(const char *s1, const char *s2)
{
    const size_t len1 = strlen(s1);
    const size_t len2 = strlen(s2);
    char *result = malloc(len1 + len2 + 1); // +1 for the null-terminator
   	if(result == NULL) return NULL ; 
    memcpy(result, s1, len1);
    memcpy(result + len1, s2, len2 + 1); // +1 to copy the null-terminator
    return result;
}

static char * 
scopedLabel(char *sLabel, const char *label, const char *scope) {
	//char *sLabel = malloc(STRLEN);
	sLabel = concat(scope, concat("/", label));
	return sLabel;
}

int
addLabel(char *name, UInt16 addr) {

	if(labelIdx(name) >= 0) {
		printf("addLabel WARNING: label %s defined twice!\n",name);
		return -1;	
	}
	if(lcount >= MAXLAB) {
		fprintf(stderr, "addLabel: label buffer full!");
		return -1 ;
	}	

	memcpy(labels[lcount].name, name, strlen(name)+1);
	memcpy(&labels[lcount++].addr, &addr, 2);
	printf("added new label %s at address 0x%x\n",labels[lcount-1].name, labels[lcount-1].addr);
	return 0;
}


// used in the second pass to tie labels to addresses
static int
parse(char *buf) {

	int op;
	int lidx;
	UInt16 val;

	//printf("parse buf is %s\n",buf);
	switch(buf[0]) {
		case '^':	// move to position in memory given by absolute argument
			val = hextract(&buf[1]);
			binlen = val;
			break;

		case '&':	// move to position in memory given by relative argument
			val = hextract(&buf[1]);
			binlen += val;
			break;

		case ':': // write address of label at current address without lit opcode
			if((lidx = labelIdx(buf+1)) >= 0) {
				printf("writing raw address in 0x%x to 0x%x\n",binlen,binlen+1);
				writeshort(labels[lidx].addr);
				printf("absolute address of raw label is 0x%x\n", labels[lidx].addr); 
			} 
			break;

		case '_': // write lsb of label address (aka offset from address 0x0000)
			if((lidx = labelIdx(buf+1)) >= 0) {
				writebyte(0x2);	// opcode for .lit
				writebyte(labels[lidx].addr);
				printf("lsb of label address %s is 0x%x\n", labels[lidx].name, (UInt8)labels[lidx].addr); 
			} 
			break;

		case ';': // absolute address
			if((lidx = labelIdx(buf+1)) < 0) { printf("ERROR: parse absolute address label %s not found\n",buf+1); return -1; }
			//printf("writing literal address of %s in 0x%x to 0x%x\n", buf, binlen,binlen+3);
			writebyte(0x22);	// opcode for .lit16
			writeshort(labels[lidx].addr);
			printf("absolute address of literal label %s is 0x%x\n", labels[lidx].name, labels[lidx].addr); 
			break;

		case '#': // literal values given as hex values or as labels to be resolved. 
			val = hextract(&buf[1]);
			// are we writing a byte or a short?
			if(stlen(buf+1) < 3) { 
				writebyte(0x02);		// opcode for .lit
				writebyte(val);
			}
			else {
				writebyte(0x22) ;	// opcode for .lit16
				writeshort(val);
			}
			break;

		case '$':	// relative address given as a label to be resolved and converted to relative distance.
			if((lidx = labelIdx(buf+1)) < 0) { printf("ERROR: parse relative address label %s not found\n",buf); return -1; }
			Label tl = labels[lidx];
			bin[binlen++] = 0x2; // write the lit opcode
			//int offset = tl.addr-(binlen+1);
			int offset = tl.addr-(binlen+2);
			writebyte(offset);// add one to address to account for this writebyte offset.	
			if( (offset < -128) || (offset > 127)) { printf("ERROR: %s offset too large to fit in signed byte\n",tl.name); return -1; }
			printf("relative address of label %s is %d\n", tl.name , tl.addr-(binlen)); 
			break;	
		
	
		case '%': // macro definition
			printf("parse ignoring macro definition: %s\n",buf);
			break;

		default:
			if((op = str2op(buf)) >= 0) {
				//printf("opcode is %d\n",op);
				bin[binlen++] = op;
			} else {
				int midx;
				// if it is a macro, call parse recursively with the contents of the macro.
				if((midx = macroIdx(buf)) >= 0 ) {
					Macro m = macros[midx];
					for (int i=0;i<m.defcount;i++) { 
						if(parse(m.defs[i]) < 0) return -1; 
					}
				} else {		
					// if the buf is a valid hex value store it at binlen as a raw value.
					if(ishex(buf)) {
						val = hextract(buf);
						if(stlen(buf) < 3) writebyte(val);
						else writeshort(val);
						//printf("found raw hex 0x%x",val);
					}
				}
			}
	}
	return 0;
}

// An second pass to ensure that labels can be referenced before they have been defined.
// NB. the binlen must be advanced in step with the mainScan.
static int
linkScan(FILE *f) {

	char buf[STRLEN], scope[STRLEN], sLabel[STRLEN];
	UInt8 inComment = 0;
	UInt8 inMacro = 0;

	binlen = 0; // reset bin length

	printf("------------ LINK-SCAN START ---------------\n");
	while(fscanf(f, "%s", buf) == 1) {

		// skip comments // NB: move this into a function.
		if(buf[0] == '(') { inComment = 1; continue; }
		if(buf[0] == ')') { inComment = 0; continue; }
		if(inComment) continue;	

		// skip macro definition 
		if(buf[0] == '{') { inMacro = 1; continue; }
		if(buf[0] == '}') { inMacro = 0; continue; }
		if(inMacro) continue;	

		if(buf[0] == '@') memcpy(scope, buf+1, STRLEN); 

		// catch scoped labels before we parse
		if(buf[1] == '.' && (buf[0] == '_' || buf[0] == '$' || buf[0] == ';')) {
			printf("caught scoped label %s...\n",buf);
			stcopy(scopedLabel(sLabel, buf+2, scope), buf+1, STRLEN);	
			printf("...and turned it into %s\n",buf);
		}
		if(parse(buf) < 0) return -1;

	}

	printf("------------ LINK-SCAN END ---------------\n");
	return 0;
}


// move the switch in mainScan in here so that macro handling can call this recursively.
static int
range(char *buf) {

	int op, lidx;
	UInt16 val;
	char scope[STRLEN], sLabel[STRLEN];
	// first check for compiler symbols
	switch(buf[0]) {

		case '^': // move to position in memory given by absolute argument
			val = hextract(&buf[1]);
			if(val < binlen) printf("WARNING: setting address backward into existing code.\n");
			binlen = val;
			break;

		case '&':	// move to position in memory given by relative argument
			val = hextract(&buf[1]);
			binlen += val;
			break;

		case ':': // elide short address
			binlen += 2;	// make room for raw address as short 
			break;

		case '.': // scoped label
			if(scope[0] == 0) { printf("ERROR: sublabel defined outside of scope.\n"); return -1; }
			if(addLabel(scopedLabel(sLabel, buf+1, scope), binlen) < 0) { return -1; } 
			break;

		case '_': // elide floored label; lit + byte addr
			if((lidx = labelIdx(buf+1)) >= 0) { binlen += 2; } 
			break;

		case ';': // absolute address
			//printf("range leaving room for literal label %s from 0x%x to 0x%x\n",buf, binlen, binlen+3);
			binlen += 3;	// make room for an opcode and a short
			break;

		case '#': // literal values given as hex values or as labels to be resolved. 
			// is the literal size a byte or a short?
			binlen += (stlen(buf+1) < 3) ? 2 : 3; // advance position in binary by the opcode and the short	

			break;	

		case '$':	// relative address given as a label to be resolved and converted to relative distance.
			binlen += 2;	// make room for opcode and a byte
			break;

		case '@': // 
			printf("Handling label: ");
			// if the label already exists, create a new one using the existing as a namespace.
			// NB: to implement. Needs a string concatenation function.
			// if the label does not exist, create it
			if(addLabel(buf+1, binlen) < 0) { return -1; } 

			// labels define a scope.
			memcpy(scope, buf+1, STRLEN);
			break;	


		default:
			if(str2op(buf) >= 0) { binlen++; } 
			else { 
				int midx;
				if((midx = macroIdx(buf)) >= 0) {
					Macro m = macros[midx];
					for (int i=0;i<m.defcount;i++) { range(m.defs[i]); }
				} else {
					if(ishex(buf)) { 
						binlen += (stlen(buf) < 3) ? 1 : 2; // advance position in binary by the byte or short	
						//printf("link-scan found raw hex 0x%x",val);
					}
				}
			}
	}	
	return 0;
}

// first scan of the file
static int
mainScan(FILE *f) {

	char *sourceline = NULL;
	size_t len = 0;
	ssize_t read;
	char buf[STRLEN];
	Label l;
	UInt8 inComment = 0;

	binlen = 0; // reset bin length
	printf("------------ MAIN SCAN START ---------------\n");
	while(fscanf(f, "%s", buf) == 1) {
		//printf("buf: %s\n", buf);

		// skip comments
		if(buf[0] == '(') { inComment = 1; continue; }
		if(buf[0] == ')') { inComment = 0; continue; }
		if(inComment) continue;	

		if(buf[0] == '%') { // macro definition
			printf("Handling macro: ");
			addMacro(&buf[1], f);
		} else {
			range(buf);
		}
	}

	printf("------------ MAIN SCAN END ---------------\n");
	rewind(f);
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

		if((err = mainScan(f))) {
			fprintf(stderr, "Error %d scanning input\n",err);
			return err;
		}
		printf("main scan was %d bytes\n",binlen);

		if((err = linkScan(f))) {
			fprintf(stderr, "Error %d link-scanning input\n",err);
			return err;
		}
		printf("link scan was %d bytes\n",binlen);

		// write out the bin.
		/*
		printf("the bin is:");
		for(UInt16 i=0;i<binlen;i++) {
			if(i%8 == 0) printf("\n");
			printf("0x%x ",bin[i]);
		}
		*/
		fwrite(bin, binlen, 1, fopen(outfile, "wb"));
		fclose(f);
	}
	return err;
}
