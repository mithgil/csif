const addon = require('./build/Release/sifaddon.node');

console.log('SIF Addon loaded successfully!');
console.log('Available functions:', Object.keys(addon));

// 測試函數是否存在
if (typeof addon.sifFileToJson === 'function') {
    console.log('sifFileToJson function is available');
} else {
    console.log('sifFileToJson function is NOT available');
    process.exit(1);
}

// 如果有測試 SIF 文件，可以實際測試
try {
    // 替換為您的測試 SIF 文件路徑
    const testFile = './test_data/1OD_500uW_sapphire_200umFiber_.sif'; 
    const fs = require('fs');
    
    if (fs.existsSync(testFile)) {
        console.log('\nTesting with file:', testFile);
        const result = addon.sifFileToJson(testFile);
        console.log('Success! JSON length:', result.length);
        console.log('First 200 characters:', result.substring(0, 200));
    } else {
        console.log('\nTest file not found:', testFile);
        console.log('Please create a test SIF file or specify the correct path');
    }
} catch (error) {
    console.log('\nError during SIF file processing:');
    console.log('Error message:', error.message);
}