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
		const char* strlit = "6 7 + pop";

		if(!(f = fopen(filepath, "rb"))) {
			fprintf(stderr, "Error: source file missing\n");
			return 0;
		}
		//while((ch = fgetc(f)) != EOF) {
		while((read = getline(&sourceline, &len, f) != EOF)) {
			printf("%s",sourceline);
		}


		// clean up.
		fclose(f);
		if(sourceline) free(sourceline);
		printf("done");
		exit(EXIT_SUCCESS);
	}
}
