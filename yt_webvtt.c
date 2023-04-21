/* rm -rf PackageName.egg-info build dist
 * pip install .
 * python setup.py build && python setup.py install
 */

#define PY_SSIZE_T_CLEAN
#define Py_LIMITED_API 0x030A0000 /* 3.10 */
#include <Python.h>

#include "yt_webvtt_impl.c"

static PyObject *cReadWebVTT(PyObject *self, PyObject *args)
{
	PyObject *input_str_obj = NULL;
	const char *input_str = NULL;
	Py_ssize_t intput_str_len = 0;

	PyObject *res_obj = NULL;
	PyObject *clean_str_obj = NULL;
	PyObject *pos_and_time_obj = NULL;
	void *scratch_memory = NULL;
	int set_item_res = 0;

	if (!PyArg_ParseTuple(args, "U", &input_str_obj)) {
		goto error;
	}
	if ((input_str = PyUnicode_AsUTF8AndSize(input_str_obj, &intput_str_len)) == NULL) {
		goto error;
	}

	scratch_memory = calloc(intput_str_len, sizeof (char));
	if (scratch_memory == NULL) {
		// NOTE: Is this really needed since we are not using a python "branded"
		// allocator?
		PyErr_NoMemory();
		goto error;
	}
	// Assuming that this is the result.
	char clean_str[5] = "Hello";
	size_t clean_str_len = 5;
	unsigned int pos_and_time[10] = {
		0, 1,
		1, 2,
		2, 3,
		3, 4,
		4, 5
	};

	free(scratch_memory), scratch_memory = NULL;

	res_obj = PyTuple_New(2);
	if (res_obj == NULL) {
		goto error;
	}

	clean_str_obj = Py_BuildValue("s#", clean_str, clean_str_len);
	if (clean_str_obj == NULL) {
		goto error;
	}
	set_item_res = PyTuple_SetItem(res_obj, 0, clean_str_obj);
	assert(set_item_res != -1);

	pos_and_time_obj = PyList_New(10);
	if (pos_and_time_obj == NULL) {
		goto error;
	}
	set_item_res = PyTuple_SetItem(res_obj, 1, pos_and_time_obj);
	assert(set_item_res != -1);

	for (int i = 0; i < 10; i++) {
		PyObject *num = PyLong_FromUnsignedLong(pos_and_time[i]);
		if (num == NULL) {
			goto error;
		}
		set_item_res = PyList_SetItem(pos_and_time_obj, i, num);
		assert(set_item_res != -1);
	}

	return res_obj;
error:
	free(scratch_memory);
	Py_DecRef(res_obj);
	Py_DecRef(clean_str_obj);
	Py_DecRef(pos_and_time_obj);
	return NULL;
}

/* https://docs.python.org/3/c-api/structures.html#c.PyMethodDef */
static PyMethodDef myMethods[] = {
	{"read_webvtt", cReadWebVTT, METH_VARARGS, "Calculate the Fibonacci numbers."},
	{NULL, NULL, 0, NULL}
};

/* https://docs.python.org/3/c-api/module.html#c.PyModuleDef */
static struct PyModuleDef my_module = {
	.m_base = PyModuleDef_HEAD_INIT,
	.m_name = "yt_webvtt",
	.m_doc = "Parsing and cleaning automatically generater YouTube's WebVTT "
		"comment files.",
	.m_size = -1,
	.m_methods = myMethods
};

PyMODINIT_FUNC PyInit_yt_webvtt(void)
{
	return PyModule_Create(&my_module);
}
