#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include "compiler.h"

#include <stdlib.h>
#include <string.h>

unsigned int qt_hash(const char *str, unsigned int chained) {
    unsigned int h = chained;
    for (const char *p = str; *p; p++) {
        h = (h << 4) + (unsigned char)(*p);
        unsigned int g = h & 0xF0000000;
        if (g) h ^= g >> 23;
        h &= 0x0FFFFFFF;
    }
    return h;
}

void bb_init(ByteBuffer *bb) {
    bb->data = NULL;
    bb->size = 0;
    bb->capacity = 0;
}

void bb_free(ByteBuffer *bb) {
    free(bb->data);
    bb->data = NULL;
    bb->size = 0;
    bb->capacity = 0;
}

void bb_append(ByteBuffer *bb, const unsigned char *src, size_t len) {
    if (bb->size + len > bb->capacity) {
        size_t new_cap = bb->capacity == 0 ? 1024 : bb->capacity * 2;
        while (bb->size + len > new_cap) new_cap *= 2;
        unsigned char *new_data = (unsigned char *)realloc((void *)bb->data, new_cap);
        if (!new_data) return;
        bb->data = new_data;
        bb->capacity = new_cap;
    }
    memcpy(bb->data + bb->size, src, len);
    bb->size += len;
}

void bb_append_u16be(ByteBuffer *bb, unsigned short val) {
    unsigned char buf[2] = {(unsigned char)((val >> 8) & 0xFF), (unsigned char)(val & 0xFF)};
    bb_append(bb, buf, 2);
}

void bb_append_u32be(ByteBuffer *bb, unsigned int val) {
    unsigned char buf[4] = {
        (unsigned char)((val >> 24) & 0xFF), (unsigned char)((val >> 16) & 0xFF),
        (unsigned char)((val >> 8) & 0xFF),  (unsigned char)(val & 0xFF)
    };
    bb_append(bb, buf, 4);
}

void bb_append_i32be(ByteBuffer *bb, int val) {
    unsigned int u = (unsigned int)val;
    unsigned char buf[4] = {
        (unsigned char)((u >> 24) & 0xFF), (unsigned char)((u >> 16) & 0xFF),
        (unsigned char)((u >> 8) & 0xFF),  (unsigned char)(u & 0xFF)
    };
    bb_append(bb, buf, 4);
}

void bb_append_u64be(ByteBuffer *bb, unsigned long long val) {
    unsigned char buf[8] = {
        (unsigned char)((val >> 56) & 0xFF), (unsigned char)((val >> 48) & 0xFF),
        (unsigned char)((val >> 40) & 0xFF), (unsigned char)((val >> 32) & 0xFF),
        (unsigned char)((val >> 24) & 0xFF), (unsigned char)((val >> 16) & 0xFF),
        (unsigned char)((val >> 8) & 0xFF),  (unsigned char)(val & 0xFF)
    };
    bb_append(bb, buf, 8);
}

ResourceNode *create_node(const char *name, int is_dir, PyObject *data_bytes) {
    ResourceNode *node = (ResourceNode *)calloc(1, sizeof(ResourceNode));
    if (!node) return NULL;
    node->name = strdup(name);
    node->is_dir = is_dir;
    if (data_bytes) {
        node->data_bytes = data_bytes;
        Py_INCREF(data_bytes);
    }
    return node;
}

void free_node(ResourceNode *node) {
    if (!node) return;
    for (int i = 0; i < node->child_count; i++) {
        free_node(node->children[i]);
    }
    free((void *)node->children);
    free(node->name);
    Py_XDECREF(node->data_bytes);
    free(node);
}

static ResourceNode *find_child(ResourceNode *parent, const char *name, int is_dir) {
    for (int i = 0; i < parent->child_count; i++) {
        if (parent->children[i]->is_dir == is_dir &&
            strcmp(parent->children[i]->name, name) == 0) {
            return parent->children[i];
        }
    }
    return NULL;
}

