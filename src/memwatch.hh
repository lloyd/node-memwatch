/*
 * 2012|lloyd|http://wtfpl.org
 */

#ifndef __MEMWATCH_HH
#define __MEMWATCH_HH

#include <node.h>

namespace memwatch
{
    v8::Handle<v8::Value> upon_gc(const v8::Arguments& args);
    v8::Handle<v8::Value> trigger_gc(const v8::Arguments& args);
    void after_gc(v8::GCType type, v8::GCCallbackFlags flags);
};

#endif
