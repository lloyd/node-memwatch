const
magic = require('./build/Release/gcstats'),
url = require('url');

var hd = new magic.HeapDiff();

function LeakingClass() {
}

var arr = [];
for (var i = 0; i < 100; i++) {
  arr.push(new LeakingClass);
}

var hd = hd.end();
console.log(JSON.stringify(hd, null, 4));

hd = null;

magic.gc(); 
console.log("all gc'd");
