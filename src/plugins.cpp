//
//  plugins.cpp
//  emulex
//
//  Created by Centny on 2/5/17.
//
//

#include "plugins.hpp"
#include "hash.hpp"

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
    virtual void on_server_shared(libed2k::shared_files_alert* alert);
    virtual void on_finished_transfer(libed2k::finished_transfer_alert* alert);
    virtual void on_resumed_data_transfer(libed2k::resumed_transfer_alert* alert);
    virtual void on_paused_data_transfer(libed2k::paused_transfer_alert* alert);
    virtual void on_deleted_data_transfer(libed2k::deleted_transfer_alert* alert);
    virtual void on_state_changed(libed2k::state_changed_alert* alert);
    virtual void on_transfer_added(libed2k::added_transfer_alert* alert);
    virtual void on_portmap(libed2k::portmap_alert* alert);
    virtual void on_portmap_error(libed2k::portmap_error_alert* alert);
    virtual void on_shutdown_completed();
    virtual void call_callback(int argc, Local<v8::Value>* argv);
};

const char* transfer_status_c(libed2k::transfer_status::state_t t) {
    switch (t) {
        case libed2k::transfer_status::queued_for_checking:
            return "queued_for_checking";
        case libed2k::transfer_status::checking_files:
            return "checking_files";
        case libed2k::transfer_status::downloading_metadata:
            return "downloading_metadata";
        case libed2k::transfer_status::downloading:
            return "downloading";
        case libed2k::transfer_status::finished:
            return "finished";
        case libed2k::transfer_status::seeding:
            return "seeding";
        case libed2k::transfer_status::allocating:
            return "allocating";
        case libed2k::transfer_status::checking_resume_data:
            return "checking_resume_data";
        default:
            return "unknow";
    }
}
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
    Local<Value> listen_port = ed2k_settings->Get(String::NewFromUtf8(isolate, "port"));
    if (listen_port->IsNumber()) {
        ed2k_session_::settings.listen_port = listen_port->Uint32Value();
    } else {
        ed2k_session_::settings.listen_port = 4132;
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
    vals->Set(String::NewFromUtf8(isolate, "client_id"), Number::New(isolate, (int64_t)alert->client_id));
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
    DBG("loader_impl: server_identity_alert: " << alert->server_hash.toString() << " name:  " << alert->server_name
                                               << " descr: " << alert->server_descr);
    if (ed2k_callback.IsEmpty()) {
        return;
    }
    Local<v8::Value> argv[2];
    argv[0] = String::NewFromUtf8(isolate, "server_identity");
    Local<Object> vals = Object::New(isolate);
    vals->Set(String::NewFromUtf8(isolate, "host"), String::NewFromUtf8(isolate, alert->host.c_str()));
    vals->Set(String::NewFromUtf8(isolate, "port"), Uint32::New(isolate, alert->port));
    vals->Set(String::NewFromUtf8(isolate, "hash"),
              String::NewFromUtf8(isolate, alert->server_hash.toString().c_str()));
    vals->Set(String::NewFromUtf8(isolate, "name"), String::NewFromUtf8(isolate, alert->name.c_str()));
    vals->Set(String::NewFromUtf8(isolate, "descr"), String::NewFromUtf8(isolate, alert->server_descr.c_str()));
    argv[1] = vals;
    call_callback(2, argv);
}

void loader_impl::on_server_shared(libed2k::shared_files_alert* alert) {
    std::vector<libed2k::shared_file_entry> collection = alert->m_files.m_collection;
    DBG("loader_impl: search RESULT: " << collection.size());
    if (ed2k_callback.IsEmpty()) {
        return;
    }
    Local<Array> fs = Array::New(isolate, collection.size());
    for (size_t n = 0; n < collection.size(); ++n) {
        Local<Object> file = Object::New(isolate);
        boost::uint64_t fsize = 0;
        boost::shared_ptr<libed2k::base_tag> low = collection[n].m_list.getTagByNameId(libed2k::FT_FILESIZE);
        boost::shared_ptr<libed2k::base_tag> hi = collection[n].m_list.getTagByNameId(libed2k::FT_FILESIZE_HI);
        boost::shared_ptr<libed2k::base_tag> src = collection[n].m_list.getTagByNameId(libed2k::FT_SOURCES);
        if (low.get()) {
            fsize = low->asInt();
        }
        if (hi.get()) {
            fsize += hi->asInt() << 32;
        }
        file->Set(String::NewFromUtf8(isolate, "size"), Number::New(isolate, (uint32_t)fsize));
        file->Set(String::NewFromUtf8(isolate, "size_h"), Number::New(isolate, (uint32_t)(fsize >> 32)));
        file->Set(String::NewFromUtf8(isolate, "sources"), Number::New(isolate, src->asInt()));
        file->Set(String::NewFromUtf8(isolate, "hash"),
                  String::NewFromUtf8(isolate, collection[n].m_hFile.toString().c_str()));
        file->Set(
            String::NewFromUtf8(isolate, "name"),
            String::NewFromUtf8(isolate, collection[n].m_list.getStringTagByNameId(libed2k::FT_FILENAME).c_str()));
        file->Set(
            String::NewFromUtf8(isolate, "format"),
            String::NewFromUtf8(isolate, collection[n].m_list.getStringTagByNameId(libed2k::FT_FILEFORMAT).c_str()));
        fs->Set(n, file);
    }
    Local<v8::Value> argv[2];
    argv[0] = String::NewFromUtf8(isolate, "server_shared");
    argv[1] = fs;
    call_callback(2, argv);
}

