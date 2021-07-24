#include <stdio.h>
#include <string.h>

int
main(int argc, char** argv) {

	if (argc != 3 || strcmp(argv[1],"-f") != 0) printf("Wrong number of arguments");
	else {
		char* filepath = argv[2];	
		FILE *f;
		char ch;

		if(!(f = fopen(filepath, "rb"))) {
			fprintf(stderr, "Error: source file missing\n");
			return 0;
		}
		while((ch = fgetc(f)) != EOF) {
			printf("%c",ch);
		}
		printf("done");
	}
}
