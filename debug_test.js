const addon = require('./build/Release/sifaddon.node');
const fs = require('fs');

function debugSifFile(filename) {
    try {
        console.log('=== Debug SIF File Analysis ===');
        console.log('File:', filename);
        
        const jsonString = addon.sifFileToJson(filename);
        const data = JSON.parse(jsonString);
        
        console.log('\n=== File Structure ===');
        console.log('Metadata:', data.metadata);
        console.log('Dimensions:', data.dimensions);
        console.log('Calibration:', data.calibration);
        
        console.log('\n=== Data Analysis ===');
        if (data.data && Array.isArray(data.data)) {
            console.log('Data array length:', data.data.length);
            if (data.data.length > 0) {
                console.log('First frame data length:', data.data[0].length);
                if (data.data[0].length > 0) {
                    console.log('First 10 data points:', data.data[0].slice(0, 10));
                }
            } else {
                console.log('❌ Data array is empty!');
            }
        } else {
            console.log('❌ Data field is missing or not an array!');
        }
        
        // 檢查文件大小
        const stats = fs.statSync(filename);
        console.log('\n=== File Info ===');
        console.log('File size:', stats.size, 'bytes');
        console.log('Expected data size:', data.dimensions.width * data.dimensions.height * data.metadata.numberOfFrames * 4, 'bytes (float32)');
        
    } catch (error) {
        console.log('Error:', error.message);
    }
}

// 運行調試
debugSifFile('./test_data/1OD_500uW_sapphire_200umFiber_.sif');