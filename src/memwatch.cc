/*
 * 2012|lloyd|http://wtfpl.org
 */

#include "platformcompat.hh"
#include "memwatch.hh"
#include "heapdiff.hh"
#include "util.hh"

#include <node.h>
#include <node_version.h>

#include <string>
#include <cstring>
#include <sstream>

#include <math.h> // for pow
#include <time.h> // for time

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

static const unsigned int RECENT_PERIOD = 10;
static const unsigned int ANCIENT_PERIOD = 120;

static struct
{
    // counts of different types of gc events
    unsigned int gc_full;
    unsigned int gc_inc;
    unsigned int gc_compact;

    // last base heap size as measured *right* after GC
    unsigned int last_base;

    // the estimated "base memory" usage of the javascript heap
    // over the RECENT_PERIOD number of GC runs
    unsigned int base_recent;

    // the estimated "base memory" usage of the javascript heap
    // over the ANCIENT_PERIOD number of GC runs
    unsigned int base_ancient;

    // the most extreme values we've seen for base heap size
    unsigned int base_max;
    unsigned int base_min;

    // leak detection!

    // the period from which this leak analysis starts
    time_t leak_time_start;
    // the base memory for the detection period
    time_t leak_base_start;
    // the number of consecutive compactions for which we've grown
    unsigned int consecutive_growth;
} s_stats;

static Handle<Value> getLeakReport(size_t heapUsage)
{
    HandleScope scope;

    size_t growth = heapUsage - s_stats.leak_base_start;
    int now = time(NULL);
    int delta = now - s_stats.leak_time_start;

    Local<Object> leakReport = Object::New();
    leakReport->Set(String::New("start"), NODE_UNIXTIME_V8(s_stats.leak_time_start));
    leakReport->Set(String::New("end"), NODE_UNIXTIME_V8(now));
    leakReport->Set(String::New("growth"), Integer::New(growth));

    std::stringstream ss;
    ss << "heap growth over 5 consecutive GCs ("
       << mw_util::niceDelta(delta) << ") - "
       << mw_util::niceSize(growth / ((double) delta / (60.0 * 60.0))) << "/hr";

    leakReport->Set(String::New("reason"), String::New(ss.str().c_str()));

    return scope.Close(leakReport);
}

