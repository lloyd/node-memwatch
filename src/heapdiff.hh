/*
 * 2012|lloyd|http://wtfpl.org
 */

#ifndef __HEADDIFF_H
#define __HEADDIFF_H

#include <v8.h>
#include <v8-profiler.h>
#include <node.h>

namespace heapdiff 
{
    class HeapDiff : public node::ObjectWrap
    {
      public:
        static void Initialize ( v8::Handle<v8::Object> target );

        static v8::Handle<v8::Value> New( const v8::Arguments& args );
        static v8::Handle<v8::Value> End( const v8::Arguments& args );
        static bool InProgress();

      protected:
        HeapDiff();
        ~HeapDiff();
      private:
        const v8::HeapSnapshot * before;
        const v8::HeapSnapshot * after;
        bool ended;
    };
};

#endif
