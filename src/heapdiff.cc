/*
 * 2012|lloyd|http://wtfpl.org
 */

#include "heapdiff.hh"
#include "util.hh"

#include <node.h>

#include <map>
#include <string>
#include <set>
#include <vector>

#include <stdlib.h> // abs()
#include <time.h>   // time()

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
    v8::HandleScope scope;
    v8::Local<v8::FunctionTemplate> t = v8::FunctionTemplate::New(New);
    t->InstanceTemplate()->SetInternalFieldCount(1);
    t->SetClassName(String::NewSymbol("HeapDiff"));

    NODE_SET_PROTOTYPE_METHOD(t, "end", End);

    target->Set(v8::String::NewSymbol( "HeapDiff"), t->GetFunction());
}

v8::Handle<v8::Value>
heapdiff::HeapDiff::New (const v8::Arguments& args)
{
    // Don't blow up when the caller says "new require('memwatch').HeapDiff()"
    // issue #30
    // stolen from: https://github.com/kkaefer/node-cpp-modules/commit/bd9432026affafd8450ecfd9b49b7dc647b6d348
    if (!args.IsConstructCall()) {
        return ThrowException(
            Exception::TypeError(
                String::New("Use the new operator to create instances of this object.")));
    }

    v8::HandleScope scope;

    // allocate the underlying c++ class and wrap it up in the this pointer
    HeapDiff * self = new HeapDiff();
    self->Wrap(args.This());

    // take a snapshot and save a pointer to it
    s_inProgress = true;
    s_startTime = time(NULL);
    self->before = v8::HeapProfiler::TakeSnapshot(v8::String::New(""));
    s_inProgress = false;

    return args.This();
}

static string handleToStr(const Handle<Value> & str)
{
	String::Utf8Value utfString(str->ToString());
	return *utfString;   
}

static void
buildIDSet(set<uint64_t> * seen, const HeapGraphNode* cur, int & s)
{
    v8::HandleScope scope;

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
    v8::HandleScope scope;
    Local<Array> a = Array::New();

    for (changeset::iterator i = changes.begin(); i != changes.end(); i++) {
        Local<Object> d = Object::New();
        d->Set(String::New("what"), String::New(i->first.c_str()));
        d->Set(String::New("size_bytes"), Integer::New(i->second.size));
        d->Set(String::New("size"), String::New(mw_util::niceSize(i->second.size).c_str()));
        d->Set(String::New("+"), Integer::New(i->second.added));
        d->Set(String::New("-"), Integer::New(i->second.released));
        a->Set(a->Length(), d);
    }

    return scope.Close(a);
}


static v8::Handle<Value>
compare(const v8::HeapSnapshot * before, const v8::HeapSnapshot * after)
{
    v8::HandleScope scope;
    int s, diffBytes;

    Local<Object> o = Object::New();

    // first let's append summary information
    Local<Object> b = Object::New();
    b->Set(String::New("nodes"), Integer::New(before->GetNodesCount()));
    b->Set(String::New("time"), NODE_UNIXTIME_V8(s_startTime));
    o->Set(String::New("before"), b);

    Local<Object> a = Object::New();
    a->Set(String::New("nodes"), Integer::New(after->GetNodesCount()));
    a->Set(String::New("time"), NODE_UNIXTIME_V8(time(NULL)));
    o->Set(String::New("after"), a);

    // now let's get allocations by name
    set<uint64_t> beforeIDs, afterIDs;
    s = 0;
    buildIDSet(&beforeIDs, before->GetRoot(), s);
    b->Set(String::New("size_bytes"), Integer::New(s));
    b->Set(String::New("size"), String::New(mw_util::niceSize(s).c_str()));

    diffBytes = s;
    s = 0;
    buildIDSet(&afterIDs, after->GetRoot(), s);
    a->Set(String::New("size_bytes"), Integer::New(s));
    a->Set(String::New("size"), String::New(mw_util::niceSize(s).c_str()));

    diffBytes = s - diffBytes;

    Local<Object> c = Object::New();
    c->Set(String::New("size_bytes"), Integer::New(diffBytes));
    c->Set(String::New("size"), String::New(mw_util::niceSize(diffBytes).c_str()));
    o->Set(String::New("change"), c);

    // before - after will reveal nodes released (memory freed)
    vector<uint64_t> changedIDs;
    setDiff(beforeIDs, afterIDs, changedIDs);
    c->Set(String::New("freed_nodes"), Integer::New(changedIDs.size()));

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

    c->Set(String::New("allocated_nodes"), Integer::New(changedIDs.size()));

    for (unsigned long i = 0; i < changedIDs.size(); i++) {
        const HeapGraphNode * n = after->GetNodeById(changedIDs[i]);
        manageChange(changes, n, true);
    }

    c->Set(String::New("details"), changesetToObject(changes));

    return scope.Close(o);
}

v8::Handle<Value>
heapdiff::HeapDiff::End( const Arguments& args )
{
    // take another snapshot and compare them
    v8::HandleScope scope;

    HeapDiff *t = Unwrap<HeapDiff>( args.This() );

    // How shall we deal with double .end()ing?  The only reasonable
    // approach seems to be an exception, cause nothing else makes
    // sense.
    if (t->ended) {
        return v8::ThrowException(
            v8::Exception::Error(
                v8::String::New("attempt to end() a HeapDiff that was "
                                "already ended")));
    }
    t->ended = true;

    s_inProgress = true;
    t->after = v8::HeapProfiler::TakeSnapshot(v8::String::New(""));
    s_inProgress = false;

    v8::Handle<Value> comparison = compare(t->before, t->after);
    // free early, free often.  I mean, after all, this process we're in is
    // probably having memory problems.  We want to help her.
    ((HeapSnapshot *) t->before)->Delete();
    t->before = NULL;
    ((HeapSnapshot *) t->after)->Delete();
    t->after = NULL;

    return scope.Close(comparison);
}
