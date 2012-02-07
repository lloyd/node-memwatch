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

  this._ignore_first_three = 0;

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

GCStats.prototype._on_gc = function(type, compacted) {
  if (type === 'full') this._gc_full++;
  else this._gc_incremental++;
  if (compacted && this._ignore_first_three >= 3) {
    // after compactions we'll do fancy calculations to try to determine
    // 'base memory usage' and general memory usage trends
    
    // "recent" memory usage decays faster when the process is younger to,
    // make memory trending visible earlier
    var curUsage = process.memoryUsage().heapUsed;    
    this._last_base = curUsage;
    var decay = ((this._gc_compaction < RECENT_PERIOD) ?
                 this._gc_compaction * .5 : RECENT_PERIOD);
    this._base_recent = ((this._base_recent * decay) + curUsage) / (decay + 1);

    decay = ((this._gc_compaction < ANCIENT_PERIOD) ? this._gc_compaction : ANCIENT_PERIOD);
    this._base_ancient = ((this._base_ancient * decay) + curUsage) / (decay + 1);    

    if (!this._gc_compaction || this._base_min.bytes > curUsage) {
      this._base_min = {
        bytes: curUsage,
        when: new Date()
      };
    }

    if (!this._gc_compaction || this._base_max.bytes < curUsage) {
      this._base_max = {
        bytes: curUsage,
        when: new Date()
      };
    }

    this._gc_compaction++;
  } else if (compacted) {
    this._ignore_first_three++;
  }
    
  this.emit((type === 'full') ? 'gc' : 'gc_incremental', { compacted: compacted });  
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

module.exports = new GCStats();

magic.upon_gc(function(type, compacted) {
  module.exports._on_gc(type, compacted);
});