static void add_child(ResourceNode *parent, ResourceNode *child) {
    if (parent->child_count >= parent->child_capacity) {
        int new_cap = parent->child_capacity == 0 ? 4 : parent->child_capacity * 2;
        ResourceNode **new_children = (ResourceNode **)realloc((void *)parent->children, sizeof(ResourceNode *) * (size_t)new_cap);
        if (!new_children) return;
        parent->children = new_children;
        parent->child_capacity = new_cap;
    }
    parent->children[parent->child_count++] = child;
}

static void add_to_tree(ResourceNode *root, const char *virtual_path, PyObject *data_bytes, long long lastmod) {
    char *temp = strdup(virtual_path);
    if (!temp) return;

    char *p = temp;
    while (*p == '/') p++;

    char *tok = strtok(p, "/");
    if (!tok) {
        free(temp);
        return;
    }

    int capacity = 8;
    int part_count = 0;
    char **parts = (char **)malloc(sizeof(char *) * (size_t)capacity);
    if (!parts) {
        free(temp);
        return;
    }

    while (tok) {
        if (part_count >= capacity) {
            capacity *= 2;
            char **new_parts = (char **)realloc(parts, sizeof(char *) * (size_t)capacity);
            if (!new_parts) {
                free(parts);
                free(temp);
                return;
            }
            parts = new_parts;
        }
        parts[part_count++] = tok;
        tok = strtok(NULL, "/");
    }

    ResourceNode *current = root;
    for (int i = 0; i < part_count - 1; i++) {
        ResourceNode *child = find_child(current, parts[i], 1);
        if (!child) {
            child = create_node(parts[i], 1, NULL);
            if (!child) {
                free(parts);
                free(temp);
                return;
            }
            add_child(current, child);
        }
        current = child;
    }

    ResourceNode *file_node = create_node(parts[part_count - 1], 0, data_bytes);
    if (file_node) {
        file_node->lastmod = lastmod;
        add_child(current, file_node);
    }

    free(parts);
    free(temp);
}

