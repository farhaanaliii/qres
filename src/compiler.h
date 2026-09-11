#ifndef QRES_COMPILER_H
#define QRES_COMPILER_H

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <stddef.h>

#ifdef _MSC_VER
#define strdup _strdup
#endif

typedef struct ResourceNode {
    char *name;
    int is_dir;
    PyObject *data_bytes;
    long long lastmod;
    struct ResourceNode **children;
    int child_count;
    int child_capacity;
    int name_offset;
    int data_offset;
    unsigned short flags;
} ResourceNode;

typedef struct {
    unsigned char *data;
    size_t size;
    size_t capacity;
} ByteBuffer;

typedef struct {
    ByteBuffer name_bytes;
    ByteBuffer data_bytes;
    ByteBuffer struct_v1_bytes;
    ByteBuffer struct_v2_bytes;
    char error[512];
} CompileResult;

typedef struct {
    ResourceNode **nodes;
    int count;
    int capacity;
} FlatList;


unsigned int qt_hash(const char *str, unsigned int chained);

void bb_init(ByteBuffer *bb);
void bb_free(ByteBuffer *bb);
void bb_append(ByteBuffer *bb, const unsigned char *src, size_t len);
void bb_append_u16be(ByteBuffer *bb, unsigned short val);
void bb_append_u32be(ByteBuffer *bb, unsigned int val);
void bb_append_i32be(ByteBuffer *bb, int val);
void bb_append_u64be(ByteBuffer *bb, unsigned long long val);

ResourceNode *create_node(const char *name, int is_dir, PyObject *data_bytes);
void free_node(ResourceNode *node);

int compile_qrc(const char *xml_content, const char *base_dir, CompileResult *out);

#endif
