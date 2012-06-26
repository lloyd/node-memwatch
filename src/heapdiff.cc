/*
 * 2012|lloyd|http://wtfpl.org
 */

#include <node.h>

#include "heapdiff.hh"

using namespace v8;
using namespace node;

void
heapdiff::HeapDiff::Initialize ( v8::Handle<v8::Object> target )
{
    v8::HandleScope scope;
    
    v8::Local<v8::FunctionTemplate> t = v8::FunctionTemplate::New(New);
//    t->InstanceTemplate()->SetInternalFieldCount(1);
    t->SetClassName(String::NewSymbol("HeapDiff"));

    NODE_SET_PROTOTYPE_METHOD(t, "end", End);

    target->Set(v8::String::NewSymbol( "HeapDiff"), t->GetFunction());
}

v8::Handle<v8::Value>
heapdiff::HeapDiff::New (const v8::Arguments& args)
{
    v8::HandleScope scope;

    // XXX: take a snapshot and save it

    return args.This();
}

v8::Handle<Value>
heapdiff::HeapDiff::End( const Arguments& args )
{
    v8::HandleScope scope;

    HeapDiff *t = Unwrap<HeapDiff>( args.This() );

    // XXX: take another snapshot and compare them

    return scope.Close(Null());
}
