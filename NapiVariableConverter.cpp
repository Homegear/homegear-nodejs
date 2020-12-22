/* Copyright 2013-2019 Homegear GmbH
 *
 * Homegear is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * Homegear is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with Homegear.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * In addition, as a special exception, the copyright holders give
 * permission to link the code of portions of this program with the
 * OpenSSL library under certain conditions as described in each
 * individual source file, and distribute linked combinations
 * including the two.
 * You must obey the GNU Lesser General Public License in all respects
 * for all of the code used other than OpenSSL.  If you modify
 * file(s) with this exception, you may extend this exception to your
 * version of the file(s), but you are not obligated to do so.  If you
 * do not wish to do so, delete this exception statement from your
 * version.  If you delete this exception statement from all source
 * files in the program, then also delete it here.
*/

#include "NapiVariableConverter.h"
#include <cassert>

Ipc::PVariable NapiVariableConverter::getVariable(napi_env env, napi_value value) {
  Ipc::PVariable variable;
  if (!value) return std::make_shared<Ipc::Variable>();
  napi_valuetype valuetype;
  auto status = napi_typeof(env, value, &valuetype);
  assert(status == napi_ok);
  if (valuetype == napi_null || valuetype == napi_undefined) {
    return std::make_shared<Ipc::Variable>();
  } else if (valuetype == napi_boolean) {
    bool boolean_value;
    status = napi_get_value_bool(env, value, &boolean_value);
    assert(status == napi_ok);
    return std::make_shared<Ipc::Variable>(boolean_value);
  } else if (valuetype == napi_number) {
    int64_t integer_value;
    double double_value;
    status = napi_get_value_int64(env, value, &integer_value);
    assert(status == napi_ok);
    status = napi_get_value_double(env, value, &double_value);
    assert(status == napi_ok);
    if (double_value != (double)integer_value) return std::make_shared<Ipc::Variable>(double_value);
    else return std::make_shared<Ipc::Variable>(integer_value);
  } else if (valuetype == napi_string) {
    size_t string_length = 0;
    status = napi_get_value_string_utf8(env, value, nullptr, 0, &string_length);
    assert(status == napi_ok);
    std::string string_value;
    string_value.resize(string_length + 1);
    status = napi_get_value_string_utf8(env, value, (char *)string_value.data(), string_value.size(), &string_length);
    assert(status == napi_ok);
    string_value.resize(string_length);
    return std::make_shared<Ipc::Variable>(string_value);
  } else if (valuetype == napi_symbol) {
    //All non-String values that may be used as the key of on Object property.
  } else if (valuetype == napi_object) {
    bool result;
    status = napi_is_array(env, value, &result);
    assert(status == napi_ok);
    if (result) { //is array
      auto ipc_array = std::make_shared<Ipc::Variable>(Ipc::VariableType::tArray);
      uint32_t array_length;
      status = napi_get_array_length(env, value, &array_length);
      assert(status == napi_ok);
      ipc_array->arrayValue->reserve(array_length);
      for (uint32_t i = 0; i < array_length; i++) {
        napi_value element_value;
        status = napi_get_element(env, value, i, &element_value);
        assert(status == napi_ok);
        ipc_array->arrayValue->emplace_back(getVariable(env, element_value));
      }
      return ipc_array;
    } else {
      auto ipc_struct = std::make_shared<Ipc::Variable>(Ipc::VariableType::tStruct);
      napi_value properties;
      status = napi_get_property_names(env, value, &properties);
      assert(status == napi_ok);
      uint32_t array_length;
      status = napi_get_array_length(env, properties, &array_length);
      assert(status == napi_ok);
      for (uint32_t i = 0; i < array_length; i++) {
        napi_value property_name;
        status = napi_get_element(env, properties, i, &property_name);
        assert(status == napi_ok);
        napi_value element_value;
        status = napi_get_property(env, value, property_name, &element_value);
        assert(status == napi_ok);
        ipc_struct->structValue->emplace(getVariable(env, property_name)->stringValue, getVariable(env, element_value));
      }
      return ipc_struct;
    }
  } else if (valuetype == napi_function) {
    //Can't be converted.
  } else if (valuetype == napi_external) {
    //Wrapped arbitrary external data - can't be converted.
  } else if (valuetype == napi_bigint) {
    int64_t integer_value;
    bool lossless;
    status = napi_get_value_bigint_int64(env, value, &integer_value, &lossless);
    assert(status == napi_ok);
    return std::make_shared<Ipc::Variable>(integer_value);
  }
  return std::make_shared<Ipc::Variable>();
}

napi_value NapiVariableConverter::getNapiVariable(napi_env env, const Ipc::PVariable &value) {
  if (!value) return nullptr;
  napi_value result = nullptr;
  if (value->type == Ipc::VariableType::tVoid) {
    auto status = napi_get_null(env, &result);
    assert(status == napi_ok);
  } else if (value->type == Ipc::VariableType::tBoolean) {
    auto status = napi_get_boolean(env, value->booleanValue, &result);
    assert(status == napi_ok);
  } else if (value->type == Ipc::VariableType::tInteger || value->type == Ipc::VariableType::tInteger64) {
    auto status = napi_create_int64(env, value->integerValue64, &result);
    assert(status == napi_ok);
  } else if (value->type == Ipc::VariableType::tFloat) {
    auto status = napi_create_int64(env, value->floatValue, &result);
    assert(status == napi_ok);
  } else if (value->type == Ipc::VariableType::tString || value->type == Ipc::VariableType::tBase64) {
    auto status = napi_create_string_utf8(env, value->stringValue.c_str(), NAPI_AUTO_LENGTH, &result);
    assert(status == napi_ok);
  } else if (value->type == Ipc::VariableType::tArray) {
    auto status = napi_create_array_with_length(env, value->arrayValue->size(), &result);
    assert(status == napi_ok);
    for (uint32_t i = 0; i < value->arrayValue->size(); i++) {
      status = napi_set_element(env, result, i, getNapiVariable(env, value->arrayValue->at(i)));
      assert(status == napi_ok);
    }
  } else if (value->type == Ipc::VariableType::tStruct) {
    auto status = napi_create_object(env, &result);
    assert(status == napi_ok);
    for (auto &element : *value->structValue) {
      status = napi_set_named_property(env, result, element.first.c_str(), getNapiVariable(env, element.second));
      assert(status == napi_ok);
    }
  }
  return result;
}
