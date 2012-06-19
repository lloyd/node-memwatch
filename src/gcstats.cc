/*
 * 2012|lloyd|do what the fuck you want to
 */

#include <node.h>
#include <node_version.h>
#include <v8-profiler.h>

#include <string>
#include <cstring>
#include <iostream>

using namespace v8;
using namespace node;

Handle<Object> g_context;
Handle<Function> g_cb;

static void after_gc(GCType type, GCCallbackFlags flags)
{
    // This is a gc epilog collback and we're calling back into javascript.
    // This must result in the allocation of ojbects, and can't really be safe.
    HandleScope scope;
    
    if (!g_cb.IsEmpty()) {
        Handle<Value> argv[2];
        argv[0] = String::New(type == kGCTypeMarkSweepCompact ? "full" : "incremental");
        argv[1] = Boolean::New(flags == kGCCallbackFlagCompacted);
        g_cb->Call(g_context, 2, argv);
    }
    scope.Close(Undefined());
}

Handle<Value> upon_gc(const Arguments& args) {
    HandleScope scope;
    if (args.Length() >= 1 && args[0]->IsFunction()) {
        g_cb = Persistent<Function>::New(Handle<Function>::Cast(args[0]));
        g_context = Persistent<Object>::New(Context::GetCalling()->Global());
    }
    return scope.Close(Undefined());
}

Handle<Value> trigger_gc(const Arguments& args) {
    HandleScope scope;
    while(!V8::IdleNotification()) {};
/*
    v8::HeapProfiler::DeleteAllSnapshots();
    const v8::HeapSnapshot * hs = v8::HeapProfiler::TakeSnapshot(v8::String::New(""));
    std::cout << "nodes in snapshot " << hs->GetNodesCount() << std::endl;
*/
    return scope.Close(Undefined());
}

/*
static void before_gc(GCType type, GCCallbackFlags flags)
{
    printf("before_gc\n");
}
*/

extern "C" void init(Handle<Object> target) {
    HandleScope scope;
    NODE_SET_METHOD(target, "upon_gc", upon_gc);
    NODE_SET_METHOD(target, "gc", trigger_gc);
    
//    V8::AddGCPrologueCallback(before_gc);
    V8::AddGCEpilogueCallback(after_gc);
};
