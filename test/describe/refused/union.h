typedef struct probe_box {
    uint32_t kind;
    union {
        uint32_t number;
    } held;
} probe_box;
