const
magic = require('./build/Release/gcstats');

console.log(1);

var hd = new magic.HeapDiff();

var arr = [];
for (var i = 0; i < 100; i++) {
  var str = "another string " +  i.toString();
  arr.push(str);
}

console.log(JSON.stringify(hd.end(), null, 4));

hd = null;

magic.gc(); 
console.log("all gc'd");
