/*
 * 2012|lloyd|http://wtfpl.org
 */
#include <map>
#include <string>
#include <set>
#include <vector>

#include <stdlib.h> // abs()
#include <time.h>   // time()

#include "heapdiff.hh"
#include "util.hh"

using namespace v8;
using namespace node;
using namespace std;

static bool s_inProgress = false;
static time_t s_startTime;

bool heapdiff::HeapDiff::InProgress()
{
    return s_inProgress;
}

heapdiff::HeapDiff::HeapDiff() : ObjectWrap(), before(NULL), after(NULL),
                                 ended(false)
{
}

heapdiff::HeapDiff::~HeapDiff()
{
    if (before) {
        ((HeapSnapshot *) before)->Delete();
        before = NULL;
    }

    if (after) {
        ((HeapSnapshot *) after)->Delete();
        after = NULL;
    }
}

void
heapdiff::HeapDiff::Initialize ( v8::Handle<v8::Object> target )
{
    NanScope();

    v8::Local<v8::FunctionTemplate> t = NanNew<v8::FunctionTemplate>(New);
    t->InstanceTemplate()->SetInternalFieldCount(1);
    t->SetClassName(NanNew("HeapDiff"));

    NODE_SET_PROTOTYPE_METHOD(t, "end", End);

    target->Set(NanNew( "HeapDiff"), t->GetFunction());
}

NAN_METHOD(heapdiff::HeapDiff::New)
{
    // Don't blow up when the caller says "new require('memwatch').HeapDiff()"
    // issue #30
    // stolen from: https://github.com/kkaefer/node-cpp-modules/commit/bd9432026affafd8450ecfd9b49b7dc647b6d348
    if (!args.IsConstructCall()) {
        return NanThrowTypeError("Use the new operator to create instances of this object.");
    }

    NanScope();

    // allocate the underlying c++ class and wrap it up in the this pointer
    HeapDiff * self = new HeapDiff();
    self->Wrap(args.This());

    // take a snapshot and save a pointer to it
    s_inProgress = true;
    s_startTime = time(NULL);

#if (NODE_MODULE_VERSION > 0x000B)
    self->before = v8::Isolate::GetCurrent()->GetHeapProfiler()->TakeHeapSnapshot(NanNew<v8::String>(""), NULL);
#else
    self->before = v8::HeapProfiler::TakeSnapshot(NanNew<v8::String>(""), HeapSnapshot::kFull, NULL);
#endif

    s_inProgress = false;

    NanReturnValue(args.This());
}

static string handleToStr(const Handle<Value> & str)
{
	String::Utf8Value utfString(str->ToString());
	return *utfString;
}

static void
buildIDSet(set<uint64_t> * seen, const HeapGraphNode* cur, int & s)
{
    NanScope();

    // cycle detection
    if (seen->find(cur->GetId()) != seen->end()) {
        return;
    }
    // always ignore HeapDiff related memory
    if (cur->GetType() == HeapGraphNode::kObject &&
        handleToStr(cur->GetName()).compare("HeapDiff") == 0)
    {
        return;
    }

    // update memory usage as we go
    s += cur->GetSelfSize();

    seen->insert(cur->GetId());

    for (int i=0; i < cur->GetChildrenCount(); i++) {
        buildIDSet(seen, cur->GetChild(i)->GetToNode(), s);
    }
}

typedef set<uint64_t> idset;

// why doesn't STL work?
// XXX: improve this algorithm
void setDiff(idset a, idset b, vector<uint64_t> &c)
{
    for (idset::iterator i = a.begin(); i != a.end(); i++) {
        if (b.find(*i) == b.end()) c.push_back(*i);
    }
}


class example
{
public:
    HeapGraphEdge::Type context;
    HeapGraphNode::Type type;
    std::string name;
    std::string value;
    std::string heap_value;
    int self_size;
    int retained_size;
    int retainers;

    example() : context(HeapGraphEdge::kHidden),
                type(HeapGraphNode::kHidden),
                self_size(0), retained_size(0), retainers(0) { };
};

class change
{
public:
    long int size;
    long int added;
    long int released;
    std::vector<example> examples;

    change() : size(0), added(0), released(0) { }
};

typedef std::map<std::string, change>changeset;

static void manageChange(changeset & changes, const HeapGraphNode * node, bool added)
{
    std::string type;

    switch(node->GetType()) {
        case HeapGraphNode::kArray:
            type.append("Array");
            break;
        case HeapGraphNode::kString:
            type.append("String");
            break;
        case HeapGraphNode::kObject:
            type.append(handleToStr(node->GetName()));
            break;
        case HeapGraphNode::kCode:
            type.append("Code");
            break;
        case HeapGraphNode::kClosure:
            type.append("Closure");
            break;
        case HeapGraphNode::kRegExp:
            type.append("RegExp");
            break;
        case HeapGraphNode::kHeapNumber:
            type.append("Number");
            break;
        case HeapGraphNode::kNative:
            type.append("Native");
            break;
        case HeapGraphNode::kHidden:
        default:
            return;
    }

    if (changes.find(type) == changes.end()) {
        changes[type] = change();
    }

    changeset::iterator i = changes.find(type);

    i->second.size += node->GetSelfSize() * (added ? 1 : -1);
    if (added) i->second.added++;
    else i->second.released++;

    // XXX: example

    return;
}

