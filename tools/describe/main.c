#include <stdio.h>
#include <string.h>

#include "allocation.h"
#include "platform.h"
#include "reader.h"
#include "writer.h"

enum { EXIT_DESCRIBED = 0, EXIT_REFUSED = 1, EXIT_USAGE = 2, EXIT_FAILED = 3 };

static const char USAGE[] =
    "usage: weivel-describe --definition <struct> --symbol <function> --prefix <identifier prefix> --output <file.c>\n"
    "                       [--export <macro>] [--status <type>] [--refusal <type>] [--value <type>] [--tree <type>]\n"
    "                       <header>...\n";

typedef struct options {
    describe_service service;
    describe_names names;
    const char *output;
    const char **headers;
    size_t header_count;
} options;

typedef struct option_slot {
    const char *flag;
    const char **slot;
} option_slot;

static int usage(const char *problem) {
    fprintf(stderr, "weivel-describe: %s\n%s", problem, USAGE);
    return EXIT_USAGE;
}

static int take_option(options *chosen, const char *flag, const char *value) {
    option_slot slots[9];
    size_t at;
    slots[0].flag = "--definition";
    slots[0].slot = &chosen->service.definition;
    slots[1].flag = "--symbol";
    slots[1].slot = &chosen->service.symbol;
    slots[2].flag = "--prefix";
    slots[2].slot = &chosen->service.prefix;
    slots[3].flag = "--export";
    slots[3].slot = &chosen->service.export_macro;
    slots[4].flag = "--output";
    slots[4].slot = &chosen->output;
    slots[5].flag = "--status";
    slots[5].slot = &chosen->names.status;
    slots[6].flag = "--refusal";
    slots[6].slot = &chosen->names.refusal;
    slots[7].flag = "--value";
    slots[7].slot = &chosen->names.value;
    slots[8].flag = "--tree";
    slots[8].slot = &chosen->names.tree;
    for (at = 0; at < sizeof slots / sizeof slots[0]; at++) {
        if (strcmp(flag, slots[at].flag) == 0) {
            *slots[at].slot = value;
            return 1;
        }
    }
    return 0;
}

static int read_options(options *chosen, char **arguments, size_t count) {
    size_t at;
    chosen->names = describe_names_of_the_standard();
    for (at = 1; at < count; at++) {
        const char *argument = arguments[at];
        if (strncmp(argument, "--", 2) != 0) {
            chosen->headers[chosen->header_count++] = argument;
        } else if (at + 1 == count) {
            return usage("an option is missing its value");
        } else if (!take_option(chosen, argument, arguments[at + 1])) {
            return usage("an option is not one weivel-describe takes");
        } else {
            at++;
        }
    }
    if (chosen->service.definition == NULL || chosen->service.symbol == NULL || chosen->service.prefix == NULL ||
        chosen->output == NULL) {
        return usage("--definition, --symbol, --prefix and --output are required");
    }
    if (chosen->header_count == 0) {
        return usage("no header was named");
    }
    return EXIT_DESCRIBED;
}

/** @implNote On Windows both separators are one, so a backslash becomes the slash every compiler reads alike. */
static char **include_spellings(const char *const *headers, size_t count) {
    char **includes = weivel_standard_memory_allocate_zeroed(count, sizeof *includes);
    size_t at;
    if (includes == NULL) {
        return NULL;
    }
    for (at = 0; at < count; at++) {
        size_t length = strlen(headers[at]);
        includes[at] = weivel_standard_memory_allocate(length + 1);
        if (includes[at] == NULL) {
            describe_arguments_release(includes, count);
            return NULL;
        }
        memcpy(includes[at], headers[at], length + 1);
#ifdef _WIN32
        {
            char *separator;
            while ((separator = strchr(includes[at], '\\')) != NULL) {
                *separator = '/';
            }
        }
#endif
    }
    return includes;
}

static void print_refusal(const describe_refusal *refusal) {
    if (refusal->file == NULL) {
        fprintf(stderr, "weivel-describe: %s\n", refusal->message);
    } else {
        fprintf(stderr, "%s:%u: %s\n", refusal->file, (unsigned)refusal->line, refusal->message);
    }
}

static int finish(describe_outcome outcome, describe_refusal *refusal, const char *output) {
    if (outcome == DESCRIBE_REFUSED) {
        print_refusal(refusal);
        describe_refusal_release(refusal);
        describe_remove_file(output);
        return EXIT_REFUSED;
    }
    fprintf(stderr, "weivel-describe: out of memory\n");
    return EXIT_FAILED;
}

static int describe(options *chosen) {
    describe_model model;
    describe_refusal refusal;
    describe_text source;
    char **includes;
    describe_outcome outcome = describe_read(&model, chosen->headers, chosen->header_count, &chosen->names, &refusal);
    if (outcome != DESCRIBE_DONE) {
        return finish(outcome, &refusal, chosen->output);
    }
    includes = include_spellings(chosen->headers, chosen->header_count);
    if (includes == NULL) {
        describe_model_release(&model);
        return finish(DESCRIBE_OUT_OF_MEMORY, &refusal, chosen->output);
    }
    chosen->service.includes = (const char *const *)includes;
    chosen->service.include_count = chosen->header_count;
    outcome = describe_write(&model, &chosen->service, &source, &refusal);
    describe_arguments_release(includes, chosen->header_count);
    describe_model_release(&model);
    if (outcome != DESCRIBE_DONE) {
        return finish(outcome, &refusal, chosen->output);
    }
    if (!describe_write_file(chosen->output, source.bytes, source.length)) {
        describe_text_release(&source);
        describe_remove_file(chosen->output);
        fprintf(stderr, "weivel-describe: %s cannot be written\n", chosen->output);
        return EXIT_FAILED;
    }
    describe_text_release(&source);
    return EXIT_DESCRIBED;
}

int main(int argc, char **argv) {
    char **arguments;
    size_t count;
    options chosen;
    int code;
    if (!describe_arguments(argc, argv, &arguments, &count)) {
        fprintf(stderr, "weivel-describe: the arguments cannot be read\n");
        return EXIT_FAILED;
    }
    memset(&chosen, 0, sizeof chosen);
    chosen.headers = count == 0 ? NULL : weivel_standard_memory_allocate_zeroed(count, sizeof *chosen.headers);
    if (count != 0 && chosen.headers == NULL) {
        describe_arguments_release(arguments, count);
        fprintf(stderr, "weivel-describe: out of memory\n");
        return EXIT_FAILED;
    }
    code = read_options(&chosen, arguments, count);
    if (code == EXIT_DESCRIBED) {
        code = describe(&chosen);
    }
    weivel_standard_memory_release((void *)chosen.headers);
    describe_arguments_release(arguments, count);
    return code;
}
