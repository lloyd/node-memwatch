/*
 * 2012|lloyd|http://wtfpl.org
 */

#include "heapdiff.hh"

#include <node.h>

#include <iostream>
#include <map>
#include <set>
#include <string>

using namespace v8;
using namespace node;
using namespace std;

heapdiff::HeapDiff::HeapDiff() : ObjectWrap(), before(NULL), after(NULL)
{
    cout << "constructor" << endl;
}

heapdiff::HeapDiff::~HeapDiff()
{
    cout << "destructor" << endl;
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
    v8::HandleScope scope;

    // allocate the underlying c++ class and wrap it up in the this pointer
    HeapDiff * self = new HeapDiff();
    self->Wrap(args.This());

    // take a snapshot and save a pointer to it
    self->before = v8::HeapProfiler::TakeSnapshot(v8::String::New(""));

    return args.This();
}

string handleToStr(const Handle<Value> & str) 
{
    Local<String> s = str->ToString();
    char buf[s->Utf8Length() + 1];
    s->WriteUtf8(buf);
    return std::string(buf);
}

static void
countAllocationsByName(map<string, int> * m, set<uint64_t> * seen, const HeapGraphNode* cur) 
{
    v8::HandleScope scope;

    // cycle detection
    if (seen->find(cur->GetId()) != seen->end()) {
        return;
    }
    seen->insert(cur->GetId());
    
    for (int i=0; i < cur->GetChildrenCount(); i++) {
        // XXX must do more careful inspection of edges
        countAllocationsByName(m, seen, cur->GetChild(i)->GetToNode());
    }
    
    if (cur->GetType() == HeapGraphNode::kHidden) {
        // skip hidden nodes.
        // cout << "hidden" << endl;
    } else {
        map<string,int>::iterator it;
        std::string name = handleToStr(cur->GetName());
        it = m->find(name);
        if (it != m->end()) {
            (*it).second++;
        } else {
            (*m)[name] = 1;
        }
    }
}

static v8::Handle<Value>
compare(const v8::HeapSnapshot * before, const v8::HeapSnapshot * after)
{
    v8::HandleScope scope;

    Local<Object> o = Object::New();

    // first let's append summary information
    o->Set(String::New("nodes_before"), Integer::New(before->GetNodesCount()));
    o->Set(String::New("nodes_after"), Integer::New(after->GetNodesCount()));
    o->Set(String::New("nodes_change"), Integer::New(after->GetNodesCount() - before->GetNodesCount()));

    // now let's get allocations by name
    map<string, int> bAllocByName, aAllocByName;
    set<uint64_t> seen;
    countAllocationsByName(&bAllocByName, &seen, before->GetRoot());
    seen.clear();
    countAllocationsByName(&aAllocByName, &seen, after->GetRoot());

    o->Set(String::New("nodes_manual_count"), Integer::New(seen.size()));    

    // interate and print
    Local<Object> byName = Object::New();
    for (map<string,int>::iterator it = aAllocByName.begin();
         it != aAllocByName.end(); it++)
    {
        byName->Set(String::New(it->first.c_str()), Integer::New(it->second)
);        
    }
    o->Set(String::New("by_name"), byName);

    return scope.Close(o);
}

v8::Handle<Value>
heapdiff::HeapDiff::End( const Arguments& args )
{
    // take another snapshot and compare them
    const v8::HeapSnapshot * after = v8::HeapProfiler::TakeSnapshot(v8::String::New(""));

    v8::HandleScope scope;

    HeapDiff *t = Unwrap<HeapDiff>( args.This() );

    
    return scope.Close(compare(t->before, after));
}


