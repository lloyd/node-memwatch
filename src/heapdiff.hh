/*
 * 2012|lloyd|http://wtfpl.org
 */

#ifndef __HEADDIFF_H
#define __HEADDIFF_H

#include <node.h>

namespace heapdiff 
{
    class HeapDiff : public node::ObjectWrap
    {
      public:
        static void Initialize ( v8::Handle<v8::Object> target );

        static v8::Handle<v8::Value> New( const v8::Arguments& args );
        static v8::Handle<v8::Value> End( const v8::Arguments& args );

      protected:
        HeapDiff();
        ~HeapDiff();
      private:
    };
};

#endif
