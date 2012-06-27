const
memwatch = require('..'),
url = require('url');

var hd = new memwatch.HeapDiff();

function LeakingClass() {
}

var arr = [];
for (var i = 0; i < 100; i++) {
  arr.push(new LeakingClass);
  arr.push((new LeakingClass).toString() + i);
}

console.log(JSON.stringify(hd.end(), null, 4));
