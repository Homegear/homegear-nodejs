//
// Created by SLaufer on 21.12.20.
//

#include "HomegearObject.h"
#include "NapiVariableConverter.h"
#include <cassert>

Homegear::Homegear(const std::string &socket_path) : env_(nullptr), wrapper_(nullptr) {
  ipc_client_ = std::make_unique<IpcClient>(socket_path);
  ipc_client_->SetOnConnect(std::bind(&Homegear::OnConnect, this));
  ipc_client_->SetOnDisconnect(std::bind(&Homegear::OnDisconnect, this));
  ipc_client_->SetBroadcastEvent(std::bind(&Homegear::OnEvent, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5));
  ipc_client_->SetNodeInput(std::bind(&Homegear::OnNodeInput, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5));
  ipc_client_->SetInvokeNodeMethod(std::bind(&Homegear::OnInvokeNodeMethod, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
  ipc_client_->start();
}

Homegear::~Homegear() {
  ipc_client_.reset();
  //Don't call napi_release_threadsafe_function() here. This would doubly release them (found out with valgrind)
  napi_delete_reference(env_, wrapper_);
}

void Homegear::Destructor(napi_env env, void *nativeObject, void * /*finalize_hint*/) {
  if (!nativeObject) return;
  delete reinterpret_cast<Homegear *>(nativeObject);
}

#define DECLARE_NAPI_METHOD(name, func)                                        \
  { name, nullptr, func, nullptr, nullptr, nullptr, napi_default, nullptr }

napi_value Homegear::Init(napi_env env, napi_value exports) {
  napi_status status;
  napi_property_descriptor properties[] = {
      DECLARE_NAPI_METHOD("connected", Connected),
      DECLARE_NAPI_METHOD("invoke", Invoke)
  };

  napi_value cons;
  status = napi_define_class(env, "Homegear", NAPI_AUTO_LENGTH, New, nullptr, 2, properties, &cons);
  assert(status == napi_ok);

  // We will need the constructor `cons` later during the life cycle of the
  // addon, so we store a persistent reference to it as the instance data for
  // our addon. This will enable us to use `napi_get_instance_data` at any
  // point during the life cycle of our addon to retrieve it. We cannot simply
  // store it as a global static variable, because that will render our addon
  // unable to support Node.js worker threads and multiple contexts on a single
  // thread.
  //
  // The finalizer we pass as a lambda will be called when our addon is unloaded
  // and is responsible for releasing the persistent reference and freeing the
  // heap memory where we stored the persistent reference.
  auto *constructor = new napi_ref;
  status = napi_create_reference(env, cons, 1, constructor);
  assert(status == napi_ok);
  status = napi_set_instance_data(
      env,
      constructor,
      [](napi_env env, void *data, void *hint) {
        auto *constructor = static_cast<napi_ref *>(data);
        napi_status status = napi_delete_reference(env, *constructor);
        assert(status == napi_ok);
        delete constructor;
      },
      nullptr);
  assert(status == napi_ok);

  status = napi_set_named_property(env, exports, "Homegear", cons);
  assert(status == napi_ok);
  return exports;
}

napi_value Homegear::Constructor(napi_env env) {
  void *instance_data = nullptr;
  napi_status status = napi_get_instance_data(env, &instance_data);
  assert(status == napi_ok);
  auto *constructor = static_cast<napi_ref *>(instance_data);

  napi_value cons;
  status = napi_get_reference_value(env, *constructor, &cons);
  assert(status == napi_ok);
  return cons;
}

napi_value Homegear::New(napi_env env, napi_callback_info info) {
  napi_status status;

  napi_value target;
  status = napi_get_new_target(env, info, &target);
  assert(status == napi_ok);
  bool is_constructor = target != nullptr;

  if (is_constructor) {
    // Invoked as constructor: `new Homegear(...)`
    size_t argc = 6;
    napi_value args[6];
    napi_value jsthis;
    status = napi_get_cb_info(env, info, &argc, args, &jsthis, nullptr);
    assert(status == napi_ok);

    napi_valuetype valuetype;
    status = napi_typeof(env, args[0], &valuetype);
    assert(status == napi_ok);
    std::string socket_path;
    if (valuetype == napi_string) {
      size_t string_length = 0;
      status = napi_get_value_string_utf8(env, args[0], nullptr, 0, &string_length);
      assert(status == napi_ok);
      socket_path.resize(string_length + 1);
      status = napi_get_value_string_utf8(env, args[0], (char *)socket_path.data(), socket_path.size(), &string_length);
      assert(status == napi_ok);
      socket_path.resize(string_length);
    }
    if (socket_path.empty()) socket_path = "/var/run/homegear/homegearIPC.sock";

    Homegear *obj = nullptr;
    { //Create new object
      obj = new Homegear(socket_path);
      obj->env_ = env;
      status = napi_wrap(env, jsthis, reinterpret_cast<void *>(obj), Homegear::Destructor, nullptr /* finalize_hint */, &obj->wrapper_);
      assert(status == napi_ok);
    }

    { //OnConnect
      napi_value on_connect_callback_js = args[1];
      status = napi_typeof(env, on_connect_callback_js, &valuetype);
      assert(status == napi_ok);
      if (valuetype == napi_function) {
        napi_value resource_name;
        status = napi_create_string_utf8(env, "Thread-safe call from OnConnect()", NAPI_AUTO_LENGTH, &resource_name);
        assert(status == napi_ok);
        status = napi_create_threadsafe_function(env, on_connect_callback_js, nullptr, resource_name, 0, 1, nullptr, nullptr, nullptr, OnConnectJs, &obj->on_connect_threadsafe_function_);
        assert(status == napi_ok);
        status = napi_unref_threadsafe_function(env, obj->on_connect_threadsafe_function_); //Allow destruction of process even though the reference counter is not 0
        assert(status == napi_ok);
      }
    }

    { //OnDisconnect
      napi_value on_disconnect_callback_js = args[2];
      status = napi_typeof(env, on_disconnect_callback_js, &valuetype);
      assert(status == napi_ok);
      if (valuetype == napi_function) {
        napi_value resource_name;
        status = napi_create_string_utf8(env, "Thread-safe call from OnDisconnect()", NAPI_AUTO_LENGTH, &resource_name);
        assert(status == napi_ok);
        status = napi_create_threadsafe_function(env, on_disconnect_callback_js, nullptr, resource_name, 0, 1, nullptr, nullptr, nullptr, OnDisconnectJs, &obj->on_disconnect_threadsafe_function_);
        assert(status == napi_ok);
        status = napi_unref_threadsafe_function(env, obj->on_disconnect_threadsafe_function_); //Allow destruction of process even though the reference counter is not 0
        assert(status == napi_ok);
      }
    }

    { //OnEvent
      napi_value on_event_callback_js = args[3];
      status = napi_typeof(env, on_event_callback_js, &valuetype);
      assert(status == napi_ok);
      if (valuetype == napi_function) {
        napi_value resource_name;
        status = napi_create_string_utf8(env, "Thread-safe call from OnEvent()", NAPI_AUTO_LENGTH, &resource_name);
        assert(status == napi_ok);
        status = napi_create_threadsafe_function(env, on_event_callback_js, nullptr, resource_name, 0, 1, nullptr, nullptr, nullptr, OnEventJs, &obj->on_event_threadsafe_function_);
        assert(status == napi_ok);
        status = napi_unref_threadsafe_function(env, obj->on_event_threadsafe_function_); //Allow destruction of process even though the reference counter is not 0
        assert(status == napi_ok);
      }
    }

    { //OnNodeInput
      napi_value on_node_input_callback_js = args[4];
      status = napi_typeof(env, on_node_input_callback_js, &valuetype);
      assert(status == napi_ok);
      if (valuetype == napi_function) {
        napi_value resource_name;
        status = napi_create_string_utf8(env, "Thread-safe call from OnNodeInput()", NAPI_AUTO_LENGTH, &resource_name);
        assert(status == napi_ok);
        status = napi_create_threadsafe_function(env, on_node_input_callback_js, nullptr, resource_name, 0, 1, nullptr, nullptr, nullptr, OnNodeInputJs, &obj->on_node_input_threadsafe_function_);
        assert(status == napi_ok);
        status = napi_unref_threadsafe_function(env, obj->on_node_input_threadsafe_function_); //Allow destruction of process even though the reference counter is not 0
        assert(status == napi_ok);
      }
    }

    { //OnInvokeNodeMethod
      napi_value on_invoke_node_method_callback_js = args[5];
      status = napi_typeof(env, on_invoke_node_method_callback_js, &valuetype);
      assert(status == napi_ok);
      if (valuetype == napi_function) {
        napi_value resource_name;
        status = napi_create_string_utf8(env, "Thread-safe call from OnInvokeNodeMethod()", NAPI_AUTO_LENGTH, &resource_name);
        assert(status == napi_ok);
        status = napi_create_threadsafe_function(env, on_invoke_node_method_callback_js, nullptr, resource_name, 0, 1, nullptr, nullptr, obj, OnInvokeNodeMethodJs, &obj->on_invoke_node_method_threadsafe_function_);
        assert(status == napi_ok);
        status = napi_unref_threadsafe_function(env, obj->on_invoke_node_method_threadsafe_function_); //Allow destruction of process even though the reference counter is not 0
        assert(status == napi_ok);
      }
    }

    return jsthis;
  } else {
    // Invoked as plain function `Homegear(...)`, turn into construct call.
    size_t argc_ = 6;
    napi_value args[argc_];
    status = napi_get_cb_info(env, info, &argc_, args, nullptr, nullptr);
    assert(status == napi_ok);

    const size_t argc = 6;
    napi_value argv[argc] = {args[0], args[1], args[2], args[3], args[4], args[5]};

    napi_value instance;
    status = napi_new_instance(env, Constructor(env), argc, argv, &instance);
    assert(status == napi_ok);

    return instance;
  }
}

void Homegear::OnConnectJs(napi_env env, napi_value callback, void *context, void *data) {
  // env and callback may both be NULL if Node.js is in its cleanup phase, and
  // items are left over from earlier thread-safe calls from the worker thread.
  // When env is NULL, we simply skip over the call into Javascript and free the
  // items.
  if (env && callback) {
    // Retrieve the JavaScript `undefined` value so we can use it as the `this`
    // value of the JavaScript function call.
    napi_value undefined;
    auto status = napi_get_undefined(env, &undefined);
    assert(status == napi_ok);

    status = napi_call_function(env, undefined, callback, 0, nullptr, nullptr);
    assert(status == napi_ok);
  }
}

void Homegear::OnConnect() {
  if (!on_connect_threadsafe_function_) return;
  auto status = napi_acquire_threadsafe_function(on_connect_threadsafe_function_);
  assert(status == napi_ok);
  status = napi_call_threadsafe_function(on_connect_threadsafe_function_, nullptr, napi_tsfn_nonblocking);
  assert(status == napi_ok);
  status = napi_release_threadsafe_function(on_connect_threadsafe_function_, napi_tsfn_release);
  assert(status == napi_ok);
}

void Homegear::OnDisconnectJs(napi_env env, napi_value callback, void *context, void *data) {
  // env and callback may both be NULL if Node.js is in its cleanup phase, and
  // items are left over from earlier thread-safe calls from the worker thread.
  // When env is NULL, we simply skip over the call into Javascript and free the
  // items.
  if (env && callback) {
    // Retrieve the JavaScript `undefined` value so we can use it as the `this`
    // value of the JavaScript function call.
    napi_value undefined;
    auto status = napi_get_undefined(env, &undefined);
    assert(status == napi_ok);

    status = napi_call_function(env, undefined, callback, 0, nullptr, nullptr);
    assert(status == napi_ok);
  }
}

void Homegear::OnDisconnect() {
  if (!on_disconnect_threadsafe_function_) return;
  auto status = napi_acquire_threadsafe_function(on_disconnect_threadsafe_function_);
  assert(status == napi_ok);
  status = napi_call_threadsafe_function(on_disconnect_threadsafe_function_, nullptr, napi_tsfn_nonblocking);
  assert(status == napi_ok);
  status = napi_release_threadsafe_function(on_disconnect_threadsafe_function_, napi_tsfn_release);
  assert(status == napi_ok);
}

void Homegear::OnEventJs(napi_env env, napi_value callback, void *context, void *data) {
  // env and callback may both be NULL if Node.js is in its cleanup phase, and
  // items are left over from earlier thread-safe calls from the worker thread.
  // When env is NULL, we simply skip over the call into Javascript and free the
  // items.
  if (env && callback) {
    // Retrieve the JavaScript `undefined` value so we can use it as the `this`
    // value of the JavaScript function call.
    napi_value undefined;
    auto status = napi_get_undefined(env, &undefined);
    assert(status == napi_ok);

    size_t argc = 5;
    napi_value args[argc];

    auto *event_struct = (OnEventStruct *)data;
    status = napi_create_string_utf8(env, event_struct->event_source.c_str(), NAPI_AUTO_LENGTH, &args[0]);
    assert(status == napi_ok);
    status = napi_create_int64(env, event_struct->peer_id, &args[1]);
    assert(status == napi_ok);
    status = napi_create_int32(env, event_struct->channel, &args[2]);
    assert(status == napi_ok);
    status = napi_create_string_utf8(env, event_struct->variable_name.c_str(), NAPI_AUTO_LENGTH, &args[3]);
    assert(status == napi_ok);
    args[4] = NapiVariableConverter::getNapiVariable(env, event_struct->value);
    assert(status == napi_ok);

    status = napi_call_function(env, undefined, callback, argc, args, nullptr);
    assert(status == napi_ok);
  }

  delete (OnEventStruct *)data;
}

void Homegear::OnEvent(std::string &event_source, uint64_t peer_id, int32_t channel, const std::string &variable_name, const Ipc::PVariable &value) {
  if (!on_event_threadsafe_function_) return;
  auto status = napi_acquire_threadsafe_function(on_event_threadsafe_function_);
  assert(status == napi_ok);
  auto *data = new OnEventStruct;
  data->event_source = event_source;
  data->peer_id = peer_id;
  data->channel = channel;
  data->variable_name = variable_name;
  data->value = value;
  status = napi_call_threadsafe_function(on_event_threadsafe_function_, data, napi_tsfn_nonblocking);
  assert(status == napi_ok);
  status = napi_release_threadsafe_function(on_event_threadsafe_function_, napi_tsfn_release);
  assert(status == napi_ok);
}

void Homegear::OnNodeInputJs(napi_env env, napi_value callback, void *context, void *data) {
  // env and callback may both be NULL if Node.js is in its cleanup phase, and
  // items are left over from earlier thread-safe calls from the worker thread.
  // When env is NULL, we simply skip over the call into Javascript and free the
  // items.
  if (env && callback) {
    // Retrieve the JavaScript `undefined` value so we can use it as the `this`
    // value of the JavaScript function call.
    napi_value undefined;
    auto status = napi_get_undefined(env, &undefined);
    assert(status == napi_ok);

    size_t argc = 5;
    napi_value args[argc];

    auto *node_input_struct = (OnNodeInputStruct *)data;
    status = napi_create_string_utf8(env, node_input_struct->node_id.c_str(), NAPI_AUTO_LENGTH, &args[0]);
    assert(status == napi_ok);
    args[1] = NapiVariableConverter::getNapiVariable(env, node_input_struct->node_info);
    assert(status == napi_ok);
    status = napi_create_uint32(env, node_input_struct->input_index, &args[2]);
    assert(status == napi_ok);
    args[3] = NapiVariableConverter::getNapiVariable(env, node_input_struct->message);
    assert(status == napi_ok);
    status = napi_get_boolean(env, node_input_struct->synchronous, &args[4]);
    assert(status == napi_ok);

    status = napi_call_function(env, undefined, callback, argc, args, nullptr);
    assert(status == napi_ok);
  }

  delete (OnNodeInputStruct *)data;
}

void Homegear::OnNodeInput(const std::string &node_id, const Ipc::PVariable &node_info, uint32_t input_index, const Ipc::PVariable &message, bool synchronous) {
  if (!on_node_input_threadsafe_function_) return;
  auto status = napi_acquire_threadsafe_function(on_node_input_threadsafe_function_);
  assert(status == napi_ok);
  auto *data = new OnNodeInputStruct;
  data->node_id = node_id;
  data->node_info = node_info;
  data->input_index = input_index;
  data->message = message;
  data->synchronous = synchronous;
  status = napi_call_threadsafe_function(on_node_input_threadsafe_function_, data, napi_tsfn_nonblocking);
  assert(status == napi_ok);
  status = napi_release_threadsafe_function(on_node_input_threadsafe_function_, napi_tsfn_release);
  assert(status == napi_ok);
}

void Homegear::OnInvokeNodeMethodJs(napi_env env, napi_value callback, void *context, void *data) {
  // env and callback may both be NULL if Node.js is in its cleanup phase, and
  // items are left over from earlier thread-safe calls from the worker thread.
  // When env is NULL, we simply skip over the call into Javascript and free the
  // items.
  if (env && callback && context) {
    // Retrieve the JavaScript `undefined` value so we can use it as the `this`
    // value of the JavaScript function call.
    napi_value undefined;
    auto status = napi_get_undefined(env, &undefined);
    assert(status == napi_ok);

    size_t argc = 3;
    napi_value args[3];

    auto *invoke_node_method_struct = (OnInvokeNodeMethodStruct *)data;
    status = napi_create_string_utf8(env, invoke_node_method_struct->node_id.c_str(), NAPI_AUTO_LENGTH, &args[0]);
    assert(status == napi_ok);
    status = napi_create_string_utf8(env, invoke_node_method_struct->method_name.c_str(), NAPI_AUTO_LENGTH, &args[1]);
    assert(status == napi_ok);
    args[2] = NapiVariableConverter::getNapiVariable(env, invoke_node_method_struct->parameters);
    assert(status == napi_ok);

    napi_value return_val;
    status = napi_call_function(env, undefined, callback, argc, args, &return_val);
    assert(status == napi_ok);

    auto result = NapiVariableConverter::getVariable(env, return_val);

    auto obj = static_cast<Homegear *>(context);
    obj->ipc_client_->InvokeResult(invoke_node_method_struct->thread_id, result);
  }

  delete (OnInvokeNodeMethodStruct *)data;
}

bool Homegear::OnInvokeNodeMethod(pthread_t thread_id, const std::string &node_id, const std::string &method_name, const Ipc::PVariable &parameters) {
  if (!on_invoke_node_method_threadsafe_function_) return false;
  auto status = napi_acquire_threadsafe_function(on_invoke_node_method_threadsafe_function_);
  assert(status == napi_ok);
  auto *data = new OnInvokeNodeMethodStruct;
  data->thread_id = thread_id;
  data->node_id = node_id;
  data->method_name = method_name;
  data->parameters = parameters;
  status = napi_call_threadsafe_function(on_invoke_node_method_threadsafe_function_, data, napi_tsfn_nonblocking);
  assert(status == napi_ok);
  status = napi_release_threadsafe_function(on_invoke_node_method_threadsafe_function_, napi_tsfn_release);
  assert(status == napi_ok);

  return true;
}

napi_value Homegear::Invoke(napi_env env, napi_callback_info info) {
  size_t argc = 2;
  napi_value args[argc];
  napi_value jsthis;
  auto status = napi_get_cb_info(env, info, &argc, args, &jsthis, nullptr);
  assert(status == napi_ok);

  auto method = NapiVariableConverter::getVariable(env, args[0]);
  auto parameters = NapiVariableConverter::getVariable(env, args[1]);

  if (method->stringValue.empty()) {
    status = napi_throw_type_error(env, "-1", "method is not a String or empty.");
    assert(status == napi_ok);
    return nullptr;
  }

  Homegear *obj;
  status = napi_unwrap(env, jsthis, reinterpret_cast<void **>(&obj));
  assert(status == napi_ok);

  auto rpc_result = obj->ipc_client_->invoke(method->stringValue, parameters->arrayValue);
  if (rpc_result->errorStruct) {
    status = napi_throw_error(env, std::to_string(rpc_result->structValue->at("faultCode")->integerValue).c_str(), rpc_result->structValue->at("faultString")->stringValue.c_str());
    assert(status == napi_ok);
    return nullptr;
  }

  return NapiVariableConverter::getNapiVariable(env, rpc_result);
}

napi_value Homegear::Connected(napi_env env, napi_callback_info info) {
  size_t argc = 0;
  napi_value jsthis;
  auto status = napi_get_cb_info(env, info, &argc, nullptr, &jsthis, nullptr);
  assert(status == napi_ok);

  Homegear *obj;
  status = napi_unwrap(env, jsthis, reinterpret_cast<void **>(&obj));
  assert(status == napi_ok);

  napi_value result;
  status = napi_get_boolean(env, obj->ipc_client_->connected(), &result);
  assert(status == napi_ok);

  return result;
}


