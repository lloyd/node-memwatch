`node-memwatch`: Leak Detection and Heap Diffing for Node.JS
============================================================

`node-memwatch` is here to help you detect and find memory leaks in
Node.JS code.

- A `leak` event is emitted, with an explanation, when it appears your
  code is leaking memory.

- A `stats` event is emitted when V8 performs a garbage collection,
  with data describing your heap usage and trends over time.

- A `HeapDiff` class lets you track all heap allocations over time.


Installation
------------

- `npm install heapdiff`

or

- `git clone git://github.com/lloyd/node-memwatch.git`


Description
-----------

There is a growing number of tools for debugging and profiling memory
usage in Node.JS applications, but there is still a need for a
platform-independent native module that requires no special
instrumentation.  This module attempts to supply that need.

Let's assume you import `node-memwatch` like so:

```javascript
var memwatch = require('memwatch');
```


### Leak Detection

You can then subscribe to `leak` events.  A `leak` event will be
emitted when your heap usage has increased for five consecutive
garbage collections:

```javascript
memwatch.on('leak', function(info) { ... });
```

The `info` object will look something like:

```javascript
{ start: Fri, 29 Jun 2012 14:12:13 GMT,
  end: Fri, 29 Jun 2012 14:12:33 GMT,
  growth: 67984,
  reason: 'heap growth over 5 consecutive GCs (20s) - 11.67 mb/hr' }
```


### Heap Usage

The best way to evaluate your memory footprint is to do so right after
V8 performs garbage collection.  `memwatch` can take a memory snapshot
after GC has occurred, and before Node has a chance to allocate any
new objects, giving you a clear view of your actual memory usage.

When V8 performs a garbage collection (for the picky, we're talking
about a full GC with heap compaction), `memwatch` will emit a `stats`
event.

```javascript
memwatch.on('stats', function(stats) { ... });
```

The `stats` data will look something like this:

```javascript
{
  "num_full_gc": 17,
  "num_inc_gc": 8,
  "heap_compactions": 8,
  "estimated_base": 2592568,
  "current_base": 2592568,
  "min": 2499912,
  "max": 2592568,
  "usage_trend": 0
}
```

`estimated_base` and `usage_trend` are tracked over time.  If usage
trend is consistently positive, it indicates that your base heap size
is continuously growing and you might have a leak.

V8 has its own idea of when it's best to perform a GC, and under a
heavy load, it may defer this action for some time.  To aid in
speedier debugging, `memwatch` provides a `gc()` method to force V8 to
do a full GC and heap compaction.


### Heap Diffing

So far we have seen how `memwatch` can aid in leak detection.  For
leak location, it provides a `HeapDiff` class that takes two snapshots
and computes a diff between them.  For example:

```javascript
// Take first snapshot
var hd = new memwatch.HeapDiff();

// do some things ...

// Take the second snapshot and compute the diff
var diff = hd.end();
```

The contents of `diff` will look something like:

```javascript
{
  "before": { "nodes": 11625, "size_bytes": 1869904, "size": "1.78 mb" },
  "after":  { "nodes": 21435, "size_bytes": 2119136, "size": "2.02 mb" },
  "change": { "size_bytes": 249232, "size": "243.39 kb", "freed_nodes": 197,
    "allocated_nodes": 10007,
    "details": [
      { "what": "String",
        "size_bytes": -2120,  "size": "-2.07 kb",  "+": 3,    "-": 62
      },
      { "what": "Array",
        "size_bytes": 66687,  "size": "65.13 kb",  "+": 4,    "-": 78
      },
      { "what": "LeakingClass",
        "size_bytes": 239952, "size": "234.33 kb", "+": 9998, "-": 0
      }
    ]
  }
```

The diff shows that during the sample period, `String` and `Array`
allocations decreased, but `Leaking Class` grew by 9998 allocations.
Hmm.

You can use `HeapDiff` in your `on('stats')` callback; even though it
takes a memory snapshot, which triggers a V8 GC, it will not trigger
the `stats` event itself.  Because that would be silly.


Future Work
-----------

Please see the Issues to share suggestions and contribute!