void loader_impl::on_finished_transfer(libed2k::finished_transfer_alert* alert) {
    DBG("ed2k_session_: finished transfer: " << alert->m_handle.save_path());
    if (ed2k_callback.IsEmpty()) {
        return;
    }
    Local<Value> argv[2];
    argv[0] = String::NewFromUtf8(isolate, "finished_transfer");
    Local<Object> vals = Object::New(isolate);
    libed2k::add_transfer_params params = alert->m_handle.params();
    vals->Set(String::NewFromUtf8(isolate, "valid"), Boolean::New(isolate, alert->m_handle.is_valid()));
    vals->Set(String::NewFromUtf8(isolate, "name"), String::NewFromUtf8(isolate, alert->m_handle.name().c_str()));
    vals->Set(String::NewFromUtf8(isolate, "save_path"), String::NewFromUtf8(isolate, params.file_path.c_str()));
    vals->Set(String::NewFromUtf8(isolate, "hash"), String::NewFromUtf8(isolate, params.file_hash.toString().c_str()));
    vals->Set(String::NewFromUtf8(isolate, "size"), Number::New(isolate, (uint32_t)alert->m_handle.size()));
    vals->Set(String::NewFromUtf8(isolate, "size_h"), Number::New(isolate, (uint32_t)(alert->m_handle.size() >> 32)));
    argv[1] = vals;
    call_callback(2, argv);
}

void loader_impl::on_resumed_data_transfer(libed2k::resumed_transfer_alert* alert) {
    DBG("ed2k_session_: resumed data transfer: " << alert->m_handle.save_path());
    if (ed2k_callback.IsEmpty()) {
        return;
    }
    Local<Value> argv[2];
    argv[0] = String::NewFromUtf8(isolate, "resumed_data_transfer");
    Local<Object> vals = Object::New(isolate);
    vals->Set(String::NewFromUtf8(isolate, "hash"),
              String::NewFromUtf8(isolate, alert->m_handle.hash().toString().c_str()));
    argv[1] = vals;
    call_callback(2, argv);
}

void loader_impl::on_paused_data_transfer(libed2k::paused_transfer_alert* alert) {
    DBG("ed2k_session_: paused data transfer: " << alert->m_handle.save_path());
    if (ed2k_callback.IsEmpty()) {
        return;
    }
    Local<Value> argv[2];
    argv[0] = String::NewFromUtf8(isolate, "paused_data_transfer");
    Local<Object> vals = Object::New(isolate);
    vals->Set(String::NewFromUtf8(isolate, "hash"),
              String::NewFromUtf8(isolate, alert->m_handle.hash().toString().c_str()));
    argv[1] = vals;
    call_callback(2, argv);
}

void loader_impl::on_deleted_data_transfer(libed2k::deleted_transfer_alert* alert) {
    DBG("ed2k_session_: deleted data transfer: " << alert->m_hash.toString());
    if (ed2k_callback.IsEmpty()) {
        return;
    }
    Local<Value> argv[2];
    argv[0] = String::NewFromUtf8(isolate, "deleted_data_transfer");
    Local<Object> vals = Object::New(isolate);
    vals->Set(String::NewFromUtf8(isolate, "hash"), String::NewFromUtf8(isolate, alert->m_hash.toString().c_str()));
    argv[1] = vals;
    call_callback(2, argv);
}

