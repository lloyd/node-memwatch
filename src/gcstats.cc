/*
 * 2012 - lloyd - do what the fuck you want to
 */


#include <node.h>
#include <node_version.h>

#include <string>
#include <cstring>

using namespace v8;
using namespace node;

Handle<Value> meh(const Arguments& args) {
    HandleScope scope;
    return Integer::New(7);
}

extern "C" void init(Handle<Object> target) {
    HandleScope scope;
    NODE_SET_METHOD(target, "meh", meh);
};
