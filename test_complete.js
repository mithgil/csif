// test_complete.js
const sifParser = require('./build/Release/sifaddon.node');

function analyzeSifFile(filename) {
    try {
        const jsonResult = sifParser.sifFileToJson(filename);
        const data = JSON.parse(jsonResult); 

        console.log('json data: ',data);
                
        console.log('=== SIF File Analysis ===');
        console.log('File:', filename);
        console.log('Camera:', data.metadata.cameraModel);
        console.log('Dimensions:', data.dimensions.width + 'x' + data.dimensions.height);
        console.log('Frames:', data.metadata.numberOfFrames);
        console.log('Data points:', data.data.length);
        
        // 使用高效的方法計算最小值和最大值
        let min = data.data[0];
        let max = data.data[0];
        for (let i = 1; i < data.data.length; i++) {
            if (data.data[i] < min) min = data.data[i];
            if (data.data[i] > max) max = data.data[i];
        }
        console.log('Data range:', min + ' to ' + max);
        
        // 檢查數據結構
        console.log('Data structure check:');
        console.log('  First frame (0-10):', data.data.slice(0, 10));
        console.log('  Second frame (1024-1034):', data.data.slice(1024, 1034));
        console.log('  Last frame (last 10):', data.data.slice(-10));
        
        return data;
    } catch (error) {
        console.error('Error:', error.message);
        return null;
    }
}

const batchFilename = '/home/tim/Documents/AS/data/andor/20240313 mos2 si/mos2_si_power_1OD_optimized_50px_4.5um_.sif';
const singleFilename = './test_data/1OD_500uW_sapphire_200umFiber_.sif';

analyzeSifFile(batchFilename);