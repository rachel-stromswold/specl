#include <math.h>
#include "speclang.h"

int main(int argc, char** argv) {
    if (argc < 2)
	fprintf(stderr, "usage: spcl <filename>\n");
    char* fname = argv[1];
    //iterate through the remaining arguments to find a call to --args
    char** call_argv = NULL;
    int args_start = argc;
    for (int i = 2; i < argc; ++i) {
	if (strcmp(argv[i], "--args") == 0) {
	    if (i+1 >= argc) {
		fprintf(stderr, "expected arguments after --args\n");
		return 1;
	    }
	    args_start = i+1;
	    break;
	}
    }
    //actually call
    spcl_val er;
    spcl_inst* c = spcl_inst_from_file(fname, &er, argc-args_start, argv+args_start);
    if (er.type == VAL_ERR) {
	cleanup_spcl_val(&er);
	destroy_spcl_inst(c);
	return 1;
    }
    cleanup_spcl_val(&er);
    destroy_spcl_inst(c);
    return 0;
}
