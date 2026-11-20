typedef struct probe_item {
    uint64_t weight;
} probe_item;

typedef struct probe_list {
    const char *const *tags;
    const probe_item *items;
    size_t count;
} probe_list;
