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

#include "PyStateManager.h"

extern "C" {
namespace org::apache::nifi::minifi::extensions::python {

static PyMethodDef PyStateManager_methods[] = {  // NOLINT(cppcoreguidelines-avoid-c-arrays)
    {"get", (PyCFunction) PyStateManager::get, METH_VARARGS, nullptr},
    {"set", (PyCFunction) PyStateManager::set, METH_VARARGS, nullptr},
    {"clear", (PyCFunction) PyStateManager::clear, METH_VARARGS, nullptr},
    {"replace", (PyCFunction) PyStateManager::replace, METH_VARARGS, nullptr},
    {}  /* Sentinel */
};

static PyType_Slot PyStateManagerTypeSpecSlots[] = {  // NOLINT(cppcoreguidelines-avoid-c-arrays)
    {Py_tp_dealloc, reinterpret_cast<void*>(pythonAllocatedInstanceDealloc<PyStateManager>)},
    {Py_tp_init, reinterpret_cast<void*>(PyStateManager::init)},
    {Py_tp_methods, reinterpret_cast<void*>(PyStateManager_methods)},
    {Py_tp_new, reinterpret_cast<void*>(newPythonAllocatedInstance<PyStateManager>)},
    {}  /* Sentinel */
};

static PyType_Spec PyStateManagerTypeSpec{
    .name = "minifi_native.StateManager",
    .basicsize = sizeof(PyStateManager),
    .itemsize = 0,
    .flags = Py_TPFLAGS_DEFAULT,
    .slots = PyStateManagerTypeSpecSlots
};

int PyStateManager::init(PyStateManager* self, PyObject* args, PyObject*) {
  PyObject* weak_ptr_capsule = nullptr;
  if (!PyArg_ParseTuple(args, "O", &weak_ptr_capsule)) {
    return -1;
  }

  auto state_manager = PyCapsule_GetPointer(weak_ptr_capsule, HeldTypeName);
  if (!state_manager)
    return -1;
  self->state_manager_ = *static_cast<HeldType*>(state_manager);
  return 0;
}

PyObject* PyStateManager::set(PyStateManager* self, PyObject* args) {
  if (!self->state_manager_) {
    PyErr_SetString(PyExc_AttributeError, "tried reading state manager outside 'on_trigger'");
    return nullptr;
  }
  core::StateManager::State cpp_state;

  auto python_state = BorrowedDict::fromTuple(args, 0);

  auto python_state_keys = OwnedList(PyDict_Keys(python_state.get()));
  for (size_t i = 0; i < python_state_keys.length(); ++i) {
    BorrowedStr key{python_state_keys[i]};
    if (auto value = python_state[key.toUtf8String()]) {
      BorrowedStr value_str{*value};
      cpp_state[key.toUtf8String()] = value_str.toUtf8String();
    }
  }

  return object::returnReference(self->state_manager_->set(cpp_state));
}

PyObject* PyStateManager::get(PyStateManager* self, PyObject*) {
  if (!self->state_manager_) {
    PyErr_SetString(PyExc_AttributeError, "tried reading state manager outside 'on_trigger'");
    return nullptr;
  }
  if (auto cpp_state = self->state_manager_->get()) {
    auto python_state = OwnedDict::create();
    for (const auto& [cpp_state_key, cpp_state_value] : *cpp_state) {
      python_state.put(cpp_state_key, cpp_state_value);
    }
    return object::returnReference(python_state);
  } else {
    Py_RETURN_NONE;
  }
}

PyObject* PyStateManager::clear(PyStateManager* self, PyObject* /*args*/) {
  if (!self->state_manager_) {
    PyErr_SetString(PyExc_AttributeError, "tried reading state manager outside 'on_trigger'");
    return nullptr;
  }

  self->state_manager_->clear();
  Py_RETURN_NONE;
}

PyObject* PyStateManager::replace(PyStateManager* self, PyObject* args) {
  if (!self->state_manager_) {
    PyErr_SetString(PyExc_AttributeError, "tried reading state manager outside 'on_trigger'");
    return nullptr;
  }
  core::StateManager::State old_cpp_state;

  auto old_state = BorrowedDict::fromTuple(args, 0);

  auto old_state_keys = OwnedList(PyDict_Keys(old_state.get()));
  for (size_t i = 0; i < old_state_keys.length(); ++i) {
    BorrowedStr key{old_state_keys[i]};
    if (auto value = old_state[key.toUtf8String()]) {
      BorrowedStr value_str{*value};
      old_cpp_state[key.toUtf8String()] = value_str.toUtf8String();
    }
  }

  auto current_cpp_state = self->state_manager_->get();
  if ((!current_cpp_state && old_state_keys.length() > 0) || (current_cpp_state && old_cpp_state != *current_cpp_state)) {
    return object::returnReference(false);
  }

  core::StateManager::State new_cpp_state;

  auto new_python_state = BorrowedDict::fromTuple(args, 1);

  auto new_python_state_keys = OwnedList(PyDict_Keys(new_python_state.get()));
  for (size_t i = 0; i < new_python_state_keys.length(); ++i) {
    BorrowedStr key{new_python_state_keys[i]};
    if (auto value = new_python_state[key.toUtf8String()]) {
      BorrowedStr value_str{*value};
      new_cpp_state[key.toUtf8String()] = value_str.toUtf8String();
    }
  }

  return object::returnReference(self->state_manager_->set(new_cpp_state));
}

PyTypeObject* PyStateManager::typeObject() {
  static OwnedObject PyStateManagerType{PyType_FromSpec(&PyStateManagerTypeSpec)};
  return reinterpret_cast<PyTypeObject*>(PyStateManagerType.get());
}

}  // namespace org::apache::nifi::minifi::extensions::python
}  // extern "C"