void loader_impl::on_state_changed(libed2k::state_changed_alert* alert) {
    DBG("ed2k_session_: state changed" << alert->m_handle.hash());
    if (ed2k_callback.IsEmpty()) {
        return;
    }
    Local<Value> argv[2];
    argv[0] = String::NewFromUtf8(isolate, "state_changed");
    Local<Object> vals = Object::New(isolate);
    vals->Set(String::NewFromUtf8(isolate, "hash"),
              String::NewFromUtf8(isolate, alert->m_handle.hash().toString().c_str()));
    vals->Set(String::NewFromUtf8(isolate, "old"), String::NewFromUtf8(isolate, transfer_status_c(alert->m_old_state)));
    vals->Set(String::NewFromUtf8(isolate, "new"), String::NewFromUtf8(isolate, transfer_status_c(alert->m_old_state)));
    argv[1] = vals;
    call_callback(2, argv);
}

void loader_impl::on_transfer_added(libed2k::added_transfer_alert* alert) {
    Local<Value> argv[2];
    argv[0] = String::NewFromUtf8(isolate, "transfer_added");
    Local<Object> vals = Object::New(isolate);
    vals->Set(String::NewFromUtf8(isolate, "hash"),
              String::NewFromUtf8(isolate, alert->m_handle.hash().toString().c_str()));
    argv[1] = vals;
    call_callback(2, argv);
}

void loader_impl::on_portmap(libed2k::portmap_alert* alert) {
    Local<Value> argv[2];
    argv[0] = String::NewFromUtf8(isolate, "portmap");
    Local<Object> vals = Object::New(isolate);
    vals->Set(String::NewFromUtf8(isolate, "mapping"), Number::New(isolate, (uint32_t)alert->mapping));
    vals->Set(String::NewFromUtf8(isolate, "external_port"), Number::New(isolate, (uint32_t)alert->external_port));
    argv[1] = vals;
    call_callback(2, argv);
}

void loader_impl::on_portmap_error(libed2k::portmap_error_alert* alert) {
    Local<Value> argv[2];
    argv[0] = String::NewFromUtf8(isolate, "portmap_error");
    Local<Object> vals = Object::New(isolate);
    vals->Set(String::NewFromUtf8(isolate, "mapping"), Number::New(isolate, (uint32_t)alert->mapping));
    argv[1] = vals;
    call_callback(2, argv);
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
    if (!(args.Length() > 2 && args[0]->IsString() && args[1]->IsNumber() && args[2]->IsString())) {
        StringException(isolate, "emulex ed2k add dht node receive wrong arguments");
        return;
    }
    String::Utf8Value host_c(args[0]->ToString());
    std::string host(*host_c);
    int port = (int)args[1]->Uint32Value();
    String::Utf8Value id_c(args[2]->ToString());
    std::string id(*id_c);
    XL.loader->add_dht_node(host, port, id);
}

void search_file(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();
    if (!XL.running) {
        StringException(isolate, "emulex is not running");
        return;
    }
    if (!(args.Length() > 0 && args[0]->IsObject())) {
        StringException(isolate, "emulex ed2k search file receive wrong arguments");
        return;
    }
    Local<Object> vargs = args[0]->ToObject();
    //
    std::string query;
    Local<Value> query_v = vargs->Get(String::NewFromUtf8(isolate, "query"));
    if (query_v->IsString()) {
        String::Utf8Value query_s(query_v->ToString());
        if (query_s.length()) {
            query = std::string(*query_s, query_s.length());
        } else {
            StringException(isolate, "emulex ed2k search file fail with args.query is empty");
            return;
        }
    } else {
        StringException(isolate, "emulex ed2k search file fail with args.query is not string");
        return;
    }
    //
    std::string extension;
    Local<Value> extension_v = vargs->Get(String::NewFromUtf8(isolate, "extension"));
    if (extension_v->IsString()) {
        String::Utf8Value extension_s(extension_v->ToString());
        query = std::string(*extension_s, extension_s.length());
    }
    //
    boost::uint64_t min_size = 0;
    Local<Value> min_size_v = vargs->Get(String::NewFromUtf8(isolate, "min_size"));
    if (min_size_v->IsNumber()) {
        min_size = (boost::uint64_t)min_size_v->NumberValue();
    }
    Local<Value> min_size_h_v = vargs->Get(String::NewFromUtf8(isolate, "min_size_h"));
    if (min_size_h_v->IsNumber()) {
        min_size += ((boost::uint64_t)min_size_v->NumberValue()) << 32;
    }
    //
    boost::uint64_t max_size = 0;
    Local<Value> max_size_v = vargs->Get(String::NewFromUtf8(isolate, "max_size"));
    if (max_size_v->IsNumber()) {
        max_size = (boost::uint64_t)max_size_v->NumberValue();
    }
    Local<Value> max_size_h_v = vargs->Get(String::NewFromUtf8(isolate, "max_size_h"));
    if (max_size_h_v->IsNumber()) {
        max_size += ((boost::uint64_t)max_size_h_v->NumberValue()) << 32;
    }
    //
    //
    XL.loader->search_file(query, extension, min_size, max_size);
}

