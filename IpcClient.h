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

#ifndef IPCCLIENT_H_
#define IPCCLIENT_H_

#include <homegear-ipc/IIpcClient.h>

#include <thread>
#include <mutex>
#include <string>
#include <set>

class IpcClient : public Ipc::IIpcClient {
 public:
  struct InvokeResultInfo {
    std::atomic_bool finished{false};
    Ipc::PVariable result;
  };
  typedef std::shared_ptr<InvokeResultInfo> PInvokeResultInfo;

  explicit IpcClient(const std::string &socketPath);
  ~IpcClient() override;

  void InvokeResult(pthread_t thread_id, const Ipc::PVariable &result);

  void SetOnConnect(std::function<void(void)> value) { on_connect_.swap(value); }
  void SetOnDisconnect(std::function<void(void)> value) { on_disconnect_.swap(value); }
  void RemoveOnConnect() { on_connect_ = std::function<void(void)>(); }
  void RemoveOnDisconnect() { on_disconnect_ = std::function<void(void)>(); }
  void SetBroadcastEvent(std::function<void(std::string &event_source, uint64_t peer_id, int32_t channel, const std::string &variable_name, const Ipc::PVariable &value)> value) { broadcast_event_.swap(value); }
  void RemoveBroadcastEvent() { broadcast_event_ = std::function<void(std::string &event_source, uint64_t peer_id, int32_t channel, const std::string &variable_name, const Ipc::PVariable &value)>(); }
  void SetNodeInput(std::function<void(const std::string &node_id, const Ipc::PVariable &node_info, uint32_t input_index, const Ipc::PVariable &message, bool synchronous)> value) { node_input_.swap(value); }
  void RemoveNodeInput() { node_input_ = std::function<void(const std::string &node_id, const Ipc::PVariable &node_info, uint32_t input_index, const Ipc::PVariable message, bool synchronous)>(); }
  void SetInvokeNodeMethod(std::function<bool(pthread_t thread_id, const std::string &node_id, const std::string &method_name, const Ipc::PVariable &parameters)> value) { invoke_node_method_.swap(value); }
  void RemoveInvokeNodeMethod() { invoke_node_method_ = std::function<bool(pthread_t thread_id, const std::string &node_id, const std::string &method_name, const Ipc::PVariable &parameters)>(); }
 private:
  struct LocalRequestInfo {
    std::mutex wait_mutex;
    std::condition_variable condition_variable;
  };
  typedef std::shared_ptr<LocalRequestInfo> PLocalRequestInfo;

  std::function<void(void)> on_connect_;
  std::function<void(void)> on_disconnect_;
  std::function<void(std::string &event_source, uint64_t peer_id, int32_t channel, const std::string &variable_name, const Ipc::PVariable &value)> broadcast_event_;
  std::function<void(const std::string &node_id, const Ipc::PVariable &node_info, uint32_t input_index, const Ipc::PVariable &message, bool synchronous)> node_input_;
  std::function<bool(pthread_t thread_id, const std::string &node_id, const std::string &method_name, const Ipc::PVariable &parameters)> invoke_node_method_;

  std::mutex local_request_info_mutex_;
  std::unordered_map<pthread_t, PLocalRequestInfo> local_request_Info_;
  std::mutex invoke_results_mutex_;
  // We only have one method call per time per thread so we don't need a packet ID.
  std::unordered_map<pthread_t, PInvokeResultInfo> invoke_results_;

  void onConnect() override;
  void onDisconnect() override;

  // {{{ RPC methods
  Ipc::PVariable broadcastEvent(Ipc::PArray &parameters) override;
  // }}}

  // {{{ RPC methods when used in a Node-BLUE node
  Ipc::PVariable InvokeNodeMethod(Ipc::PArray &parameters);
  Ipc::PVariable NodeInput(Ipc::PArray &parameters);
  // }}}
};

#endif
