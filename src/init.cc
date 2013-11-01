/*
 * 2012|lloyd|do what the fuck you want to
 */

#include <v8.h>
#include <node.h>

#include "heapdiff.hh"
#include "memwatch.hh"

using namespace v8;

void init (Handle<Object> target)
{
    HandleScope scope;
    heapdiff::HeapDiff::Initialize(target);

    NODE_SET_METHOD(target, "upon_gc", memwatch::upon_gc);
    NODE_SET_METHOD(target, "gc", memwatch::trigger_gc);

    V8::AddGCEpilogueCallback(memwatch::after_gc);
}

NODE_MODULE(memwatch, init);
