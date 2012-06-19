/*
 * 2012|lloyd|do what the fuck you want to
 */

#include <node.h>
#include <node_version.h>

#include <string>
#include <cstring>

using namespace v8;
using namespace node;

Handle<Object> g_context;
Handle<Function> g_cb;

struct Baton {
    uv_work_t req;
    size_t heapUsage;
    GCType type;
    GCCallbackFlags flags;
};

static void AsyncGCStatsAfter(uv_work_t* request) {
    HandleScope scope;

    Baton * b = (Baton *) request->data;

    if (!g_cb.IsEmpty()) {
        Handle<Value> argv[3];
        argv[0] = String::New(b->type == kGCTypeMarkSweepCompact ? "full" : "incremental");
        argv[1] = Boolean::New(b->flags == kGCCallbackFlagCompacted);
        argv[2] = Number::New(b->heapUsage);
        g_cb->Call(g_context, 3, argv);
    }

    delete b;

    scope.Close(Undefined());
}

static void after_gc(GCType type, GCCallbackFlags flags)
{
    HandleScope scope;

    Baton * baton = new Baton;
    v8::HeapStatistics hs;

    v8::V8::GetHeapStatistics(&hs);

    baton->heapUsage = hs.used_heap_size();
    baton->type = type;
    baton->flags = flags;
    baton->req.data = (void *) baton;
    
    uv_queue_work(uv_default_loop(), &(baton->req), NULL, AsyncGCStatsAfter);

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
    return scope.Close(Undefined());
}

extern "C" void init(Handle<Object> target) {
    HandleScope scope;
    NODE_SET_METHOD(target, "upon_gc", upon_gc);
    NODE_SET_METHOD(target, "gc", trigger_gc);
    
    V8::AddGCEpilogueCallback(after_gc);
};
