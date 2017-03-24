//
//  hash.hpp
//  binding
//
//  Created by Centny on 3/22/17.
//
//

#ifndef hash_hpp
#define hash_hpp
#include <string>
#include <vector>
#include <fstream>
#include <node.h>
#include <stdio.h>
#include <v8.h>
#include <uv.h>
#include <string>
#include "common.h"
#include <emulex/hash.hpp>

namespace emulex {
namespace n {
using namespace node;
using namespace v8;

void parse_hash(const FunctionCallbackInfo<Value>& args);
//
}
}
#endif /* hash_hpp */
