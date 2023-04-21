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
	const char *input_str = NULL;
	Py_ssize_t intput_str_len = 0;

	PyObject *res_obj = NULL;
	PyObject *clean_str_obj = NULL;
	PyObject *pos_and_time_obj = NULL;
	void *scratch_memory = NULL;
	int set_item_res = 0;

	if (PyArg_ParseTuple(args, "s#", &input_str, &intput_str_len) == NULL) {
		goto error;
	}

	char *clean_str = NULL;
	size_t clean_str_len = 0;
	Webvtt_timestap *pos_and_time = NULL;
	size_t pos_and_time_len = 0;
	{
		long gibibyte = 1L << 30;
		scratch_memory = calloc(gibibyte, sizeof (char));
		if (scratch_memory == NULL) {
			// NOTE: Is this really needed since we are not using a python
			// "branded" allocator?
			// PyErr_NoMemory();
			goto error;
		}
		Str file = {.chars = input_str, .len = intput_str_len};
		Buf arena_allocator = {.mem = scratch_memory, .size = gibibyte, .used = 0};

		Webvtt_error res = read_webvtt(
			file, &arena_allocator,
			&clean_str, &clean_str_len,
			&pos_and_time, &pos_and_time_len);
		if (res != WEBVTT_OK) {
			goto error;
		}
	}

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

	pos_and_time_obj = PyList_New(pos_and_time_len*2);
	if (pos_and_time_obj == NULL) {
		goto error;
	}
	set_item_res = PyTuple_SetItem(res_obj, 1, pos_and_time_obj);
	assert(set_item_res != -1);

	for (size_t i = 0; i < pos_and_time_len; i++) {
		PyObject *pos = PyLong_FromSize_t(pos_and_time[i].pos);
		if (pos == NULL) {
			goto error;
		}
		PyObject *time = PyLong_FromUnsignedLong(pos_and_time[i].time_ms);
		if (time == NULL) {
			goto error;
		}
		set_item_res = PyList_SetItem(pos_and_time_obj, i*2, pos);
		assert(set_item_res != -1);
		set_item_res = PyList_SetItem(pos_and_time_obj, i*2+1, time);
		assert(set_item_res != -1);
	}

	free(scratch_memory);
	return res_obj;
error:
	free(scratch_memory);
	Py_DecRef(res_obj);
	Py_DecRef(clean_str_obj);
	Py_DecRef(pos_and_time_obj);
	Py_RETURN_NONE;
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
