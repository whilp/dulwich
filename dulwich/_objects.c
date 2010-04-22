/* 
 * Copyright (C) 2009 Jelmer Vernooij <jelmer@samba.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License or (at your option) a later version of the License.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301, USA.
 */

#include <Python.h>
#include <stdlib.h>
#include <sys/stat.h>

#define bytehex(x) (((x)<0xa)?('0'+(x)):('a'-0xa+(x)))

size_t strnlen(const char *s, size_t maxlen)
{
	int i;

	for (i = 0; i < maxlen && *s != '\0'; i++, s++)
		;

	return i;
}

static PyObject *sha_to_pyhex(const unsigned char *sha)
{
	char hexsha[41];
	int i;
	for (i = 0; i < 20; i++) {
		hexsha[i*2] = bytehex((sha[i] & 0xF0) >> 4);
		hexsha[i*2+1] = bytehex(sha[i] & 0x0F);
	}
	
	return PyString_FromStringAndSize(hexsha, 40);
}

static PyObject *py_parse_tree(PyObject *self, PyObject *args)
{
	char *text, *start, *end;
	int len, namelen;
	PyObject *ret, *item, *name;

	if (!PyArg_ParseTuple(args, "s#", &text, &len))
		return NULL;

	/* TODO: currently this returns a list; if memory usage is a concern,
	* consider rewriting as a custom iterator object */
	ret = PyList_New(0);

	if (ret == NULL) {
		return NULL;
	}

	start = text;
	end = text + len;

	while (text < end) {
		long mode;
		mode = strtol(text, &text, 8);

		if (*text != ' ') {
			PyErr_SetString(PyExc_ValueError, "Expected space");
			Py_DECREF(ret);
			return NULL;
		}

		text++;

		namelen = strnlen(text, len - (text - start));

		name = PyString_FromStringAndSize(text, namelen);
		if (name == NULL) {
			Py_DECREF(ret);
			return NULL;
		}

		if (text + namelen + 20 >= end) {
			PyErr_SetString(PyExc_ValueError, "SHA truncated");
			Py_DECREF(ret);
			Py_DECREF(name);
			return NULL;
		}

		item = Py_BuildValue("(NlN)", name, mode,
							 sha_to_pyhex((unsigned char *)text+namelen+1));
		if (item == NULL) {
			Py_DECREF(ret);
			Py_DECREF(name);
			return NULL;
		}
		if (PyList_Append(ret, item) == -1) {
			Py_DECREF(ret);
			Py_DECREF(item);
			return NULL;
		}
		Py_DECREF(item);

		text += namelen+21;
	}

	return ret;
}

struct tree_item {
	const char *name;
	int mode;
	PyObject *tuple;
};

int cmp_tree_item(const void *_a, const void *_b)
{
	const struct tree_item *a = _a, *b = _b;
	const char *remain_a, *remain_b;
	int ret, common;
	if (strlen(a->name) > strlen(b->name)) {
		common = strlen(b->name);
		remain_a = a->name + common;
		remain_b = (S_ISDIR(b->mode)?"/":"");
	} else if (strlen(b->name) > strlen(a->name)) { 
		common = strlen(a->name);
		remain_a = (S_ISDIR(a->mode)?"/":"");
		remain_b = b->name + common;
	} else { /* strlen(a->name) == strlen(b->name) */
		common = 0;
		remain_a = a->name;
		remain_b = b->name;
	}
	ret = strncmp(a->name, b->name, common);
	if (ret != 0)
		return ret;
	return strcmp(remain_a, remain_b);
}

static PyObject *py_sorted_tree_items(PyObject *self, PyObject *entries)
{
	struct tree_item *qsort_entries;
	int num, i;
	PyObject *ret;
	Py_ssize_t pos = 0; 
	PyObject *key, *value;

	if (!PyDict_Check(entries)) {
		PyErr_SetString(PyExc_TypeError, "Argument not a dictionary");
		return NULL;
	}

	num = PyDict_Size(entries);
	qsort_entries = malloc(num * sizeof(struct tree_item));
	if (qsort_entries == NULL) {
		PyErr_NoMemory();
		return NULL;
	}

	i = 0;
	while (PyDict_Next(entries, &pos, &key, &value)) {
		PyObject *py_mode, *py_sha;
		
		if (PyTuple_Size(value) != 2) {
			PyErr_SetString(PyExc_ValueError, "Tuple has invalid size");
			free(qsort_entries);
			return NULL;
		}

		py_mode = PyTuple_GET_ITEM(value, 0);
		py_sha = PyTuple_GET_ITEM(value, 1);
		qsort_entries[i].tuple = Py_BuildValue("(OOO)", key, py_mode, py_sha);
		if (!PyString_CheckExact(key)) {
			PyErr_SetString(PyExc_TypeError, "Name is not a string");
			free(qsort_entries);
			return NULL;
		}
		qsort_entries[i].name = PyString_AS_STRING(key);
		if (!PyInt_CheckExact(py_mode)) {
			PyErr_SetString(PyExc_TypeError, "Mode is not an int");
			free(qsort_entries);
			return NULL;
		}
		qsort_entries[i].mode = PyInt_AS_LONG(py_mode);
		i++;
	}

	qsort(qsort_entries, num, sizeof(struct tree_item), cmp_tree_item);

	ret = PyList_New(num);
	if (ret == NULL) {
		free(qsort_entries);
		PyErr_NoMemory();
		return NULL;
	}

	for (i = 0; i < num; i++) {
		PyList_SET_ITEM(ret, i, qsort_entries[i].tuple);
	}

	free(qsort_entries);

	return ret;
}

static PyMethodDef py_objects_methods[] = {
	{ "parse_tree", (PyCFunction)py_parse_tree, METH_VARARGS, NULL },
	{ "sorted_tree_items", (PyCFunction)py_sorted_tree_items, METH_O, NULL },
	{ NULL, NULL, 0, NULL }
};

void init_objects(void)
{
	PyObject *m;

	m = Py_InitModule3("_objects", py_objects_methods, NULL);
	if (m == NULL)
		return;
}
