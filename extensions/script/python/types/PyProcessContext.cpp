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
a * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "PyProcessContext.h"
#include <string>
#include "PyException.h"

extern "C" {
namespace org::apache::nifi::minifi::python {

static PyMethodDef PyProcessContext_methods[] = {
    {"getProperty", (PyCFunction) PyProcessContext::getProperty, METH_VARARGS, nullptr},
    {}  /* Sentinel */
};

static PyType_Slot PyProcessContextTypeSpecSlots[] = {
    {Py_tp_dealloc, reinterpret_cast<void*>(PyProcessContext::dealloc)},
    {Py_tp_init, reinterpret_cast<void*>(PyProcessContext::init)},
    {Py_tp_methods, reinterpret_cast<void*>(PyProcessContext_methods)},
    {Py_tp_new, reinterpret_cast<void*>(PyProcessContext::newInstance)},
    {}  /* Sentinel */
};

static PyType_Spec PyProcessContextTypeSpec{
    .name = "minifi_native.ProcessContext",
    .basicsize = sizeof(PyProcessContext),
    .itemsize = 0,
    .flags = Py_TPFLAGS_DEFAULT,
    .slots = PyProcessContextTypeSpecSlots
};

PyObject* PyProcessContext::newInstance(PyTypeObject* type, PyObject*, PyObject*) {
  auto self = reinterpret_cast<PyProcessContext*>(PyType_GenericAlloc(type, 0));
  if (self == nullptr) {
    return nullptr;
  }

  self->process_context_.reset();
  return reinterpret_cast<PyObject*>(self);
}

int PyProcessContext::init(PyProcessContext* self, PyObject* args, PyObject*) {
  PyObject* weak_ptr_capsule = nullptr;
  if (!PyArg_ParseTuple(args, "O", &weak_ptr_capsule)) {
    return -1;
  }

  auto process_context = static_cast<HeldType*>(PyCapsule_GetPointer(weak_ptr_capsule, nullptr));
  // Py_DECREF(weak_ptr_capsule);
  self->process_context_ = *process_context;
  return 0;
}

void PyProcessContext::dealloc(PyProcessContext* self) {
  self->process_context_.reset();
}

PyObject* PyProcessContext::getProperty(PyProcessContext* self, PyObject* args) {
  auto context = self->process_context_.lock();
  if (!context) {
    PyErr_SetString(PyExc_AttributeError, "tried reading process context outside 'on_trigger'");
    return nullptr;
  }

  const char* property;
  if (!PyArg_ParseTuple(args, "s", &property)) {
    throw PyException();
  }
  return object::returnReference(context->getProperty(std::string(property)));
}

PyTypeObject* PyProcessContext::typeObject() {
  static OwnedObject PyProcessContextType{PyType_FromSpec(&PyProcessContextTypeSpec)};
  return reinterpret_cast<PyTypeObject*>(PyProcessContextType.get());
}

}  // namespace org::apache::nifi::minifi::python
}  // extern "C"
