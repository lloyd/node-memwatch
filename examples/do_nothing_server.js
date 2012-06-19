// a trivial process that does nothing except
// trigger GC and output the present base memory
// usage every second.  this example is intended to
// demonstrate that gcstats itself does not leak.

var gcstats = require('../');

gcstats.on('gc', function(d) {
  if (d.compacted) {
    console.log('current base memory usage:', gcstats.stats().current_base);
  }
});

setInterval(function() {
  gcstats.gc();
}, 1000);
