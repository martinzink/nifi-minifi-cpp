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

#pragma once

#include <string>
#include <optional>

#include "Exception.h"

namespace org::apache::nifi::minifi::python {

/**
 * @brief Python object type
 * 
 * @tparam reference_type 
 */
template <ReferenceType reference_type>
class Object : public ReferenceHolder<reference_type> {
 public:
  using Reference = ObjectReference<reference_type>;

  Object() = default;

  explicit Object(Reference ref)
      : ReferenceHolder<reference_type>(std::move(ref)) {

  }

  explicit Object(PyObject *object)
      : ReferenceHolder<reference_type>(object) {

  }

  OwnedReference getAttribute(std::string_view attribute_name) {
    return OwnedReference(PyObject_GetAttrString(this->ref_.get(), attribute_name.data()));
  }
};

/**
 * @brief Helper methods to create Object instances
 * 
 */
namespace object {

template <typename T>
struct Converter {};

template <typename T>
concept convertible = requires(T type) {
  { Converter<T>().from(std::forward<T>(type)) } -> std::same_as<OwnedObject>;
};

template <>
struct Converter<bool> {
  OwnedObject from(bool value) {
    if (value) {
      Py_INCREF(Py_True);
      return OwnedObject(Py_True);
    }
    Py_INCREF(Py_False);
    return OwnedObject(Py_False);
  }
};

template <>
struct Converter<nullptr_t> {
  OwnedObject from(nullptr_t) {
    Py_INCREF(Py_None);
    return OwnedObject(Py_None);
  }
};

template <>
struct Converter<std::string_view> {
  OwnedObject from(std::string_view value) {
    return OwnedObject(PyUnicode_FromStringAndSize(value.data(), value.length()));
  }
};

template <>
struct Converter<std::string> : public Converter<std::string_view> {};

template <typename T>
requires std::signed_integral<T> && (sizeof(T) <= sizeof(long))
struct Converter<T> {
  OwnedObject from(T value) {
    return OwnedObject(PyLong_FromLong(static_cast<long>(value)));
  }
};

template <typename T>
requires std::unsigned_integral<T> && (sizeof(T) <= sizeof(unsigned long))
struct Converter<T> {
  OwnedObject from(T value) {
    return OwnedObject(PyLong_FromUnsignedLong(static_cast<unsigned long>(value)));
  }
};

template <typename T>
requires std::signed_integral<T> && (sizeof(T) > sizeof(long))
struct Converter<T> {
  OwnedObject from(T value) {
    return OwnedObject(PyLong_FromLongLong(static_cast<long long>(value)));
  }
};

template <typename T>
requires std::unsigned_integral<T> && (sizeof(T) > sizeof(unsigned long))
struct Converter<T> {
  OwnedObject from(T value) {
    return OwnedObject(PyLong_FromUnsignedLongLong(static_cast<unsigned long>(value)));
  }
};

template <std::derived_from<OwnedReferenceHolder> T>
struct Converter<T> {
  OwnedObject from(T &&ownedObject) {
    return OwnedObject(ownedObject.releaseReference());
  }
};

template <holder_type T>
struct HolderTypeConverter {
  OwnedObject from(typename T::HeldType &&value) {
    auto typeObject = T::typeObject();
    return OwnedObject(PyObject_CallFunction(reinterpret_cast<PyObject*>(typeObject), "O&", &HolderTypeConverter::convertToCapsule, reinterpret_cast<void*>(&value)));
  }

  static inline PyObject* convertToCapsule(void *ptr) {
    return PyCapsule_New(ptr, nullptr, nullptr);
  }
};

template <convertible T>
OwnedObject from(T value) {
  return Converter<T>().from(std::forward<T>(value));
}

template <convertible T>
PyObject* returnReference(T value) {
  return from(std::forward<T>(value)).releaseReference();
}
} // namespace object

/**
 * @brief Wrapper for Python "long" objects
 * 
 * @tparam reference_type 
 */
template <ReferenceType reference_type>
class Long : public ReferenceHolder<reference_type> {
 public:
  using Reference = ObjectReference<reference_type>;

  Long() = default;

  explicit Long(Reference ref)
      : ReferenceHolder<reference_type>(std::move(ref)) {

  }

  explicit Long(PyObject *object)
      : ReferenceHolder<reference_type>(object) {

  }

  int64_t asInt64() {
    return static_cast<int64_t>(PyLong_AsLongLong(this->ref_.get()));
  }
};

/**
 * @brief Wrapper for Python "str" objects
 * 
 * @tparam reference_type 
 */
template <ReferenceType reference_type>
class Str : public ReferenceHolder<reference_type> {
 public:
  using Reference = ObjectReference<reference_type>;

  Str() = default;

  explicit Str(Reference ref)
      : ReferenceHolder<reference_type>(std::move(ref)) {

  }

  explicit Str(PyObject *object)
      : ReferenceHolder<reference_type>(object) {

  }

  std::string_view asUtf8() {
    return PyUnicode_AsUTF8AndSize(this->ref_.get(), nullptr);
  }

  std::string toUtf8String() {
    return std::string(asUtf8());
  }

