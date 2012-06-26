/*
 * 2012|lloyd|do what the fuck you want to
 */

#include <v8.h>
#include <node.h>

#include "heapdiff.hh"
#include "gcstats.hh"

extern "C" {
    void init (v8::Handle<v8::Object> target)
    {
        v8::HandleScope scope;
        heapdiff::HeapDiff::Initialize(target);

        NODE_SET_METHOD(target, "upon_gc", gcstats::upon_gc);
        NODE_SET_METHOD(target, "gc", gcstats::trigger_gc);

        v8::V8::AddGCEpilogueCallback(gcstats::after_gc);
    }

    NODE_MODULE(gcstats, init);
};