static Handle<Value> changesetToObject(changeset & changes)
{
    NanEscapableScope();
    Local<Array> a = NanNew<v8::Array>();

    for (changeset::iterator i = changes.begin(); i != changes.end(); i++) {
        Local<Object> d = NanNew<v8::Object>();
        d->Set(NanNew("what"), NanNew(i->first.c_str()));
        d->Set(NanNew("size_bytes"), NanNew<v8::Number>(i->second.size));
        d->Set(NanNew("size"), NanNew(mw_util::niceSize(i->second.size).c_str()));
        d->Set(NanNew("+"), NanNew<v8::Number>(i->second.added));
        d->Set(NanNew("-"), NanNew<v8::Number>(i->second.released));
        a->Set(a->Length(), d);
    }

    return NanEscapeScope(a);
}


static v8::Handle<Value>
compare(const v8::HeapSnapshot * before, const v8::HeapSnapshot * after)
{
    NanEscapableScope();
    int s, diffBytes;

    Local<Object> o = NanNew<v8::Object>();

    // first let's append summary information
    Local<Object> b = NanNew<v8::Object>();
    b->Set(NanNew("nodes"), NanNew(before->GetNodesCount()));
    //b->Set(NanNew("time"), s_startTime);
    o->Set(NanNew("before"), b);

    Local<Object> a = NanNew<v8::Object>();
    a->Set(NanNew("nodes"), NanNew(after->GetNodesCount()));
    //a->Set(NanNew("time"), time(NULL));
    o->Set(NanNew("after"), a);

    // now let's get allocations by name
    set<uint64_t> beforeIDs, afterIDs;
    s = 0;
    buildIDSet(&beforeIDs, before->GetRoot(), s);
    b->Set(NanNew("size_bytes"), NanNew(s));
    b->Set(NanNew("size"), NanNew(mw_util::niceSize(s).c_str()));

    diffBytes = s;
    s = 0;
    buildIDSet(&afterIDs, after->GetRoot(), s);
    a->Set(NanNew("size_bytes"), NanNew(s));
    a->Set(NanNew("size"), NanNew(mw_util::niceSize(s).c_str()));

    diffBytes = s - diffBytes;

    Local<Object> c = NanNew<v8::Object>();
    c->Set(NanNew("size_bytes"), NanNew(diffBytes));
    c->Set(NanNew("size"), NanNew(mw_util::niceSize(diffBytes).c_str()));
    o->Set(NanNew("change"), c);

    // before - after will reveal nodes released (memory freed)
    vector<uint64_t> changedIDs;
    setDiff(beforeIDs, afterIDs, changedIDs);
    c->Set(NanNew("freed_nodes"), NanNew<v8::Number>(changedIDs.size()));

    // here's where we'll collect all the summary information
    changeset changes;

    // for each of these nodes, let's aggregate the change information
    for (unsigned long i = 0; i < changedIDs.size(); i++) {
        const HeapGraphNode * n = before->GetNodeById(changedIDs[i]);
        manageChange(changes, n, false);
    }

    changedIDs.clear();

    // after - before will reveal nodes added (memory allocated)
    setDiff(afterIDs, beforeIDs, changedIDs);

    c->Set(NanNew("allocated_nodes"), NanNew<v8::Number>(changedIDs.size()));

    for (unsigned long i = 0; i < changedIDs.size(); i++) {
        const HeapGraphNode * n = after->GetNodeById(changedIDs[i]);
        manageChange(changes, n, true);
    }

    c->Set(NanNew("details"), changesetToObject(changes));

    return NanEscapeScope(o);
}

NAN_METHOD(heapdiff::HeapDiff::End)
{
    // take another snapshot and compare them
    NanScope();

    HeapDiff *t = Unwrap<HeapDiff>( args.This() );

    // How shall we deal with double .end()ing?  The only reasonable
    // approach seems to be an exception, cause nothing else makes
    // sense.
    if (t->ended) {
        return NanThrowError("attempt to end() a HeapDiff that was already ended");
    }
    t->ended = true;

    s_inProgress = true;
#if (NODE_MODULE_VERSION > 0x000B)
    t->after = v8::Isolate::GetCurrent()->GetHeapProfiler()->TakeHeapSnapshot(NanNew<v8::String>(""), NULL);
#else
    t->after = v8::HeapProfiler::TakeSnapshot(NanNew<v8::String>(""), HeapSnapshot::kFull, NULL);
#endif
    s_inProgress = false;

    v8::Handle<Value> comparison = compare(t->before, t->after);
    // free early, free often.  I mean, after all, this process we're in is
    // probably having memory problems.  We want to help her.
    ((HeapSnapshot *) t->before)->Delete();
    t->before = NULL;
    ((HeapSnapshot *) t->after)->Delete();
    t->after = NULL;

    NanReturnValue(comparison);
}
