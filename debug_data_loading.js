const addon = require('./build/Release/sifaddon.node');

console.log('=== Testing SIF Parser with Detailed Debug ===');
try {
    const result = addon.sifFileToJson('./test_data/1OD_500uW_sapphire_200umFiber_.sif');
    const data = JSON.parse(result);
    
    console.log('✓ Successfully parsed SIF file');
    console.log('Data array length:', data.data.length);
    
    if (data.data.length > 0) {
        console.log('First 10 data points:', data.data.slice(0, 10));
        console.log('Data range:', Math.min(...data.data), 'to', Math.max(...data.data));
    }
    
} catch (error) {
    console.log('❌ Error:', error.message);
}