void search_hash_file(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();
    if (!XL.running) {
        StringException(isolate, "emulex is not running");
        return;
    }
    if (!(args.Length() > 1 && args[0]->IsString() && args[1]->IsNumber())) {
        StringException(isolate, "emulex ed2k search hash file receive wrong arguments");
        return;
    }
    String::Utf8Value hash_c(args[0]->ToString());
    std::string hash(*hash_c);
    HashType type = MD4_H;
    switch (args[1]->Uint32Value()) {
        case 1:
            type = MD4_H;
            break;
        case 2:
            type = MD5_H;
            break;
        case 3:
            type = SHA1_H;
            break;
        default:
            type = MD4_H;
    }
    XL.loader->search_file(hash, type);
}

void piece_availability(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();
    if (!XL.running) {
        StringException(isolate, "emulex is not running");
        return;
    }
    if (!(args.Length() > 0 && args[0]->IsObject())) {
        StringException(isolate, "emulex ed2k piece availability receive wrong arguments");
        return;
    }
    Local<Object> vargs = args[0]->ToObject();
    //
    std::string hash;
    Local<Value> hash_v = vargs->Get(String::NewFromUtf8(isolate, "hash"));
    if (hash_v->IsString()) {
        String::Utf8Value hash_s(hash_v->ToString());
        if (hash_s.length()) {
            hash = std::string(*hash_s, hash_s.length());
        } else {
            StringException(isolate, "emulex ed2k piece availability fail with args.hash is empty");
            return;
        }
    } else {
        StringException(isolate, "emulex ed2k piece availability fail with args.hash is not string");
        return;
    }
    auto hash_4 = libed2k::md4_hash::fromString(hash);
    auto th = XL.loader->find_transfer(hash_4);
    if (th.is_valid()) {
        std::vector<int> avail;
        th.piece_availability(avail);
        Local<Array> ts = Array::New(isolate, avail.size());
        for (size_t i = 0; i < avail.size(); i++) {
            ts->Set(i, Uint32::New(isolate, avail[i]));
        }
        args.GetReturnValue().Set(ts);
    } else {
        args.GetReturnValue().Set(Null(isolate));
    }
}

void add_transfer(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();
    if (!XL.running) {
        StringException(isolate, "emulex is not running");
        return;
    }
    if (!(args.Length() > 0 && args[0]->IsObject())) {
        StringException(isolate, "emulex ed2k add transfer receive wrong arguments");
        return;
    }
    Local<Object> vargs = args[0]->ToObject();
    //
    std::string hash;
    Local<Value> hash_v = vargs->Get(String::NewFromUtf8(isolate, "hash"));
    if (hash_v->IsString()) {
        String::Utf8Value hash_s(hash_v->ToString());
        if (hash_s.length()) {
            hash = std::string(*hash_s, hash_s.length());
        } else {
            StringException(isolate, "emulex ed2k add transfer fail with args.hash is empty");
            return;
        }
    } else {
        StringException(isolate, "emulex ed2k add transfer fail with args.hash is not string");
        return;
    }
    //
    std::string path;
    Local<Value> path_v = vargs->Get(String::NewFromUtf8(isolate, "path"));
    if (path_v->IsString()) {
        String::Utf8Value path_s(path_v->ToString());
        if (path_s.length()) {
            path = std::string(*path_s, path_s.length());
        } else {
            StringException(isolate, "emulex ed2k add transfer fail with args.path is empty");
            return;
        }
    } else {
        StringException(isolate, "emulex ed2k add transfer fail with args.path is not string");
        return;
    }
    //
    boost::uint64_t size = 0;
    Local<Value> size_v = vargs->Get(String::NewFromUtf8(isolate, "size"));
    if (size_v->IsNumber()) {
        size = (boost::uint64_t)size_v->NumberValue();
        if (size < 1) {
            StringException(isolate, "emulex ed2k add transfer fail with args.size is zero");
            return;
        }
    } else {
        StringException(isolate, "emulex ed2k add transfer fail with args.size is not int");
        return;
    }
    Local<Value> size_h_v = vargs->Get(String::NewFromUtf8(isolate, "size_h"));
    if (size_h_v->IsNumber()) {
        size += ((boost::uint64_t)size_v->NumberValue()) << 32;
    }
    //
    std::vector<std::string> parts;
    Local<Value> parts_v = vargs->Get(String::NewFromUtf8(isolate, "parts"));
    if (parts_v->IsArray()) {
        Local<Array> parts_l = Local<Array>::Cast(parts_v);
        for (uint32_t i = 0; i < parts_l->Length(); i++) {
            Local<Value> part_v = parts_l->Get(i);
            String::Utf8Value part_s(part_v->ToString());
            if (part_s.length()) {
                parts.push_back(std::string(*part_s, part_s.length()));
            } else {
                StringException(isolate, "emulex ed2k add transfer fail with args.parts is not string");
                return;
            }
        }
    }
    //
    std::string resources;
    Local<Value> resources_v = vargs->Get(String::NewFromUtf8(isolate, "resources"));
    if (resources_v->IsString()) {
        String::Utf8Value resources_s(resources_v->ToString());
        if (resources_s.length()) {
            resources = std::string(*resources_s, resources_s.length());
        } else {
            StringException(isolate, "emulex ed2k add transfer fail with args.resources is empty");
            return;
        }
    }
    //
    bool seed;
    Local<Value> seed_v = vargs->Get(String::NewFromUtf8(isolate, "seed"));
    seed = seed_v->IsBoolean() && seed_v->BooleanValue();
    //
    //
    DBG("emulex: add transfter by hash:" << hash << ",path:" << path << ",size:" << size << ",parts:" << parts.size()
                                         << ",resource:" << resources << ",seed:" << seed);
    XL.loader->add_transfer(hash, path, size, parts, resources, seed);
}

