// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/serial/serial_event_dispatcher.h"

#include "base/lazy_instance.h"
#include "extensions/browser/api/serial/serial_connection.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extensions_browser_client.h"

namespace extensions {

namespace core_api {

namespace {

bool ShouldPauseOnReceiveError(serial::ReceiveError error) {
  return error == serial::RECEIVE_ERROR_DEVICE_LOST ||
         error == serial::RECEIVE_ERROR_SYSTEM_ERROR ||
         error == serial::RECEIVE_ERROR_DISCONNECTED ||
         error == serial::RECEIVE_ERROR_BREAK ||
         error == serial::RECEIVE_ERROR_FRAME_ERROR ||
         error == serial::RECEIVE_ERROR_OVERRUN ||
         error == serial::RECEIVE_ERROR_BUFFER_OVERFLOW ||
         error == serial::RECEIVE_ERROR_PARITY_ERROR;
}

}  // namespace

static base::LazyInstance<BrowserContextKeyedAPIFactory<SerialEventDispatcher> >
    g_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<SerialEventDispatcher>*
SerialEventDispatcher::GetFactoryInstance() {
  return g_factory.Pointer();
}

// static
SerialEventDispatcher* SerialEventDispatcher::Get(
    content::BrowserContext* context) {
  return BrowserContextKeyedAPIFactory<SerialEventDispatcher>::Get(context);
}

SerialEventDispatcher::SerialEventDispatcher(content::BrowserContext* context)
    : thread_id_(SerialConnection::kThreadId), context_(context) {
  ApiResourceManager<SerialConnection>* manager =
      ApiResourceManager<SerialConnection>::Get(context_);
  DCHECK(manager) << "No serial connection manager.";
  connections_ = manager->data_;
}

SerialEventDispatcher::~SerialEventDispatcher() {
}

SerialEventDispatcher::ReceiveParams::ReceiveParams() {
}

SerialEventDispatcher::ReceiveParams::~ReceiveParams() {
}

void SerialEventDispatcher::PollConnection(const std::string& extension_id,
                                           int connection_id) {
  DCHECK_CURRENTLY_ON(thread_id_);

  ReceiveParams params;
  params.thread_id = thread_id_;
  params.browser_context_id = context_;
  params.extension_id = extension_id;
  params.connections = connections_;
  params.connection_id = connection_id;

  StartReceive(params);
}

// static
void SerialEventDispatcher::StartReceive(const ReceiveParams& params) {
  DCHECK_CURRENTLY_ON(params.thread_id);

  SerialConnection* connection =
      params.connections->Get(params.extension_id, params.connection_id);
  if (!connection)
    return;
  DCHECK(params.extension_id == connection->owner_extension_id());

  if (connection->paused())
    return;

  connection->Receive(base::Bind(&ReceiveCallback, params));
}

// static
void SerialEventDispatcher::ReceiveCallback(const ReceiveParams& params,
                                            const std::vector<char>& data,
                                            serial::ReceiveError error) {
  DCHECK_CURRENTLY_ON(params.thread_id);

  // Note that an error (e.g. timeout) does not necessarily mean that no data
  // was read, so we may fire an onReceive regardless of any error code.
  if (data.size() > 0) {
    serial::ReceiveInfo receive_info;
    receive_info.connection_id = params.connection_id;
    receive_info.data = data;
    scoped_ptr<base::ListValue> args = serial::OnReceive::Create(receive_info);
    scoped_ptr<extensions::Event> event(
        new extensions::Event(extensions::events::UNKNOWN,
                              serial::OnReceive::kEventName, args.Pass()));
    PostEvent(params, event.Pass());
  }

  if (error != serial::RECEIVE_ERROR_NONE) {
    serial::ReceiveErrorInfo error_info;
    error_info.connection_id = params.connection_id;
    error_info.error = error;
    scoped_ptr<base::ListValue> args =
        serial::OnReceiveError::Create(error_info);
    scoped_ptr<extensions::Event> event(
        new extensions::Event(extensions::events::UNKNOWN,
                              serial::OnReceiveError::kEventName, args.Pass()));
    PostEvent(params, event.Pass());
    if (ShouldPauseOnReceiveError(error)) {
      SerialConnection* connection =
          params.connections->Get(params.extension_id, params.connection_id);
      if (connection)
        connection->set_paused(true);
    }
  }

  // Queue up the next read operation.
  BrowserThread::PostTask(
      params.thread_id, FROM_HERE, base::Bind(&StartReceive, params));
}

// static
void SerialEventDispatcher::PostEvent(const ReceiveParams& params,
                                      scoped_ptr<extensions::Event> event) {
  DCHECK_CURRENTLY_ON(params.thread_id);

  BrowserThread::PostTask(BrowserThread::UI,
                          FROM_HERE,
                          base::Bind(&DispatchEvent,
                                     params.browser_context_id,
                                     params.extension_id,
                                     base::Passed(event.Pass())));
}

// static
void SerialEventDispatcher::DispatchEvent(void* browser_context_id,
                                          const std::string& extension_id,
                                          scoped_ptr<extensions::Event> event) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  content::BrowserContext* context =
      reinterpret_cast<content::BrowserContext*>(browser_context_id);
  if (!extensions::ExtensionsBrowserClient::Get()->IsValidContext(context))
    return;

  EventRouter* router = EventRouter::Get(context);
  if (router)
    router->DispatchEventToExtension(extension_id, event.Pass());
}

}  // namespace core_api

}  // namespace extensions
