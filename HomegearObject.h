//
// Created by SLaufer on 21.12.20.
//

#ifndef HOMEGEAR_NODEJS__HOMEGEAROBJECT_H_
#define HOMEGEAR_NODEJS__HOMEGEAROBJECT_H_

#include <node_api.h>
#include <string>
#include "IpcClient.h"

class Homegear {
 public:
  static napi_value Init(napi_env env, napi_value exports);
  static void Destructor(napi_env env, void *native_object, void *finalize_hint);

 private:
  struct OnEventStruct {
    std::string event_source;
    uint64_t peer_id = 0;
    int32_t channel = -1;
    std::string variable_name;
    Ipc::PVariable value;
  };

  struct OnNodeInputStruct {
    std::string node_id;
    Ipc::PVariable node_info;
    uint32_t input_index;
    Ipc::PVariable message;
    bool synchronous = false;
  };

  struct OnInvokeNodeMethodStruct {
    pthread_t thread_id;
    std::string node_id;
    std::string method_name;
    Ipc::PVariable parameters;
  };

  explicit Homegear(const std::string &socket_path);
  ~Homegear();

  static napi_value New(napi_env env, napi_callback_info info);
  static inline napi_value Constructor(napi_env env);

  static napi_value Connected(napi_env env, napi_callback_info info);
  static napi_value Invoke(napi_env env, napi_callback_info info);

  static void OnConnectJs(napi_env env, napi_value callback, void *context, void *data);
  void OnConnect();
  static void OnDisconnectJs(napi_env env, napi_value callback, void *context, void *data);
  void OnDisconnect();
  static void OnEventJs(napi_env env, napi_value callback, void *context, void *data);
  void OnEvent(std::string &event_source, uint64_t peer_id, int32_t channel, const std::string &variable_name, const Ipc::PVariable &value);
  static void OnNodeInputJs(napi_env env, napi_value callback, void *context, void *data);
  void OnNodeInput(const std::string &node_id, const Ipc::PVariable &node_info, uint32_t input_index, const Ipc::PVariable &message, bool synchronous);
  static void OnInvokeNodeMethodJs(napi_env env, napi_value callback, void *context, void *data);
  bool OnInvokeNodeMethod(pthread_t thread_id, const std::string &node_id, const std::string &method_name, const Ipc::PVariable &parameters);

  std::unique_ptr<IpcClient> ipc_client_;
  napi_threadsafe_function on_connect_threadsafe_function_ = nullptr;
  napi_threadsafe_function on_disconnect_threadsafe_function_ = nullptr;
  napi_threadsafe_function on_event_threadsafe_function_ = nullptr;
  napi_threadsafe_function on_node_input_threadsafe_function_ = nullptr;
  napi_threadsafe_function on_invoke_node_method_threadsafe_function_ = nullptr;
  napi_env env_ = nullptr;
  napi_ref wrapper_ = nullptr;
};

#endif //HOMEGEAR_NODEJS__HOMEGEAROBJECT_H_
