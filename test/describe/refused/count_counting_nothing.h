typedef struct probe_item {
    uint64_t weight;
} probe_item;

typedef struct probe_tree {
    const probe_item *children;
    size_t child_count;
} probe_tree;