static int parse_qrc(const char *xml_content, ResourceNode *root, const char *base_dir, char *err_buf, size_t err_size) {
    PyObject *etree = PyImport_ImportModule("xml.etree.ElementTree");
    if (!etree) {
        snprintf(err_buf, err_size, "Failed to import xml.etree.ElementTree");
        return -1;
    }

    PyObject *root_elem = PyObject_CallMethod(etree, "fromstring", "s", xml_content);
    if (!root_elem) {
        PyObject *ptype, *pvalue, *ptrace;
        PyErr_Fetch(&ptype, &pvalue, &ptrace);
        if (pvalue) {
            PyObject *pstr = PyObject_Str(pvalue);
            if (pstr) {
                snprintf(err_buf, err_size, "XML syntax error: %s", PyUnicode_AsUTF8(pstr));
                Py_DECREF(pstr);
            }
            Py_DECREF(pvalue);
        }
        Py_XDECREF(ptype);
        Py_XDECREF(ptrace);
        Py_DECREF(etree);
        return -1;
    }

    PyObject *pathlib = PyImport_ImportModule("pathlib");
    if (!pathlib) {
        Py_DECREF(root_elem);
        Py_DECREF(etree);
        snprintf(err_buf, err_size, "Failed to import pathlib");
        return -1;
    }

    PyObject *path_cls = PyObject_GetAttrString(pathlib, "Path");
    PyObject *base_path_obj = PyObject_CallFunction(path_cls, "s", base_dir);
    Py_DECREF(path_cls);

    PyObject *qresources = PyObject_CallMethod(root_elem, "findall", "s", "qresource");
    if (!qresources) {
        Py_DECREF(base_path_obj);
        Py_DECREF(pathlib);
        Py_DECREF(root_elem);
        Py_DECREF(etree);
        return -1;
    }

    Py_ssize_t qres_count = PyList_Size(qresources);
    for (Py_ssize_t i = 0; i < qres_count; i++) {
        PyObject *qres = PyList_GetItem(qresources, i);
        PyObject *prefix_obj = PyObject_CallMethod(qres, "get", "ss", "prefix", "");
        const char *raw_prefix = prefix_obj ? PyUnicode_AsUTF8(prefix_obj) : "";
        while (*raw_prefix == '/') raw_prefix++;

        char clean_prefix[256];
        strncpy(clean_prefix, raw_prefix, sizeof(clean_prefix) - 1);
        clean_prefix[sizeof(clean_prefix) - 1] = '\0';
        size_t plen = strlen(clean_prefix);
        while (plen > 0 && clean_prefix[plen - 1] == '/') {
            clean_prefix[--plen] = '\0';
        }

        PyObject *files = PyObject_CallMethod(qres, "findall", "s", "file");
        if (!files) {
            Py_XDECREF(prefix_obj);
            continue;
        }

        Py_ssize_t file_count = PyList_Size(files);
        for (Py_ssize_t j = 0; j < file_count; j++) {
            PyObject *file_item = PyList_GetItem(files, j);
            PyObject *text_obj = PyObject_GetAttrString(file_item, "text");
            if (!text_obj || text_obj == Py_None) {
                Py_XDECREF(text_obj);
                continue;
            }

            PyObject *stripped_text = PyObject_CallMethod(text_obj, "strip", NULL);
            Py_DECREF(text_obj);
            if (!stripped_text) continue;

            const char *rel_path = PyUnicode_AsUTF8(stripped_text);
            if (!rel_path || strlen(rel_path) == 0) {
                Py_DECREF(stripped_text);
                continue;
            }

            PyObject *file_path_obj = PyObject_CallMethod(base_path_obj, "joinpath", "s", rel_path);
            PyObject *data_bytes = PyObject_CallMethod(file_path_obj, "read_bytes", NULL);
            if (!data_bytes) {
                PyErr_Clear();
                PyObject *path_str = PyObject_Str(file_path_obj);
                snprintf(err_buf, err_size, "Cannot open file: %s", path_str ? PyUnicode_AsUTF8(path_str) : rel_path);
                Py_XDECREF(path_str);
                Py_DECREF(file_path_obj);
                Py_DECREF(stripped_text);
                Py_DECREF(files);
                Py_XDECREF(prefix_obj);
                Py_DECREF(qresources);
                Py_DECREF(base_path_obj);
                Py_DECREF(pathlib);
                Py_DECREF(root_elem);
                Py_DECREF(etree);
                return -1;
            }

            long long lastmod = 0;
            PyObject *stat_obj = PyObject_CallMethod(file_path_obj, "stat", NULL);
            if (stat_obj) {
                PyObject *mtime_obj = PyObject_GetAttrString(stat_obj, "st_mtime");
                if (mtime_obj) {
                    double mtime = PyFloat_AsDouble(mtime_obj);
                    lastmod = (long long)(mtime * 1000.0);
                    Py_DECREF(mtime_obj);
                }
                Py_DECREF(stat_obj);
            }
            Py_DECREF(file_path_obj);

            PyObject *alias_obj = PyObject_CallMethod(file_item, "get", "s", "alias");
            const char *target = (alias_obj && alias_obj != Py_None) ? PyUnicode_AsUTF8(alias_obj) : rel_path;
            while (*target == '/') target++;

            size_t vpath_len = plen + strlen(target) + 2;
            char *vpath = (char *)malloc(vpath_len);
            if (vpath) {
                if (plen > 0) {
                    snprintf(vpath, vpath_len, "%s/%s", clean_prefix, target);
                } else {
                    snprintf(vpath, vpath_len, "%s", target);
                }
                for (char *c = vpath; *c; c++) {
                    if (*c == '\\') *c = '/';
                }
                add_to_tree(root, vpath, data_bytes, lastmod);
                free(vpath);
            }

            Py_XDECREF(alias_obj);
            Py_DECREF(data_bytes);
            Py_DECREF(stripped_text);
        }
        Py_DECREF(files);
        Py_XDECREF(prefix_obj);
    }

    Py_DECREF(qresources);
    Py_DECREF(base_path_obj);
    Py_DECREF(pathlib);
    Py_DECREF(root_elem);
    Py_DECREF(etree);
    return 0;
}

