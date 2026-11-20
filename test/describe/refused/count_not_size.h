typedef struct probe_item {
    uint64_t weight;
} probe_item;

typedef struct probe_list {
    const probe_item *items;
    uint32_t item_count;
} probe_list;