void list_transfer(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();
    if (!XL.running) {
        StringException(isolate, "emulex is not running");
        return;
    }
    std::vector<libed2k::transfer_handle> ths = XL.loader->list_transfter();
    Local<Array> ts = Array::New(isolate, ths.size());
    for (size_t i = 0; i < ths.size(); i++) {
        const libed2k::transfer_handle& th = ths.at(i);
        const libed2k::transfer_status status = th.status();
        Local<Object> t = Object::New(isolate);
        t->Set(String::NewFromUtf8(isolate, "emd4"), String::NewFromUtf8(isolate, th.hash().toString().c_str()));
        t->Set(String::NewFromUtf8(isolate, "save_path"), String::NewFromUtf8(isolate, th.save_path().c_str()));
        t->Set(String::NewFromUtf8(isolate, "size"), Number::New(isolate, th.size()));
        t->Set(String::NewFromUtf8(isolate, "state"), Uint32::New(isolate, th.state()));
        t->Set(String::NewFromUtf8(isolate, "all_time_download"), Uint32::New(isolate, status.all_time_download));
        t->Set(String::NewFromUtf8(isolate, "all_time_upload"), Uint32::New(isolate, status.all_time_upload));
        t->Set(String::NewFromUtf8(isolate, "download_payload_rate"),
               Uint32::New(isolate, status.download_payload_rate));
        t->Set(String::NewFromUtf8(isolate, "upload_payload_rate"), Uint32::New(isolate, status.upload_payload_rate));
        t->Set(String::NewFromUtf8(isolate, "download_payload_rate"),
               Uint32::New(isolate, status.download_payload_rate));
        t->Set(String::NewFromUtf8(isolate, "num_seeds"), Uint32::New(isolate, status.num_seeds));
        t->Set(String::NewFromUtf8(isolate, "num_peers"), Uint32::New(isolate, status.num_peers));
        t->Set(String::NewFromUtf8(isolate, "total_done"), Uint32::New(isolate, status.total_done));
        t->Set(String::NewFromUtf8(isolate, "progress"), Number::New(isolate, status.progress));
        ts->Set(i, t);
    }
    args.GetReturnValue().Set(ts);
}

