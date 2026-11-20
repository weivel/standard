typedef struct probe_item {
    uint64_t weight;
} probe_item;

typedef struct probe_list {
    const probe_item *items;
    size_t item_count;
    size_t count;
} probe_list;
