#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include "compiler.h"

static PyObject *py_compile(PyObject *self, PyObject *args) {
    (void)self;
    const char *xml_content;
    const char *base_dir;

    if (!PyArg_ParseTuple(args, "ss", &xml_content, &base_dir))
        return NULL;

    CompileResult result;
    if (compile_qrc(xml_content, base_dir, &result) != 0) {
        PyErr_SetString(PyExc_RuntimeError, result.error);
        return NULL;
    }

    PyObject *data      = PyBytes_FromStringAndSize((char *)result.data_bytes.data,      (Py_ssize_t)result.data_bytes.size);
    PyObject *name      = PyBytes_FromStringAndSize((char *)result.name_bytes.data,      (Py_ssize_t)result.name_bytes.size);
    PyObject *struct_v1 = PyBytes_FromStringAndSize((char *)result.struct_v1_bytes.data, (Py_ssize_t)result.struct_v1_bytes.size);
    PyObject *struct_v2 = PyBytes_FromStringAndSize((char *)result.struct_v2_bytes.data, (Py_ssize_t)result.struct_v2_bytes.size);

    bb_free(&result.data_bytes);
    bb_free(&result.name_bytes);
    bb_free(&result.struct_v1_bytes);
    bb_free(&result.struct_v2_bytes);

    if (!data || !name || !struct_v1 || !struct_v2) {
        Py_XDECREF(data);
        Py_XDECREF(name);
        Py_XDECREF(struct_v1);
        Py_XDECREF(struct_v2);
        return NULL;
    }

    PyObject *dict = PyDict_New();
    PyDict_SetItemString(dict, "data",      data);
    PyDict_SetItemString(dict, "name",      name);
    PyDict_SetItemString(dict, "struct_v1", struct_v1);
    PyDict_SetItemString(dict, "struct_v2", struct_v2);

    Py_DECREF(data);
    Py_DECREF(name);
    Py_DECREF(struct_v1);
    Py_DECREF(struct_v2);

    return dict;
}

static PyMethodDef qres_methods[] = {
    {"compile", py_compile, METH_VARARGS,
     "compile(xml: str, base_dir: str) -> dict\n\n"
     "Compile a .qrc XML string into Qt resource blobs.\n\n"
     "Returns a dict with keys: 'data', 'name', 'struct_v1', 'struct_v2'.\n"
     "base_dir is the directory used to resolve relative file paths in the XML."},
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef qres_module = {
    PyModuleDef_HEAD_INIT,
    "qres._qres",
    "Qt resource compiler C extension.",
    -1,
    qres_methods,
    NULL,
    NULL,
    NULL,
    NULL
};

PyMODINIT_FUNC PyInit__qres(void) {
    return PyModule_Create(&qres_module);
}
