//
//  plugins.cpp
//  emulex
//
//  Created by Centny on 2/5/17.
//
//

#include "plugins.hpp"
//#include <boost/foreach.hpp>
#define LIBED2K_DEBUG 1
#define LIBED2K_USE_BOOST_DATE_TIME 1
#include <emulex/loader.hpp>

namespace emulex {
namespace n {
class loader_impl;
struct xloader {
    loader_impl* loader = 0;
    Local<Object> settings;
    bool running = false;
    Persistent<Function> shutdown_callback;
};

static xloader XL;

class loader_impl : public emulex::loader_ {
   public:
    Isolate* isolate;
    Persistent<Function> ed2k_callback;
    Local<Object> base_settings;
    Local<Object> ed2k_settings;

   public:
    loader_impl(Local<Object> settings);
    virtual ~loader_impl();
    virtual void do_async_alert(libed2k::alert const& alert);

   protected:
    virtual void on_alert(libed2k::alert const& alert);
    virtual void on_server_initialized(libed2k::server_connection_initialized_alert* alert);
    virtual void on_server_resolved(libed2k::server_name_resolved_alert* alert);
    virtual void on_server_status(libed2k::server_status_alert* alert);
    virtual void on_server_message(libed2k::server_message_alert* alert);
    virtual void on_server_identity(libed2k::server_identity_alert* alert);
    virtual void on_shutdown_completed();
    virtual void call_callback(int argc, Local<v8::Value>* argv);
};
//
struct alert_uv {
    uv_work_t request;
    libed2k::alert* alert;
};
//
static void alert_async(uv_work_t* req) {}
static void alert_async_after(uv_work_t* req, int status) {
    alert_uv* alert = static_cast<alert_uv*>(req->data);
    if (XL.loader) {
        XL.loader->do_async_alert(*alert->alert);
    }
    delete alert->alert;
    delete alert;
}
//
loader_impl::loader_impl(Local<Object> settings) : base_settings(settings) {
    DBG("loader_impl: session is initialing");
    this->isolate = settings->GetIsolate();
    Local<Value> ed2k_v = settings->Get(String::NewFromUtf8(isolate, "ed2k"));
    if (ed2k_v->IsObject()) {
        ed2k_settings = ed2k_v->ToObject();
    } else {
        ed2k_settings = Object::New(isolate);
    }
    Local<Value> timeout = ed2k_settings->Get(String::NewFromUtf8(isolate, "peer_timeout"));
    if (timeout->IsUint32()) {
        ed2k_session_::settings.peer_timeout = timeout->Uint32Value();
    } else {
        ed2k_session_::settings.peer_timeout = 60;
    }
    this->settings.peer_connect_timeout = this->settings.peer_timeout;
    Local<Value> known = ed2k_settings->Get(String::NewFromUtf8(isolate, "m_known_file"));
    if (known->IsString()) {
        String::Utf8Value known_s(known->ToString());
        ed2k_session_::settings.m_known_file = std::string(*known_s, known_s.length());
    }
    Local<Value> port = ed2k_settings->Get(String::NewFromUtf8(isolate, "peer_timeout"));
    if (port->IsUint32()) {
        ed2k_session_::settings.listen_port = port->Uint32Value();
    } else {
        ed2k_session_::settings.listen_port = 4668;
    }
    Local<Value> func = ed2k_settings->Get(String::NewFromUtf8(isolate, "callback"));
    if (func->IsFunction()) {
        Local<Function> cback = Local<Function>::Cast(func);
        ed2k_callback.Reset(isolate, cback);
    }
}

loader_impl::~loader_impl() {}

void loader_impl::do_async_alert(libed2k::alert const& alert) {
    v8::HandleScope hs(isolate);
    emulex::ed2k_session_::on_alert(alert);
}

void loader_impl::on_alert(libed2k::alert const& alert) {
    if (!XL.loader) {
        return;
    }
    alert_uv* uv = new alert_uv;
    uv->alert = alert.clone().release();
    uv->request.data = uv;
    uv_queue_work(uv_default_loop(), &uv->request, alert_async, alert_async_after);
}

void loader_impl::on_server_initialized(libed2k::server_connection_initialized_alert* alert) {
    DBG("loader_impl: server initialized: cid: " << alert->client_id);
    if (ed2k_callback.IsEmpty()) {
        return;
    }
    Local<Value> argv[2];
    argv[0] = String::NewFromUtf8(isolate, "server_initialized");
    Local<Object> vals = Object::New(isolate);
    vals->Set(String::NewFromUtf8(isolate, "host"), String::NewFromUtf8(isolate, alert->host.c_str()));
    vals->Set(String::NewFromUtf8(isolate, "port"), Uint32::New(isolate, alert->port));
    vals->Set(String::NewFromUtf8(isolate, "client_id"), Uint32::New(isolate, alert->client_id));
    argv[1] = vals;
    call_callback(2, argv);
}

void loader_impl::on_server_resolved(libed2k::server_name_resolved_alert* alert) {
    DBG("loader_impl: server name was resolved: " << alert->endpoint);
    if (ed2k_callback.IsEmpty()) {
        return;
    }
    Local<Value> argv[2];
    argv[0] = String::NewFromUtf8(isolate, "server_resolved");
    Local<Object> vals = Object::New(isolate);
    vals->Set(String::NewFromUtf8(isolate, "host"), String::NewFromUtf8(isolate, alert->host.c_str()));
    vals->Set(String::NewFromUtf8(isolate, "port"), Uint32::New(isolate, alert->port));
    vals->Set(String::NewFromUtf8(isolate, "endpoint"), String::NewFromUtf8(isolate, alert->endpoint.c_str()));
    argv[1] = vals;
    call_callback(2, argv);
}

void loader_impl::on_server_status(libed2k::server_status_alert* alert) {
    DBG("loader_impl: server status: files count: " << alert->files_count << " users count " << alert->users_count);
    if (ed2k_callback.IsEmpty()) {
        return;
    }
    Local<v8::Value> argv[2];
    argv[0] = String::NewFromUtf8(isolate, "server_status");
    Local<Object> vals = Object::New(isolate);
    vals->Set(String::NewFromUtf8(isolate, "host"), String::NewFromUtf8(isolate, alert->host.c_str()));
    vals->Set(String::NewFromUtf8(isolate, "port"), Uint32::New(isolate, alert->port));
    vals->Set(String::NewFromUtf8(isolate, "files_count"), Uint32::New(isolate, alert->files_count));
    vals->Set(String::NewFromUtf8(isolate, "users_count"), Uint32::New(isolate, alert->users_count));
    argv[1] = vals;
    call_callback(2, argv);
}

void loader_impl::on_server_message(libed2k::server_message_alert* alert) {
    DBG("loader_impl: msg: " << alert->server_message);
    if (ed2k_callback.IsEmpty()) {
        return;
    }
    Local<v8::Value> argv[2];
    argv[0] = String::NewFromUtf8(isolate, "server_message");
    Local<Object> vals = Object::New(isolate);
    vals->Set(String::NewFromUtf8(isolate, "host"), String::NewFromUtf8(isolate, alert->host.c_str()));
    vals->Set(String::NewFromUtf8(isolate, "port"), Uint32::New(isolate, alert->port));
    vals->Set(String::NewFromUtf8(isolate, "message"), String::NewFromUtf8(isolate, alert->server_message.c_str()));
    argv[1] = vals;
    call_callback(2, argv);
}

void loader_impl::on_server_identity(libed2k::server_identity_alert* alert) {
    DBG("loader_impl: server_identity_alert: " << alert->server_hash << " name:  " << alert->server_name
                                               << " descr: " << alert->server_descr);
    if (ed2k_callback.IsEmpty()) {
        return;
    }
    Local<v8::Value> argv[1];
    argv[0] = String::NewFromUtf8(isolate, "server_identity");
    Local<Object> vals = Object::New(isolate);
    vals->Set(String::NewFromUtf8(isolate, "host"), String::NewFromUtf8(isolate, alert->host.c_str()));
    vals->Set(String::NewFromUtf8(isolate, "port"), Uint32::New(isolate, alert->port));
    vals->Set(String::NewFromUtf8(isolate, "hash"),
              String::NewFromUtf8(isolate, alert->server_hash.toString().c_str()));
    vals->Set(String::NewFromUtf8(isolate, "name"), String::NewFromUtf8(isolate, alert->name.c_str()));
    vals->Set(String::NewFromUtf8(isolate, "descr"), String::NewFromUtf8(isolate, alert->server_descr.c_str()));
    argv[1] = vals;
    call_callback(1, argv);
}

void loader_impl::on_shutdown_completed() {
    DBG("loader_impl: loader shutdown completed");
    XL.running = false;
    if (XL.loader) {
        delete XL.loader;
        XL.loader = 0;
    }
    if (XL.shutdown_callback.IsEmpty()) {
        return;
    }
    Local<Function>::New(isolate, XL.shutdown_callback)->Call(isolate->GetCurrentContext()->Global(), 0, 0);
}

void loader_impl::call_callback(int argc, Local<Value>* argv) {
    if (ed2k_callback.IsEmpty()) {
        return;
    }
    Local<Function>::New(isolate, ed2k_callback)->Call(isolate->GetCurrentContext()->Global(), argc, argv);
}

void bootstrap(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();
    if (args.Length() < 1 || !args[0]->IsObject()) {
        StringException(isolate, "bootstrap receive bad argument, the setting is not object");
        return;
    }
    XL.settings = args[0]->ToObject();
    Local<Value> log = XL.settings->Get(String::NewFromUtf8(isolate, "logging"));
    if (log->IsNumber()) {
        LOGGER_INIT((unsigned char)log->Uint32Value());
    } else {
        LOGGER_INIT('\xFF');
    }
    XL.loader = new loader_impl(XL.settings);
    XL.loader->start();
    XL.running = true;
}

void shutdown(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();
    if (!XL.running) {
        StringException(isolate, "the loader is not running");
        return;
    }
    DBG("emulex: loader is shutdowning...");
    if (args[0]->IsFunction()) {
        XL.shutdown_callback.Reset(isolate, Local<Function>::Cast(args[0]));
    }
    XL.loader->stop();
}

void ed2k_server_connect(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();
    if (!XL.running) {
        StringException(isolate, "emulex is not running");
        return;
    }
    if (!(args.Length() > 3 && args[0]->IsString() && args[1]->IsString() && args[2]->IsNumber() &&
          args[3]->IsBoolean())) {
        std::cout << args[0]->IsString() << args[1]->IsString() << args[2]->IsNumber() << args[3]->IsBoolean()
                  << std::endl;
        StringException(isolate, "emulex ed2k server connect receive wrong arguments");
        return;
    }
    String::Utf8Value name_c(args[0]->ToString());
    std::string name(*name_c);
    String::Utf8Value host_c(args[1]->ToString());
    std::string host(*host_c);
    int port = (int)args[2]->Uint32Value();
    if (args[3]->BooleanValue()) {
        XL.loader->server_connect(name, host, port);
    } else {
        XL.loader->slave_server_connect(name, host, port);
    }
}

void ed2k_add_dht_node(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();
    if (!XL.running) {
        StringException(isolate, "emulex is not running");
        return;
    }
    if (!(args.Length() > 2 && args[0]->IsStringObject() && args[1]->IsNumber() && args[2]->IsStringObject())) {
        StringException(isolate, "emulex ed2k add dht node receive wrong arguments");
        return;
    }
    String::Utf8Value host_c(args[0]->ToString());
    std::string host(*host_c);
    int port = (int)args[1]->Uint32Value();
    String::Utf8Value id_c(args[1]->ToString());
    std::string id(*id_c);
    XL.loader->add_dht_node(host, port, id);
}

void init(Local<Object> exports) {
    NODE_SET_METHOD(exports, "bootstrap", bootstrap);
    NODE_SET_METHOD(exports, "shutdown", shutdown);
    NODE_SET_METHOD(exports, "ed2k_server_connect", ed2k_server_connect);
    NODE_SET_METHOD(exports, "ed2k_add_dht_node", ed2k_add_dht_node);
}

NODE_MODULE(emulex, init);
///
}
}
