// index.js - 主模組入口
const addon = require('./build/Release/sifaddon.node');

/**
 * 解析 SIF 文件並返回 JSON 數據
 * @param {string} filename - SIF 文件路徑
 * @returns {Object} 包含光譜數據和元數據的對象
 * @throws {Error} 當文件無法解析時拋出錯誤
 */
function parseSifFile(filename) {
  try {
    const jsonString = addon.sifFileToJson(filename);
    return JSON.parse(jsonString);
  } catch (error) {
    throw new Error(`Failed to parse SIF file '${filename}': ${error.message}`);
  }
}

/**
 * 快速檢查 SIF 文件信息
 * @param {string} filename - SIF 文件路徑
 * @returns {Object} 基本信息對象
 */
function getFileInfo(filename) {
  const data = parseSifFile(filename);
  return {
    camera: data.metadata.cameraModel,
    dimensions: data.dimensions,
    frames: data.metadata.numberOfFrames,
    exposureTime: data.metadata.exposureTime,
    dataPoints: data.data.length
  };
}

module.exports = {
  parseSifFile,
  getFileInfo,
  // 保持向後兼容
  sifFileToJson: addon.sifFileToJson
};
