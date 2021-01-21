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

#include "IpcClient.h"

IpcClient::IpcClient(const std::string &socketPath) : IIpcClient(socketPath) {
  Ipc::Output::setLogLevel(-1);

  _localRpcMethods.emplace("invokeNodeMethod", std::bind(&IpcClient::InvokeNodeMethod, this, std::placeholders::_1));
  _localRpcMethods.emplace("nodeInput", std::bind(&IpcClient::NodeInput, this, std::placeholders::_1));
}

IpcClient::~IpcClient() {
}

void IpcClient::onConnect() {
  if (on_connect_) on_connect_();
}

void IpcClient::onDisconnect() {
  if (on_disconnect_) on_disconnect_();
}

void IpcClient::InvokeResult(pthread_t thread_id, const Ipc::PVariable &result) {
  std::lock_guard<std::mutex> request_info_guard(local_request_info_mutex_);
  auto request_iterator = local_request_Info_.find(thread_id);
  if (request_iterator != local_request_Info_.end()) {
    std::unique_lock<std::mutex> wait_lock(request_iterator->second->wait_mutex);

    {
      std::lock_guard<std::mutex> result_guard(invoke_results_mutex_);
      auto result_iterator = invoke_results_.find(thread_id);
      if (result_iterator != invoke_results_.end()) {
        auto &element = result_iterator->second;
        if (element) {
          element->finished = true;
          element->result = result;
        }
      }
    }

    wait_lock.unlock();
    request_iterator->second->condition_variable.notify_all();
  }
}

// {{{ RPC methods
Ipc::PVariable IpcClient::broadcastEvent(Ipc::PArray &parameters) {
  if (parameters->size() != 5) return Ipc::Variable::createError(-1, "Wrong parameter count.");

  for (uint32_t i = 0; i < parameters->at(3)->arrayValue->size(); ++i) {
    if (broadcast_event_) broadcast_event_(parameters->at(0)->stringValue, (uint64_t)parameters->at(1)->integerValue64, parameters->at(2)->integerValue, parameters->at(3)->arrayValue->at(i)->stringValue, parameters->at(4)->arrayValue->at(i));
  }

  return std::make_shared<Ipc::Variable>();
}
// }}}

// {{{ RPC methods when used in a Node-BLUE node
Ipc::PVariable IpcClient::InvokeNodeMethod(Ipc::PArray &parameters) {
  if (parameters->size() < 3) return Ipc::Variable::createError(-1, "Wrong parameter count.");

  if (!invoke_node_method_) return Ipc::Variable::createError(-1, "Unknown method (no callback method specified).");

  auto thread_id = pthread_self();
  PLocalRequestInfo request_info;
  std::unique_lock<std::mutex> request_info_guard(local_request_info_mutex_);
  auto request_info_iterator = local_request_Info_.emplace(std::piecewise_construct, std::make_tuple(thread_id), std::make_tuple(std::make_shared<LocalRequestInfo>()));
  if (request_info_iterator.second) request_info = request_info_iterator.first->second;
  if (!request_info) {
    Ipc::Output::printError("Critical: Could not insert local request struct into map.");
    return Ipc::Variable::createError(-32500, "Unknown application error.");
  }
  request_info_guard.unlock();

  PInvokeResultInfo result;
  {
    std::lock_guard<std::mutex> result_guard(invoke_results_mutex_);
    auto result_iterator = invoke_results_.emplace(thread_id, std::make_shared<InvokeResultInfo>());
    if (result_iterator.second) result = result_iterator.first->second;
  }
  if (!result) {
    Ipc::Output::printError("Critical: Could not insert local response struct into map.");
    return Ipc::Variable::createError(-32500, "Unknown application error.");
  }

  if (invoke_node_method_(thread_id, parameters->at(0)->stringValue, parameters->at(1)->stringValue, parameters->at(2))) {
    auto start_time = Ipc::HelperFunctions::getTime();
    std::unique_lock<std::mutex> wait_lock(request_info->wait_mutex);
    while (!request_info->condition_variable.wait_for(wait_lock, std::chrono::milliseconds(1000), [&] {
      return result->finished || _closed || _stopped || _disposing || (Ipc::HelperFunctions::getTime() - start_time > 30000);
    }));
  } else {
    //Not really an error
    result->finished = true;
    result->result = Ipc::Variable::createError(-1, "Unknown method (no callback method specified).");
  }

  if (!result->finished) {
    Ipc::Output::printError("Error: No response received to local RPC request. Method: invokeNodeMethod");
    result->result = Ipc::Variable::createError(-1, "No response received.");
  }

  auto return_value = result->result;

  {
    std::lock_guard<std::mutex> result_guard(invoke_results_mutex_);
    invoke_results_.erase(thread_id);
  }

  {
    request_info_guard.lock();
    local_request_Info_.erase(thread_id);
  }

  return return_value;
}

Ipc::PVariable IpcClient::NodeInput(Ipc::PArray &parameters) {
  if (parameters->size() != 5) return Ipc::Variable::createError(-1, "Wrong parameter count.");

  parameters->at(3)->structValue->emplace("inputIndex", parameters->at(2));
  if (node_input_) node_input_(parameters->at(0)->stringValue, parameters->at(1), parameters->at(2)->integerValue64, parameters->at(3), parameters->at(4)->booleanValue);

  return std::make_shared<Ipc::Variable>();
}
// }}}