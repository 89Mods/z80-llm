const fs = require('fs')

fs.readFile("./encoder.json", (error, data) => {
	if(error) {
		console.warn(error);
		return;
	}
	let json = JSON.parse(data);
	let res = {};
	Object.keys(json).forEach(function(key) {
		let val = json[key];
		key = key.replace('~', '-').replace('Ġ', '~');
		res[val] = key;
	});
	let pointersLength = Object.keys(json).length * 4 + 4;
	let pointersBuffer = Buffer.alloc(pointersLength);
	let fileDescriptor = fs.openSync('dict.bin', 'w', 438);
	let dataBuffer = new Buffer(0); //Hopefully enough
	let position = 0;
	let pointersPosition = 4;
	pointersBuffer[0] = pointersLength & 0xFF;
	pointersBuffer[1] = (pointersLength >> 8) & 0xFF;
	pointersBuffer[2] = (pointersLength >> 16) & 0xFF;
	pointersBuffer[3] = (pointersLength >> 24) & 0xFF;
	let lastKey = -1;
	Object.keys(res).forEach(function(key) {
		let positionAfterPointers = position + pointersLength;
		pointersBuffer[pointersPosition+0] = positionAfterPointers & 0xFF;
		pointersBuffer[pointersPosition+1] = (positionAfterPointers >> 8) & 0xFF;
		pointersBuffer[pointersPosition+2] = (positionAfterPointers >> 16) & 0xFF;
		pointersBuffer[pointersPosition+3] = (positionAfterPointers >> 24) & 0xFF;
		pointersPosition += 4;
		let val = res[key];
		if(parseInt(key) != parseInt(lastKey) + 1) {
			console.warn('Sequence break ' + key + ' ' + lastKey);
		}
		lastKey = key;
		let numBuffer = new Buffer(2);
		numBuffer[0] = key & 0xFF;
		numBuffer[1] = key >> 8;
		let zeroBuffer = new Buffer(1);
		zeroBuffer[0] = 0;
		let textBuffer = Buffer.from(val, 'utf8');
		dataBuffer = Buffer.concat([dataBuffer, numBuffer, textBuffer, zeroBuffer]);
		position += 2 + textBuffer.length + 1;
	});
	fs.writeSync(fileDescriptor, pointersBuffer, 0, pointersLength);
	fs.writeSync(fileDescriptor, dataBuffer, 0, position);
	fs.closeSync(fileDescriptor);
});
