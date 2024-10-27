const fs = require('fs')
const readline = require('readline');

const fileStream = fs.createReadStream(process.argv[2]);
let outFnBase = process.argv[2];
let lastIdx = outFnBase.lastIndexOf('.');
if(lastIdx < 0) lastIdx = outFnBase.length;
outFnBase = outFnBase.substring(0, lastIdx);
const doSplit = fs.statSync(process.argv[2])['size'] > (1.5 * 1024 * 1024 * 1024);

const rl = readline.createInterface({input: fileStream, crlfDelay: Infinity});

let counter = 0;
let currSize = 0;
let print = true;
let currFn = `${outFnBase}${(doSplit ? '0' : '')}.lines`;
rl.on('line', function(line) {
	let str = JSON.parse(line)['text'];
	if(str.endsWith('\n')) str = str.substring(0, str.length - 1);
	str = str.replace(/\r/g, "").replace(/\n/g, "\\n");
	fs.appendFile(currFn, `${str}\n`, function(err) {
		if(err) {
			console.warn('Append failed');
		}
	});
	currSize += str.length + 1;
	if(doSplit && currSize >= 1024 * 1024 * 1024) {
		currSize = 0;
		counter++;
		currFn = `${outFnBase}${(doSplit ? counter : '')}.lines`;
	}
});
