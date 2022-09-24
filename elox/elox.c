#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elox/common.h"
#include "elox/state.h"
#include "elox/util.h"

static void repl(VMCtx *vmCtx) {
	char line[1024];
	for (;;) {
		printf("> ");

		if (!fgets(line, sizeof(line), stdin)) {
			printf("\n");
			break;
		}

		String main = STRING_INITIALIZER("<main>");
		interpret(vmCtx, line, &main);
	}
}

int main(int argc, char **argv) {
	VMCtx vmCtx;
	initVMCtx(&vmCtx);

	if (argc == 1)
		repl(&vmCtx);
	else if (argc == 2)
		eloxRunFile(&vmCtx, argv[1]);
	else {
		fprintf(stderr, "Usage: elox [path]\n");
		exit(64);
	}

	destroyVMCtx(&vmCtx);

	return 0;
}