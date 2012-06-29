const
magic = require('./build/Release/memwatch'),
events = require('events');

module.exports = new events.EventEmitter();

module.exports.gc = magic.gc;
module.exports.HeapDiff = magic.HeapDiff;

magic.upon_gc(function(has_listeners, event, data) {
  if (has_listeners) {
    return ((module.exports._events && module.exports._events.stats) ?
            module.exports._events.stats.length > 0 : false);
  } else {
    return module.exports.emit(event, data);
  }
});