void find_transfer(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();
    if (!XL.running) {
        StringException(isolate, "emulex is not running");
        return;
    }
    if (!(args.Length() > 0 && args[0]->IsObject())) {
        StringException(isolate, "emulex ed2k find transfer receive wrong arguments");
        return;
    }
    Local<Object> vargs = args[0]->ToObject();
    //
    std::string hash;
    Local<Value> hash_v = vargs->Get(String::NewFromUtf8(isolate, "hash"));
    if (hash_v->IsString()) {
        String::Utf8Value hash_s(hash_v->ToString());
        if (hash_s.length()) {
            hash = std::string(*hash_s, hash_s.length());
        } else {
            StringException(isolate, "emulex ed2k find transfer fail with args.hash is empty");
            return;
        }
    } else {
        StringException(isolate, "emulex ed2k find transfer fail with args.hash is not string");
        return;
    }
    auto hash_4 = libed2k::md4_hash::fromString(hash);
    auto th = XL.loader->find_transfer(hash_4);
    Local<Object> t = Object::New(isolate);
    if (th.is_valid()) {
        const libed2k::transfer_status status = th.status();
        t->Set(String::NewFromUtf8(isolate, "is_valid"), Boolean::New(isolate, true));
        t->Set(String::NewFromUtf8(isolate, "emd4"), String::NewFromUtf8(isolate, th.hash().toString().c_str()));
        t->Set(String::NewFromUtf8(isolate, "save_path"), String::NewFromUtf8(isolate, th.save_path().c_str()));
        t->Set(String::NewFromUtf8(isolate, "size"), Number::New(isolate, th.size()));
        t->Set(String::NewFromUtf8(isolate, "state"), Uint32::New(isolate, th.state()));
        t->Set(String::NewFromUtf8(isolate, "all_time_download"), Uint32::New(isolate, status.all_time_download));
        t->Set(String::NewFromUtf8(isolate, "all_time_upload"), Uint32::New(isolate, status.all_time_upload));
        t->Set(String::NewFromUtf8(isolate, "download_payload_rate"),
               Uint32::New(isolate, status.download_payload_rate));
        t->Set(String::NewFromUtf8(isolate, "upload_payload_rate"), Uint32::New(isolate, status.upload_payload_rate));
        t->Set(String::NewFromUtf8(isolate, "download_payload_rate"),
               Uint32::New(isolate, status.download_payload_rate));
        t->Set(String::NewFromUtf8(isolate, "num_seeds"), Uint32::New(isolate, status.num_seeds));
        t->Set(String::NewFromUtf8(isolate, "num_peers"), Uint32::New(isolate, status.num_peers));
        t->Set(String::NewFromUtf8(isolate, "total_done"), Uint32::New(isolate, status.total_done));
        t->Set(String::NewFromUtf8(isolate, "progress"), Number::New(isolate, status.progress));
    } else {
        t->Set(String::NewFromUtf8(isolate, "is_valid"), Boolean::New(isolate, false));
    }
    args.GetReturnValue().Set(t);
}

void pause_transfer(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();
    if (!XL.running) {
        StringException(isolate, "emulex is not running");
        return;
    }
    if (!(args.Length() > 0 && args[0]->IsObject())) {
        StringException(isolate, "emulex ed2k pause transfer receive wrong arguments");
        return;
    }
    Local<Object> vargs = args[0]->ToObject();
    //
    std::string hash;
    Local<Value> hash_v = vargs->Get(String::NewFromUtf8(isolate, "hash"));
    if (hash_v->IsString()) {
        String::Utf8Value hash_s(hash_v->ToString());
        if (hash_s.length()) {
            hash = std::string(*hash_s, hash_s.length());
        } else {
            StringException(isolate, "emulex ed2k pause transfer fail with args.hash is empty");
            return;
        }
    } else {
        StringException(isolate, "emulex ed2k pause transfer fail with args.hash is not string");
        return;
    }
    auto hash_4 = libed2k::md4_hash::fromString(hash);
    auto th = XL.loader->pause_transfer(hash_4);
    args.GetReturnValue().Set(Boolean::New(isolate, th.is_valid()));
}

void resume_transfer(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();
    if (!XL.running) {
        StringException(isolate, "emulex is not running");
        return;
    }
    if (!(args.Length() > 0 && args[0]->IsObject())) {
        StringException(isolate, "emulex ed2k resume transfer receive wrong arguments");
        return;
    }
    Local<Object> vargs = args[0]->ToObject();
    //
    std::string hash;
    Local<Value> hash_v = vargs->Get(String::NewFromUtf8(isolate, "hash"));
    if (hash_v->IsString()) {
        String::Utf8Value hash_s(hash_v->ToString());
        if (hash_s.length()) {
            hash = std::string(*hash_s, hash_s.length());
        } else {
            StringException(isolate, "emulex ed2k resume transfer fail with args.hash is empty");
            return;
        }
    } else {
        StringException(isolate, "emulex ed2k resume transfer fail with args.hash is not string");
        return;
    }
    auto hash_4 = libed2k::md4_hash::fromString(hash);
    auto th = XL.loader->resume_transfer(hash_4);
    args.GetReturnValue().Set(Boolean::New(isolate, th.is_valid()));
}

