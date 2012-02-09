const
gcstats = require('./include.js'),
fs = require('fs');

var startTime = new Date();

var filename = "data_" + (process.env['LEAK'] ? "leak" : "noleak");

var memoryFile = fs.createWriteStream(filename + "_mem.txt");
var trendFile = fs.createWriteStream(filename + "_trend.txt");

gcstats.on('gc', function(e) {
  if (e.compacted) {
    trendFile.write(((new Date() - startTime) / 1000.0).toFixed(1) + " " + gcstats.stats().usage_trend + "\n");
  }
});

setInterval(function() {
  memoryFile.write(((new Date() - startTime) / 1000.0).toFixed(1) + " " + process.memoryUsage().heapUsed + "\n");
}, 2000);

process.on('exit', function() {
});

const json = '{  "name": "gcstats",  "description": "Get some data about how V8\'s GC is behaving within your node program .",  "version": "0.0.1",  "author": "Lloyd Hilaiel (http://lloyd.io)",  "engines": { "node": ">= 0.6.0" },  "repository": {        "type": "git",        "url": "https://github.com/lloydncb000gt/gcstats.git"   },   "licenses": [ { "type": "wtfpl" } ],   "bugs": {     "url" : "https://github.com/lloyd/gcstats/issues"   },   "scripts": {     "install": "make build"   },   "contributors": [   ]}';


var j = 0;

var leaked = [];

function doIt() {
  if ((new Date() - startTime) > 180 * 1000) process.exit(0);
  var f = [];
  var notleaked = [];
  for (var i = 0; i < 100000; i++) {
    f.push(JSON.parse(json));
    // there's a 1 in 10000 chance of leaking
    if (Math.random() > .9999) {
      (process.env['LEAK'] ? leaked : notleaked).push(JSON.parse(json));
    }
  }
  setTimeout(function() {
    doIt();
  }, 50);
}

doIt();
