typedef enum probe_step_kind { PROBE_PROVIDER = 0, PROBE_PACKAGE = 1 } probe_step_kind;

/** A probe_step_kind: a provider step installs `provider`; a package step installs `package`. */
typedef struct probe_step {
    uint32_t kind;
    const char *provider;
    const char *package;
} probe_step;
