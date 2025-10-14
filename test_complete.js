// test_complete.js
const sifParser = require('./build/Release/sifaddon.node');

function analyzeSifFile(filename) {
    try {
        const jsonResult = sifParser.sifFileToJson(filename);
        const data = JSON.parse(jsonResult);
        
        console.log('=== SIF File Analysis ===');
        console.log('File:', filename);
        console.log('Camera:', data.metadata.cameraModel);
        console.log('Dimensions:', data.dimensions.width + 'x' + data.dimensions.height);
        console.log('Frames:', data.metadata.numberOfFrames);
        console.log('Data points:', data.data.length);
        console.log('Data range:', Math.min(...data.data) + ' to ' + Math.max(...data.data));
        
        return data;
    } catch (error) {
        console.error('Error:', error.message);
        return null;
    }
}

// 測試多個文件
analyzeSifFile('./test_data/1OD_500uW_sapphire_200umFiber_.sif');