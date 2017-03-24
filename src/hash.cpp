//
//  hash.cpp
//  binding
//
//  Created by Centny on 3/22/17.
//
//

#include "hash.hpp"
#include <node.h>

namespace emulex {
namespace n {

struct parse_hash_uv {
    uv_work_t request;
    std::string path;
    bool bmd4, bmd5, bsha1;
    file_hash hash;
    std::string err;
    Isolate* isolate;
    Persistent<Function> callback;
};

static void parse_hash_async(uv_work_t* req) {
    parse_hash_uv* uv = static_cast<parse_hash_uv*>(req->data);
    try {
        uv->hash = read_file_hash(uv->path, uv->bmd4, uv->bmd5, uv->bsha1);
    } catch (...) {
        uv->err = "parse hash for " + uv->path + " fail";
    }
}

static void parse_hash_async_after(uv_work_t* req, int status) {
    parse_hash_uv* uv = static_cast<parse_hash_uv*>(req->data);
    HandleScope hs(uv->isolate);
    Local<Value> argv[2];
    argv[0] = String::NewFromUtf8(uv->isolate, uv->err.c_str());
    Local<Object> hash = Object::New(uv->isolate);
    if (uv->err.length() < 1) {
        hash->Set(String::NewFromUtf8(uv->isolate, "path"), String::NewFromUtf8(uv->isolate, uv->path.c_str()));
        hash->Set(String::NewFromUtf8(uv->isolate, "size"), Number::New(uv->isolate, uv->hash.size));
        hash->Set(String::NewFromUtf8(uv->isolate, "md4"), String::NewFromUtf8(uv->isolate, uv->hash.md4.c_str()));
        hash->Set(String::NewFromUtf8(uv->isolate, "md5"), String::NewFromUtf8(uv->isolate, uv->hash.md5.c_str()));
        hash->Set(String::NewFromUtf8(uv->isolate, "sha1"), String::NewFromUtf8(uv->isolate, uv->hash.sha1.c_str()));
        Local<Array> parts = Array::New(uv->isolate, uv->hash.parts.size());
        for (size_t i = 0; i < uv->hash.parts.size(); i++) {
            parts->Set(i, String::NewFromUtf8(uv->isolate, uv->hash.parts[i].c_str()));
        }
        hash->Set(String::NewFromUtf8(uv->isolate, "parts"), parts);
    }
    argv[1] = hash;
    Local<Function>::New(uv->isolate, uv->callback)->Call(uv->isolate->GetCurrentContext()->Global(), 2, argv);
    delete uv;
}

//
void parse_hash(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();
    if (!(args.Length() > 1 && args[0]->IsObject() && args[1]->IsFunction())) {
        StringException(isolate, "emulex parse hash receive wrong arguments");
        return;
    }
    Local<Object> vargs = args[0]->ToObject();
    parse_hash_uv* uv = new parse_hash_uv;
    //
    Local<Value> path_v = vargs->Get(String::NewFromUtf8(isolate, "path"));
    if (path_v->IsString()) {
        String::Utf8Value path_s(path_v->ToString());
        if (path_s.length()) {
            uv->path = std::string(*path_s, path_s.length());
        } else {
            StringException(isolate, "emulex ed2k parse hash fail with args.path is empty");
            return;
        }
    } else {
        StringException(isolate, "emulex ed2k parse hash fail with args.path is not string");
        return;
    }
    //
    Local<Value> bmd4_v = vargs->Get(String::NewFromUtf8(isolate, "bmd4"));
    uv->bmd4 = bmd4_v->IsBoolean() && bmd4_v->BooleanValue();
    //
    Local<Value> bmd5_v = vargs->Get(String::NewFromUtf8(isolate, "bmd5"));
    uv->bmd5 = bmd5_v->IsBoolean() && bmd4_v->BooleanValue();
    //
    Local<Value> bsha1_v = vargs->Get(String::NewFromUtf8(isolate, "bsha1"));
    uv->bsha1 = bsha1_v->IsBoolean() && bsha1_v->BooleanValue();
    DBG("emulex: parse hash by path:" << uv->path << ",bmd4:" << uv->bmd4 << ",bmd5:" << uv->bmd5
                                      << ",bsha1:" << uv->bsha1);
    //
    Local<Function> cback = Local<Function>::Cast(args[1]);
    uv->isolate = isolate;
    uv->callback.Reset(isolate, cback);
    //
    uv->request.data = uv;
    uv_queue_work(uv_default_loop(), &uv->request, parse_hash_async, parse_hash_async_after);
}
//
}
}
