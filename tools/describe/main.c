#include <stdio.h>

#include "platform.h"
#include "program.h"

int main(int argc, char **argv) {
    char **arguments;
    size_t count;
    int code;
    if (!describe_arguments(argc, argv, &arguments, &count)) {
        fprintf(stderr, "weivel-describe: the arguments cannot be read\n");
        return DESCRIBE_EXIT_FAILED;
    }
    code = describe_program(arguments, count);
    describe_arguments_release(arguments, count);
    return code;
}