static int compare_nodes(const void *a, const void *b) {
    ResourceNode *const *node_a = (ResourceNode *const *)a;
    ResourceNode *const *node_b = (ResourceNode *const *)b;
    unsigned int hash_a = qt_hash((*node_a)->name, 0);
    unsigned int hash_b = qt_hash((*node_b)->name, 0);
    if (hash_a < hash_b) return -1;
    if (hash_a > hash_b) return 1;
    return strcmp((*node_a)->name, (*node_b)->name);
}

static void sort_tree(ResourceNode *node) {
    if (node->child_count > 0) {
        qsort((void *)node->children, (size_t)node->child_count, sizeof(ResourceNode *), compare_nodes);
        for (int i = 0; i < node->child_count; i++) {
            sort_tree(node->children[i]);
        }
    }
}

static void flatten_tree(ResourceNode *root, FlatList *fl) {
    int queue_capacity = 1000;
    ResourceNode **queue = (ResourceNode **)malloc(sizeof(ResourceNode *) * (size_t)queue_capacity);
    if (!queue) return;
    int head = 0, tail = 0;
    queue[tail++] = root;

    while (head < tail) {
        ResourceNode *curr = queue[head++];

        if (fl->count >= fl->capacity) {
            int new_cap = fl->capacity == 0 ? 1000 : fl->capacity * 2;
            ResourceNode **new_nodes = (ResourceNode **)realloc((void *)fl->nodes, sizeof(ResourceNode *) * (size_t)new_cap);
            if (!new_nodes) {
                free((void *)queue);
                return;
            }
            fl->nodes = new_nodes;
            fl->capacity = new_cap;
        }
        fl->nodes[fl->count++] = curr;

        for (int i = 0; i < curr->child_count; i++) {
            if (tail >= queue_capacity) {
                int new_q_cap = queue_capacity * 2;
                ResourceNode **new_q = (ResourceNode **)realloc((void *)queue, sizeof(ResourceNode *) * (size_t)new_q_cap);
                if (!new_q) {
                    free((void *)queue);
                    return;
                }
                queue = new_q;
                queue_capacity = new_q_cap;
            }
            queue[tail++] = curr->children[i];
        }
    }
    free((void *)queue);
}