static void AsyncMemwatchAfter(uv_work_t* request) {
    HandleScope scope;

    Baton * b = (Baton *) request->data;

    // do the math in C++, permanent
    // record the type of GC event that occured
    if (b->type == kGCTypeMarkSweepCompact) s_stats.gc_full++;
    else s_stats.gc_inc++;

    if (
#if defined(NEW_COMPACTION_BEHAVIOR)
        b->type == kGCTypeMarkSweepCompact
#else
        b->flags == kGCCallbackFlagCompacted
#endif
        ) {
        // leak detection code.  has the heap usage grown?
        if (s_stats.last_base < b->heapUsage) {
            if (s_stats.consecutive_growth == 0) {
                s_stats.leak_time_start = time(NULL);
                s_stats.leak_base_start = b->heapUsage;
            }

            s_stats.consecutive_growth++;

            // consecutive growth over 5 GCs suggests a leak
            if (s_stats.consecutive_growth >= 5) {
                // reset to zero
                s_stats.consecutive_growth = 0;

                // emit a leak report!
                Handle<Value> argv[3];
                argv[0] = Boolean::New(false);
                // the type of event to emit
                argv[1] = String::New("leak");
                argv[2] = getLeakReport(b->heapUsage);
                g_cb->Call(g_context, 3, argv);
            }
        } else {
            s_stats.consecutive_growth = 0;
        }

        // update last_base
        s_stats.last_base = b->heapUsage;

        // update compaction count
        s_stats.gc_compact++;

        // the first ten compactions we'll use a different algorithm to
        // dampen out wider memory fluctuation at startup
        if (s_stats.gc_compact < RECENT_PERIOD) {
            double decay = pow(s_stats.gc_compact / RECENT_PERIOD, 2.5);
            decay *= s_stats.gc_compact;
            if (ISINF(decay) || ISNAN(decay)) decay = 0;
            s_stats.base_recent = ((s_stats.base_recent * decay) +
                                   s_stats.last_base) / (decay + 1);

            decay = pow(s_stats.gc_compact / RECENT_PERIOD, 2.4);
            decay *= s_stats.gc_compact;
            s_stats.base_ancient = ((s_stats.base_ancient * decay) +
                                    s_stats.last_base) /  (1 + decay);

        } else {
            s_stats.base_recent = ((s_stats.base_recent * (RECENT_PERIOD - 1)) +
                                   s_stats.last_base) / RECENT_PERIOD;
            double decay = FMIN(ANCIENT_PERIOD, s_stats.gc_compact);
            s_stats.base_ancient = ((s_stats.base_ancient * (decay - 1)) +
                                    s_stats.last_base) / decay;
        }

        // only record min/max after 3 gcs to let initial instability settle
        if (s_stats.gc_compact >= 3) {
            if (!s_stats.base_min || s_stats.base_min > s_stats.last_base) {
                s_stats.base_min = s_stats.last_base;
            }

            if (!s_stats.base_max || s_stats.base_max < s_stats.last_base) {
                s_stats.base_max = s_stats.last_base;
            }
        }

        // if there are any listeners, it's time to emit!
        if (!g_cb.IsEmpty()) {
            Handle<Value> argv[3];
            // magic argument to indicate to the callback all we want to know is whether there are
            // listeners (here we don't)
            argv[0] = Boolean::New(true);

            Handle<Value> haveListeners = g_cb->Call(g_context, 1, argv);

            if (haveListeners->BooleanValue()) {
                double ut= 0.0;
                if (s_stats.base_ancient) {
                    ut = (double) ROUND(((double) (s_stats.base_recent - s_stats.base_ancient) /
                                         (double) s_stats.base_ancient) * 1000.0) / 10.0;
                }

                // ok, there are listeners, we actually must serialize and emit this stats event
                Local<Object> stats = Object::New();
                stats->Set(String::New("num_full_gc"), Integer::New(s_stats.gc_full));
                stats->Set(String::New("num_inc_gc"), Integer::New(s_stats.gc_inc));
                stats->Set(String::New("heap_compactions"), Integer::New(s_stats.gc_compact));
                stats->Set(String::New("usage_trend"), Number::New(ut));
                stats->Set(String::New("estimated_base"), Integer::New(s_stats.base_recent));
                stats->Set(String::New("current_base"), Integer::New(s_stats.last_base));
                stats->Set(String::New("min"), Integer::New(s_stats.base_min));
                stats->Set(String::New("max"), Integer::New(s_stats.base_max));
                argv[0] = Boolean::New(false);
                // the type of event to emit
                argv[1] = String::New("stats");
                argv[2] = stats;
                g_cb->Call(g_context, 3, argv);
            }
        }
    }

    delete b;
}

void memwatch::after_gc(GCType type, GCCallbackFlags flags)
{
    if (heapdiff::HeapDiff::InProgress()) return;

    HandleScope scope;

    Baton * baton = new Baton;
    v8::HeapStatistics hs;

    v8::V8::GetHeapStatistics(&hs);

    baton->heapUsage = hs.used_heap_size();
    baton->type = type;
    baton->flags = flags;
    baton->req.data = (void *) baton;

    uv_queue_work(uv_default_loop(), &(baton->req), NULL, AsyncMemwatchAfter);

    scope.Close(Undefined());
}

Handle<Value> memwatch::upon_gc(const Arguments& args) {
    HandleScope scope;
    if (args.Length() >= 1 && args[0]->IsFunction()) {
        g_cb = Persistent<Function>::New(Handle<Function>::Cast(args[0]));
        g_context = Persistent<Object>::New(Context::GetCalling()->Global());
    }
    return scope.Close(Undefined());
}

Handle<Value> memwatch::trigger_gc(const Arguments& args) {
    HandleScope scope;
    while(!V8::IdleNotification()) {};
    return scope.Close(Undefined());
}
