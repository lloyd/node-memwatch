const
magic = require('./build/Release/gcstats'),
events = require('events'),
util = require('util');

const RECENT_PERIOD = 10;
const ANCIENT_PERIOD = 120;

function GCStats() {
  events.EventEmitter.call(this);

  // the number of GC events of various flavor
  this._gc_full = 0;
  this._gc_incremental = 0;
  this._gc_compaction = 0;

  this._last_base = 0;

  // the estimated "base memory" usage of the javascript heap
  // over the RECENT_PERIOD number of GC runs
  this._base_recent = 0;

  // the estimated "base memory" usage of the javascript heap
  // over the ANCIENT_PERIOD number of GC runs
  this._base_ancient = 0;

  this._base_max = undefined;
  this._base_min = undefined;
};

util.inherits(GCStats, events.EventEmitter);

function min(a, b) {
  return a < b ? a : b;
}


GCStats.prototype._on_gc = function(type, compacted) {
  if (type === 'full') this._gc_full++;
  else this._gc_incremental++;
  if (compacted) {
    this._last_base = process.memoryUsage().heapUsed;

    // the first ten compactions we'll use a different algorithm to
    // dampen out wider memory fluctuation at startup
    if (this._gc_compaction < RECENT_PERIOD) {
      var decay = Math.pow(this._gc_compaction / RECENT_PERIOD, 2.5);
      decay *= this._gc_compaction;
      if (decay == Infinity) decay = 0;
      this._base_recent = ((this._base_recent * decay) +
                           this._last_base) / (decay + 1);
      decay = Math.pow(this._gc_compaction / RECENT_PERIOD, 2.4);
      decay *= this._gc_compaction;
      this._base_ancient = ((this._base_ancient * decay) +
                            this._last_base) /  (1 + decay);
    } else {
      this._base_recent = ((this._base_recent * (RECENT_PERIOD - 1)) +
                           this._last_base) / RECENT_PERIOD;
      var decay = min(ANCIENT_PERIOD, this._gc_compaction);
      this._base_ancient = ((this._base_ancient * (decay - 1)) +
                           this._last_base) / decay;
    }

    // only record min/max after 3 gcs to let initial instability settle
    if (this._gc_compaction >= 3) {
      if (!this._base_min || this._base_min.bytes > this._last_base) {
        this._base_min = {
          bytes: this._last_base,
          when: new Date()
        };
      }

      if (!this._base_max || this._base_max.bytes < this._last_base) {
        this._base_max = {
          bytes: this._last_base,
          when: new Date()
        };
      }
    }
    this._gc_compaction++;
  }

  var self = this;
  process.nextTick(function() {
    self.emit((type === 'full') ? 'gc' : 'gc_incremental', { compacted: compacted });  
  });
};

GCStats.prototype.stats = function() {
  return {
    num_full_gc: this._gc_full,
    num_inc_gc: this._gc_incremental,
    heap_compactions: this._gc_compaction,
    usage_trend: (this._base_ancient ? ((this._base_recent - this._base_ancient) / this._base_ancient) : 0).toFixed(3) * 100.0,
    estimated_base: (this._base_recent ? this._base_recent : 0).toFixed(0),
    current_base: this._last_base,
    min: this._base_min,
    max: this._base_max
  };
};

GCStats.prototype.gc = function() {
  var t = new Date();
  magic.gc();
  t = new Date() - t;
  console.log(t, "ms");
};

module.exports = new GCStats();

magic.upon_gc(function(type, compacted) {
  module.exports._on_gc(type, compacted);
});
