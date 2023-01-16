/**
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "PyLogger.h"
#include "Exception.h"

extern "C" {
namespace org::apache::nifi::minifi::python {

static PyMethodDef PyLogger_methods[] = {
    {"error", (PyCFunction) PyLogger::error, METH_VARARGS, nullptr},
    {"warn", (PyCFunction) PyLogger::warn, METH_VARARGS, nullptr},
    {"info", (PyCFunction) PyLogger::info, METH_VARARGS, nullptr},
    {"debug", (PyCFunction) PyLogger::debug, METH_VARARGS, nullptr},
    {"trace", (PyCFunction) PyLogger::trace, METH_VARARGS, nullptr},
    {}  /* Sentinel */
};

static PyType_Slot PyLoggerTypeSpecSlots[] = {
    {Py_tp_dealloc, reinterpret_cast<void*>(PyLogger::dealloc)},
    {Py_tp_init, reinterpret_cast<void*>(PyLogger::init)},
    {Py_tp_methods, reinterpret_cast<void*>(PyLogger_methods)},
    {Py_tp_new, reinterpret_cast<void*>(PyLogger::newInstance)},
    {}  /* Sentinel */
};

static PyType_Spec PyLoggerTypeSpec{
    .name = "minifi_native.Logger",
    .basicsize = sizeof(PyLogger),
    .itemsize = 0,
    .flags = Py_TPFLAGS_DEFAULT,
    .slots = PyLoggerTypeSpecSlots
};

PyObject* PyLogger::newInstance(PyTypeObject* type, PyObject*, PyObject*) {
  auto self = reinterpret_cast<PyLogger*>(PyType_GenericAlloc(type, 0));
  if (self == nullptr) {
    return nullptr;
  }

  self->logger_.reset();
  return reinterpret_cast<PyObject*>(self);
}

int PyLogger::init(PyLogger* self, PyObject* args, PyObject*) {
  PyObject* weak_ptr_capsule = nullptr;
  // char* keywordArgs[]{ nullptr };
  if (!PyArg_ParseTuple(args, "O", &weak_ptr_capsule)) {
    return -1;
  }

  auto weak_ptr = static_cast<std::weak_ptr<Logger>*>(PyCapsule_GetPointer(weak_ptr_capsule, nullptr));
//   Py_DECREF(weak_ptr_capsule);
  self->logger_ = *weak_ptr;
  return 0;
}

void PyLogger::dealloc(PyLogger* self) {
  self->logger_.reset();
}

PyObject* PyLogger::error(PyLogger* self, PyObject* args) {
  auto logger = self->logger_.lock();
  if (logger == nullptr) {
    PyErr_SetString(PyExc_AttributeError, "internal 'logger' instance is null");
    Py_RETURN_NONE;
  }

  const char* message;
  if (!PyArg_ParseTuple(args, "s", &message)) {
    throw PyException();
  }
  logger->log_error(message);
  Py_RETURN_NONE;
}

PyObject* PyLogger::warn(PyLogger* self, PyObject* args) {
  auto logger = self->logger_.lock();
  if (logger == nullptr) {
    PyErr_SetString(PyExc_AttributeError, "internal 'logger' instance is null");
    Py_RETURN_NONE;
  }

  const char* message;
  if (!PyArg_ParseTuple(args, "s", &message)) {
    throw PyException();
  }
  logger->log_warn(message);
  Py_RETURN_NONE;
}

PyObject* PyLogger::info(PyLogger* self, PyObject* args) {
  auto logger = self->logger_.lock();
  if (logger == nullptr) {
    PyErr_SetString(PyExc_AttributeError, "internal 'logger' instance is null");
    Py_RETURN_NONE;
  }

  const char* message;
  if (!PyArg_ParseTuple(args, "s", &message)) {
    throw PyException();
  }
  logger->log_info(message);
  Py_RETURN_NONE;
}

PyObject* PyLogger::debug(PyLogger* self, PyObject* args) {
  auto logger = self->logger_.lock();
  if (logger == nullptr) {
    PyErr_SetString(PyExc_AttributeError, "internal 'logger' instance is null");
    Py_RETURN_NONE;
  }

  const char* message;
  if (!PyArg_ParseTuple(args, "s", &message)) {
    throw PyException();
  }
  logger->log_debug(message);
  Py_RETURN_NONE;
}

PyObject* PyLogger::trace(PyLogger* self, PyObject* args) {
  auto logger = self->logger_.lock();
  if (logger == nullptr) {
    PyErr_SetString(PyExc_AttributeError, "internal 'logger' instance is null");
    Py_RETURN_NONE;
  }

  const char* message;
  if (!PyArg_ParseTuple(args, "s", &message)) {
    throw PyException();
  }
  logger->log_trace(message);
  Py_RETURN_NONE;
}

PyTypeObject* PyLogger::typeObject() {
  static OwnedObject PyLoggerType{PyType_FromSpec(&PyLoggerTypeSpec)};
  return reinterpret_cast<PyTypeObject*>(PyLoggerType.get());
}
}  // namespace org::apache::nifi::minifi::python
}  // extern "C"
