typedef struct probe_service {
    void *context;
    weivel_status (*run)(void *context);
} probe_service;