void remove_transfer(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();
    if (!XL.running) {
        StringException(isolate, "emulex is not running");
        return;
    }
    if (!(args.Length() > 0 && args[0]->IsObject())) {
        StringException(isolate, "emulex ed2k resume transfer receive wrong arguments");
        return;
    }
    Local<Object> vargs = args[0]->ToObject();
    //
    std::string hash;
    Local<Value> hash_v = vargs->Get(String::NewFromUtf8(isolate, "hash"));
    if (hash_v->IsString()) {
        String::Utf8Value hash_s(hash_v->ToString());
        if (hash_s.length()) {
            hash = std::string(*hash_s, hash_s.length());
        } else {
            StringException(isolate, "emulex ed2k resume transfer fail with args.hash is empty");
            return;
        }
    } else {
        StringException(isolate, "emulex ed2k resume transfer fail with args.hash is not string");
        return;
    }
    auto hash_4 = libed2k::md4_hash::fromString(hash);
    auto th = XL.loader->remove_transfer(hash_4);
    args.GetReturnValue().Set(Boolean::New(isolate, th.is_valid()));
}

void restore_transfer(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();
    if (!XL.running) {
        StringException(isolate, "emulex is not running");
        return;
    }
    if (!(args.Length() > 0 && args[0]->IsObject())) {
        StringException(isolate, "emulex ed2k restore transfer receive wrong arguments");
        return;
    }
    Local<Object> vargs = args[0]->ToObject();
    //
    std::string path;
    Local<Value> path_v = vargs->Get(String::NewFromUtf8(isolate, "path"));
    if (path_v->IsString()) {
        String::Utf8Value path_s(path_v->ToString());
        if (path_s.length()) {
            path = std::string(*path_s, path_s.length());
        } else {
            StringException(isolate, "emulex ed2k restore transfer fail with args.hash is empty");
            return;
        }
    } else {
        StringException(isolate, "emulex ed2k restore transfer fail with args.hash is not string");
        return;
    }
    auto th = XL.loader->restore_transfer(path);
    args.GetReturnValue().Set(Boolean::New(isolate, th.is_valid()));
}

void load_node_dat(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();
    if (!(args.Length() > 0 && args[0]->IsObject())) {
        StringException(isolate, "emulex ed2k load node dat receive wrong arguments");
        return;
    }
    Local<Object> vargs = args[0]->ToObject();
    //
    //
    std::string path;
    Local<Value> path_v = vargs->Get(String::NewFromUtf8(isolate, "path"));
    if (path_v->IsString()) {
        String::Utf8Value path_s(path_v->ToString());
        if (path_s.length()) {
            path = std::string(*path_s, path_s.length());
        } else {
            StringException(isolate, "emulex ed2k load node dat fail with args.path is empty");
            return;
        }
    } else {
        StringException(isolate, "emulex ed2k load node dat fail with args.path is not string");
        return;
    }
    libed2k::kad_nodes_dat knd;
    if (!emulex::load_nodes(knd, path)) {
        std::string emsg = "emulex ed2k load node dat fail with read file " + path + " error";
        StringException(isolate, emsg.c_str());
        return;
    }
    //
    uint32_t nidx = 0;
    Local<Array> nodes =
        Array::New(isolate, knd.bootstrap_container.m_collection.size() + knd.contacts_container.size());
    for (size_t i = 0; i != knd.bootstrap_container.m_collection.size(); ++i) {
        libed2k::kad_entry& entry = knd.bootstrap_container.m_collection[i];
        Local<Object> node = Object::New(isolate);
        node->Set(String::NewFromUtf8(isolate, "address"),
                  String::NewFromUtf8(isolate, libed2k::int2ipstr(entry.address.address).c_str()));
        node->Set(String::NewFromUtf8(isolate, "udp_port"), Number::New(isolate, (uint16_t)entry.address.udp_port));
        node->Set(String::NewFromUtf8(isolate, "kid"), String::NewFromUtf8(isolate, entry.kid.toString().c_str()));
        node->Set(String::NewFromUtf8(isolate, "bootstrap"), Boolean::New(isolate, true));
        nodes->Set(nidx, node);
        nidx++;
    }

    for (std::list<libed2k::kad_entry>::const_iterator itr = knd.contacts_container.begin();
         itr != knd.contacts_container.end(); ++itr) {
        Local<Object> node = Object::New(isolate);
        node->Set(String::NewFromUtf8(isolate, "address"),
                  String::NewFromUtf8(isolate, libed2k::int2ipstr(itr->address.address).c_str()));
        node->Set(String::NewFromUtf8(isolate, "udp_port"), Number::New(isolate, (uint16_t)itr->address.udp_port));
        node->Set(String::NewFromUtf8(isolate, "kid"), String::NewFromUtf8(isolate, itr->kid.toString().c_str()));
        node->Set(String::NewFromUtf8(isolate, "bootstrap"), Boolean::New(isolate, false));
        nodes->Set(nidx, node);
        nidx++;
    }
    args.GetReturnValue().Set(nodes);
}

