//
//  plugins.hpp
//  emulex
//
//  Created by Centny on 2/5/17.
//
//

#ifndef plugins_hpp
#define plugins_hpp

#include <node.h>
#include <node_buffer.h>
#include <stdio.h>
#include <v8.h>
#include <uv.h>
#include <string>
#include "common.h"

namespace emulex {
namespace n {

using namespace node;
using namespace v8;
//
void Initialize(Local<Object> exports);
void NodeRegister();
//
}  // namespace n
}  // namespace emulex
#endif /* plugins_hpp */
