/*
 * Libkmod -- Python interface to kmod API.
 *
 * Copyright (C) 2012 Red Hat, Inc. All rights reserved.
 *
 * This copyrighted material is made available to anyone wishing to use,
 * modify, copy, or redistribute it subject to the terms and conditions
 * of the GNU Lesser General Public License v.2.1.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Andy Grover <agrover redhat com>
 *
 */

#include <Python.h>
#include <libkmod.h>

typedef struct {
    PyObject_HEAD
    struct kmod_ctx *ctx;
} kmodobject;

static PyTypeObject KmodObjType;

static PyObject *KmodError;


/* ----------------------------------------------------------------------
 * kmod object initialization/deallocation
 */

static int
libkmod_init(PyObject *self, PyObject *args, PyObject *kwds)
{
    kmodobject *kmod = (kmodobject *)self;
    char *mod_dir = NULL;

    if (!PyArg_ParseTuple(args, "|s", &mod_dir))
       return -1;

    kmod->ctx = kmod_new(mod_dir, NULL);
    if (!kmod->ctx) {
        PyErr_SetString(KmodError, "Could not initialize");
        return -1;
    }

    kmod_load_resources(kmod->ctx);

    return 0;
}

static void
libkmod_dealloc(PyObject *self)
{
    kmodobject *kmod = (kmodobject *)self;

    kmod_unload_resources(kmod->ctx);

    /* if already closed, don't reclose it */
    if (kmod->ctx != NULL){
	    kmod_unref(kmod->ctx);
    }
    //self->ob_type->tp_free((PyObject*)self);
    PyObject_Del(self);
}

static PyObject *
libkmod_library_get_version(kmodobject *self)
{
    return Py_BuildValue("s", "whatup dude v1.234");
}

static PyObject *
libkmod_kmod_loaded_modules(PyObject *self, PyObject *unused_args)
{
    kmodobject *kmod = (kmodobject *)self;
    struct kmod_list *list, *itr;
    int err;

    err = kmod_module_new_from_loaded(kmod->ctx, &list);
    if (err < 0) {
        PyErr_SetString(KmodError, "Could not get loaded modules");
        return NULL;
    }

    PyObject *pylist = PyList_New(0);

    kmod_list_foreach(itr, list) {
        struct kmod_module *mod = kmod_module_get_module(itr);
        const char *name = kmod_module_get_name(mod);
        long size = kmod_module_get_size(mod);

	PyObject *entry = Py_BuildValue("(sl)", name, size);

	PyList_Append(pylist, entry);

	Py_DECREF(entry);
        kmod_module_unref(mod);
    }
    kmod_module_unref_list(list);

    return pylist;
}

static PyObject *
libkmod_kmod_modprobe(PyObject *self, PyObject *args)
{
    kmodobject *kmod = (kmodobject *)self;
    struct kmod_list *list = NULL, *itr;
    struct kmod_module *mod;
    char *alias_name;
    unsigned int flags = KMOD_PROBE_APPLY_BLACKLIST;
    int err;

    if (!PyArg_ParseTuple(args, "s", &alias_name))
       return NULL;

    err = kmod_module_new_from_lookup(kmod->ctx, alias_name, &list);
    if (err < 0) {
        PyErr_SetString(KmodError, "Could not modprobe");
        return NULL;
    }

    kmod_list_foreach(itr, list) {
        mod = kmod_module_get_module(itr);

	err = kmod_module_probe_insert_module(mod, flags,
	    NULL, NULL, NULL, NULL);

	if (err < 0) {
		if (err == -EEXIST) {
			PyErr_SetString(KmodError, "Module already loaded");
			goto err;
		} else {
			PyErr_SetString(KmodError, "Could not load module");
			goto err;
		}
	}

        kmod_module_unref(mod);
    }
    kmod_module_unref_list(list);

    Py_INCREF(Py_None);
    return Py_None;

err:
    kmod_module_unref(mod);
    kmod_module_unref_list(list);
    return NULL;
}

static PyObject *
libkmod_kmod_rmmod(PyObject *self, PyObject *args)
{
    kmodobject *kmod = (kmodobject *)self;
    struct kmod_module *mod;
    char *module_name;
    int err;

    if (!PyArg_ParseTuple(args, "s", &module_name))
       return NULL;

    err = kmod_module_new_from_name(kmod->ctx, module_name, &mod);
    if (err < 0) {
        PyErr_SetString(KmodError, "Could  get module");
        return NULL;
    }

    err = kmod_module_remove_module(mod, 0);
    if (err < 0) {
	    PyErr_SetString(KmodError, "Could not remove module");
	    kmod_module_unref(mod);
	    return NULL;
    }
    kmod_module_unref(mod);

    Py_INCREF(Py_None);
    return Py_None;
}

/* ----------------------------------------------------------------------
 * Method tables and other bureaucracy
 */

static PyMethodDef Libkmod_methods[] = {
    { "getVersion",          (PyCFunction)libkmod_library_get_version, METH_NOARGS },
    { NULL,             NULL}           /* sentinel */
};

static PyMethodDef kmodobj_methods[] = {
    { "loaded_modules", libkmod_kmod_loaded_modules, METH_NOARGS },
    { "modprobe", libkmod_kmod_modprobe, METH_VARARGS },
    { "rmmod", libkmod_kmod_rmmod, METH_VARARGS },
    { NULL,             NULL}           /* sentinel */
};


static PyTypeObject KmodObjType = {
    PyObject_HEAD_INIT(NULL)
    .tp_name = "kmod.kmod",
    .tp_basicsize = sizeof(kmodobject),
    .tp_new = PyType_GenericNew,
    .tp_init = libkmod_init,
    .tp_dealloc = libkmod_dealloc,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_doc = "Libkmod objects",
    .tp_methods = kmodobj_methods,
};

#ifndef PyMODINIT_FUNC    /* declarations for DLL import/export */
#define PyMODINIT_FUNC void
#endif
PyMODINIT_FUNC
initkmod(void)
{
    PyObject *m;

    if (PyType_Ready(&KmodObjType) < 0)
        return;

    m = Py_InitModule3("kmod", Libkmod_methods, "kmod module");
    if (!m)
        return;

    Py_INCREF(&KmodObjType);
    PyModule_AddObject(m, "Kmod", (PyObject *)&KmodObjType);

    KmodError = PyErr_NewException("kmod.KmodError",
                                   NULL, NULL);
    if (KmodError) {
        /* Each call to PyModule_AddObject decrefs it; compensate: */
        Py_INCREF(KmodError);
        PyModule_AddObject(m, "KmodError", KmodError);
    }

}