void load_server_met(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();
    if (!(args.Length() > 0 && args[0]->IsObject())) {
        StringException(isolate, "emulex ed2k load server met receive wrong arguments");
        return;
    }
    Local<Object> vargs = args[0]->ToObject();
    //
    //
    std::string path;
    Local<Value> path_v = vargs->Get(String::NewFromUtf8(isolate, "path"));
    if (path_v->IsString()) {
        String::Utf8Value path_s(path_v->ToString());
        if (path_s.length()) {
            path = std::string(*path_s, path_s.length());
        } else {
            StringException(isolate, "emulex ed2k load server met fail with args.path is empty");
            return;
        }
    } else {
        StringException(isolate, "emulex ed2k load server met fail with args.path is not string");
        return;
    }
    libed2k::server_met sm;
    if (!emulex::load_server_met(sm, path)) {
        std::string emsg = "emulex ed2k load server met fail with read file " + path + " error";
        StringException(isolate, emsg.c_str());
        return;
    }
    //
    uint32_t nidx = 0;
    Local<Array> nodes = Array::New(isolate, sm.m_servers.m_size);
    for (size_t n = 0; n < sm.m_servers.m_size; ++n) {
        Local<Object> node = Object::New(isolate);
        node->Set(String::NewFromUtf8(isolate, "address"),
                  String::NewFromUtf8(
                      isolate, libed2k::int2ipstr(sm.m_servers.m_collection.at(n).m_network_point.m_nIP).c_str()));
        node->Set(String::NewFromUtf8(isolate, "port"),
                  Number::New(isolate, (uint16_t)sm.m_servers.m_collection.at(n).m_network_point.m_nPort));
        node->Set(
            String::NewFromUtf8(isolate, "name"),
            String::NewFromUtf8(
                isolate, sm.m_servers.m_collection.at(n).m_list.getStringTagByNameId(libed2k::ST_SERVERNAME).c_str()));
        nodes->Set(nidx, node);
        nidx++;
    }
    args.GetReturnValue().Set(nodes);
}

void emulex_env(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();
    Local<Object> envs = Object::New(isolate);
    envs->Set(String::NewFromUtf8(isolate, "PIECE_SIZE"), Number::New(isolate, libed2k::PIECE_SIZE));
    envs->Set(String::NewFromUtf8(isolate, "BLOCK_SIZE"), Number::New(isolate, libed2k::BLOCK_SIZE));
    envs->Set(String::NewFromUtf8(isolate, "HIGHEST_LOWID_ED2K"), Number::New(isolate, libed2k::HIGHEST_LOWID_ED2K));
    envs->Set(String::NewFromUtf8(isolate, "MAX_ED2K_PACKET_LEN"), Number::New(isolate, libed2k::MAX_ED2K_PACKET_LEN));
    envs->Set(String::NewFromUtf8(isolate, "LIBED2K_SERVER_CONN_MAX_SIZE"),
              Number::New(isolate, libed2k::LIBED2K_SERVER_CONN_MAX_SIZE));
    args.GetReturnValue().Set(envs);
}

void init(Local<Object> exports) {
    NODE_SET_METHOD(exports, "bootstrap", bootstrap);
    NODE_SET_METHOD(exports, "shutdown", shutdown);
    NODE_SET_METHOD(exports, "ed2k_server_connect", ed2k_server_connect);
    NODE_SET_METHOD(exports, "ed2k_add_dht_node", ed2k_add_dht_node);
    NODE_SET_METHOD(exports, "ed2k_server_connect", ed2k_server_connect);
    NODE_SET_METHOD(exports, "search_file", search_file);
    NODE_SET_METHOD(exports, "search_hash_file", search_hash_file);
    NODE_SET_METHOD(exports, "piece_availability", piece_availability);
    NODE_SET_METHOD(exports, "add_transfer", add_transfer);
    NODE_SET_METHOD(exports, "list_transfer", list_transfer);
    NODE_SET_METHOD(exports, "find_transfer", find_transfer);
    NODE_SET_METHOD(exports, "pause_transfer", pause_transfer);
    NODE_SET_METHOD(exports, "resume_transfer", resume_transfer);
    NODE_SET_METHOD(exports, "restore_transfer", restore_transfer);
    NODE_SET_METHOD(exports, "remove_transfer", remove_transfer);
    NODE_SET_METHOD(exports, "load_node_dat", load_node_dat);
    NODE_SET_METHOD(exports, "load_server_met", load_server_met);
    NODE_SET_METHOD(exports, "parse_hash", parse_hash);
    NODE_SET_METHOD(exports, "emulex_env", emulex_env);
}

NODE_MODULE(emulex, init);
///
}  // namespace n
}  // namespace emulex