  static OwnedStr from(convertible_to_object auto object) requires (reference_type == ReferenceType::OWNED) {
    return OwnedStr(PyObject_Str(static_cast<PyObject*>(object)));
  }
};

/**
 * @brief Wrapper for Python "list" objects
 * 
 * @tparam reference_type 
 */
template <ReferenceType reference_type>
class List : public ReferenceHolder<reference_type> {
 public:
  using Reference = ObjectReference<reference_type>;

  List() = default;

  explicit List(Reference ref)
      : ReferenceHolder<reference_type>(std::move(ref)) {

  }

  explicit List(PyObject *object)
      : ReferenceHolder<reference_type>(object) {

  }

  size_t length() {
    return PyList_Size(this->ref_.get());
  }

  BorrowedReference operator[](size_t index) {
    auto item = PyList_GetItem(this->ref_.get(), index);
    if (!item) {
      throw PyException();
    }
    return BorrowedReference(item);
  }
};

/**
 * @brief Wrapper for Python "dict" objects
 * 
 * @tparam reference_type 
 */
template <ReferenceType reference_type>
class Dict : public ReferenceHolder<reference_type> {
 public:
  using Reference = ObjectReference<reference_type>;

  Dict() = default;

  explicit Dict(Reference ref)
      : ReferenceHolder<reference_type>(std::move(ref)) {

  }

  explicit Dict(PyObject *object)
      : ReferenceHolder<reference_type>(object) {

  }

  static OwnedDict create() requires (reference_type == ReferenceType::OWNED) {
    return OwnedDict(PyDict_New());
  }

  template <object::convertible T>
  bool contains(T key) const {
    auto key_object = object::from(key);
    return PyDict_Contains(this->ref_.get(), key_object.get()) != 0;
  }

  template <object::convertible T>
  void put(std::string_view key, T value) {
    PyDict_SetItemString(this->ref_.get(), key.data(), object::from(value).releaseReference());
  }

  std::optional<BorrowedReference> operator[](std::string_view key) {
    auto item = BorrowedReference(PyDict_GetItemString(this->ref_.get(), key.data()));
    return item ? std::optional{ item } : std::nullopt;
  }
};

/**
 * @brief Wrapper for Python "bytes" objects
 * 
 * @tparam reference_type 
 */
template <ReferenceType reference_type>
class Bytes : public ReferenceHolder<reference_type> {
 public:
  using Reference = ObjectReference<reference_type>;

  Bytes() = default;

  explicit Bytes(Reference ref)
      : ReferenceHolder<reference_type>(std::move(ref)) {

  }

  explicit Bytes(PyObject *object)
      : ReferenceHolder<reference_type>(object) {

  }

  static OwnedBytes fromStringAndSize(std::string_view string) requires (reference_type == ReferenceType::OWNED) {
    return OwnedBytes(PyBytes_FromStringAndSize(string.data(), string.length()));
  }
};

/**
 * @brief Helper methods for callable objects
 * 
 */
namespace callable
{
template <ReferenceType reference_type>
ObjectReference<reference_type> argument(ObjectReference<reference_type> reference) {
  return reference;
}

static inline BorrowedReference argument(nullptr_t) {
  return BorrowedReference(Py_None);
}

static inline BorrowedReference argument(bool value) {
  return BorrowedReference(value ? Py_True : Py_False);
}

template <typename T>
OwnedReference argument(T value) {
  return OwnedReference(object::from(std::forward<T>(value)).releaseReference());
}

} // namespace callable

/**
 * @brief Wrapper for Python objects implementing the callable protocol
 * 
 * @tparam reference_type 
 */
template <ReferenceType reference_type>
class Callable : public ReferenceHolder<reference_type>{
 public:
  using Reference = ObjectReference<reference_type>;

  Callable() = default;
  
  explicit Callable(Reference ref)
      : ReferenceHolder<reference_type>(std::move(ref)) {

  }

  explicit Callable(PyObject *object)
      : ReferenceHolder<reference_type>(object) {

  }

  template <typename... Args>
  OwnedReference operator()(Args &&...args) {
    return OwnedReference(PyObject_CallFunctionObjArgs(this->ref_.get(), callable::argument(std::forward<Args>(args)).get()..., nullptr));
  }
};

/**
 * @brief Wrapper for Python modules
 * 
 * @tparam reference_type 
 */
template <ReferenceType reference_type>
class Module : public ReferenceHolder<reference_type> {
 public:
  using Reference = ObjectReference<reference_type>;

  Module() = default;
  
  explicit Module(Reference ref)
      : ReferenceHolder<reference_type>(std::move(ref)) {

  }

  explicit Module(PyObject *object)
      : ReferenceHolder<reference_type>(object) {

  }

  static OwnedModule import(std::string_view module_name) requires (reference_type == ReferenceType::OWNED) {
    return OwnedModule(PyImport_ImportModule(module_name.data()));
  }

  OwnedCallable getFunction(std::string_view function_name) {
    return OwnedCallable(PyObject_GetAttrString(this->ref_.get(), function_name.data()));
  }
};

}  // namespace org::apache::nifi::minifi::python
