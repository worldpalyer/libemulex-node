//
//  common.h
//  binding
//
//  Created by Centny on 3/22/17.
//
//

#ifndef common_h
#define common_h

//#define LIBED2K_DEBUG
#define LIBED2K_USE_BOOST_DATE_TIME 1
#include <emulex/loader.hpp>

#define StringException(isolate, m) isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, m)))

#endif /* common_h */