int compile_qrc(const char *xml_content, const char *base_dir, CompileResult *out) {
    bb_init(&out->name_bytes);
    bb_init(&out->data_bytes);
    bb_init(&out->struct_v1_bytes);
    bb_init(&out->struct_v2_bytes);
    out->error[0] = '\0';

    ResourceNode *root = create_node("", 1, NULL);
    if (!root) {
        snprintf(out->error, sizeof(out->error), "Failed to allocate root node");
        return -1;
    }

    if (parse_qrc(xml_content, root, base_dir, out->error, sizeof(out->error)) != 0) {
        free_node(root);
        return -1;
    }

    sort_tree(root);

    FlatList fl = {NULL, 0, 0};
    flatten_tree(root, &fl);

    for (int i = 0; i < fl.count; i++) {
        ResourceNode *node = fl.nodes[i];
        if (i == 0) {
            node->name_offset = 0;
            continue;
        }
        node->name_offset = (int)out->name_bytes.size;

        PyObject *py_name = PyUnicode_FromString(node->name);
        if (!py_name) {
            snprintf(out->error, sizeof(out->error), "Invalid name: %s", node->name);
            free((void *)fl.nodes);
            free_node(root);
            return -1;
        }

        PyObject *u16_bytes = PyUnicode_AsEncodedString(py_name, "utf-16be", "strict");
        Py_DECREF(py_name);
        if (!u16_bytes) {
            snprintf(out->error, sizeof(out->error), "Failed to encode name to UTF-16: %s", node->name);
            free((void *)fl.nodes);
            free_node(root);
            return -1;
        }

        char *u16_data = NULL;
        Py_ssize_t u16_len = 0;
        PyBytes_AsStringAndSize(u16_bytes, &u16_data, &u16_len);

        unsigned short u16_units = (unsigned short)(u16_len / 2);
        bb_append_u16be(&out->name_bytes, u16_units);
        bb_append_u32be(&out->name_bytes, qt_hash(node->name, 0));
        bb_append(&out->name_bytes, (const unsigned char *)u16_data, (size_t)u16_len);
        Py_DECREF(u16_bytes);
    }

    PyObject *zlib_mod = PyImport_ImportModule("zlib");
    if (!zlib_mod) {
        snprintf(out->error, sizeof(out->error), "Failed to import Python zlib module");
        free((void *)fl.nodes);
        free_node(root);
        return -1;
    }

    for (int i = 0; i < fl.count; i++) {
        ResourceNode *node = fl.nodes[i];
        if (node->is_dir) continue;

        node->data_offset = (int)out->data_bytes.size;

        char *raw_data = NULL;
        Py_ssize_t raw_size = 0;
        PyBytes_AsStringAndSize(node->data_bytes, &raw_data, &raw_size);

        PyObject *comp_obj = PyObject_CallMethod(zlib_mod, "compress", "y#i", raw_data, raw_size, 9);
        if (!comp_obj) {
            PyErr_Clear();
            snprintf(out->error, sizeof(out->error), "Failed to compress file data: %s", node->name);
            Py_DECREF(zlib_mod);
            free((void *)fl.nodes);
            free_node(root);
            return -1;
        }

        char *comp_data = NULL;
        Py_ssize_t comp_size = 0;
        PyBytes_AsStringAndSize(comp_obj, &comp_data, &comp_size);

        size_t name_len = strlen(node->name);
        int is_ico = (name_len >= 4 && strcmp(node->name + name_len - 4, ".ico") == 0);

        if (is_ico || comp_size >= raw_size) {
            node->flags = 0x0000;
            bb_append_u32be(&out->data_bytes, (unsigned int)raw_size);
            bb_append(&out->data_bytes, (const unsigned char *)raw_data, (size_t)raw_size);
        } else {
            node->flags = 0x0001;
            unsigned int payload_size = (unsigned int)comp_size + 4;
            bb_append_u32be(&out->data_bytes, payload_size);
            bb_append_u32be(&out->data_bytes, (unsigned int)raw_size);
            bb_append(&out->data_bytes, (const unsigned char *)comp_data, (size_t)comp_size);
        }

        Py_DECREF(comp_obj);
    }

    Py_DECREF(zlib_mod);

    for (int i = 0; i < fl.count; i++) {
        ResourceNode *node = fl.nodes[i];
        unsigned int mix, offset = 0;

        if (node->is_dir) {
            node->flags = 2;
            mix = (unsigned int)node->child_count;
            if (node->child_count > 0) {
                ResourceNode *first_child = node->children[0];
                for (int j = 0; j < fl.count; j++) {
                    if (fl.nodes[j] == first_child) {
                        offset = (unsigned int)j;
                        break;
                    }
                }
            }
        } else {
            mix = 1;
            offset = (unsigned int)node->data_offset;
        }

        bb_append_u32be(&out->struct_v1_bytes, (unsigned int)node->name_offset);
        bb_append_u16be(&out->struct_v1_bytes, node->flags);
        bb_append_u32be(&out->struct_v1_bytes, mix);
        bb_append_u32be(&out->struct_v1_bytes, offset);

        bb_append_u32be(&out->struct_v2_bytes, (unsigned int)node->name_offset);
        bb_append_u16be(&out->struct_v2_bytes, node->flags);
        bb_append_u32be(&out->struct_v2_bytes, mix);
        bb_append_i32be(&out->struct_v2_bytes, (int)offset);
        bb_append_u64be(&out->struct_v2_bytes, (unsigned long long)node->lastmod);
    }

    free((void *)fl.nodes);
    free_node(root);
    return 0;
}
