var fs = require('fs');
var path = require('path');

// 1. Inyectar MONGODB_URI en settings/db.json si existe en Render
if (process.env.MONGODB_URI) {
  var dbPath = path.join(__dirname, 'settings', 'db.json');
  var dbSettings = {};

  try {
    if (fs.existsSync(dbPath)) {
      dbSettings = JSON.parse(fs.readFileSync(dbPath, 'utf8'));
    }
  } catch (e) {
    dbSettings = { db: 'lynxchan' };
  }

  dbSettings.address = process.env.MONGODB_URI;
  fs.writeFileSync(dbPath, JSON.stringify(dbSettings, null, 2));
}

// 2. Inyectar REDIS_URI y PORT en settings/general.json si existen en Render
if (process.env.REDIS_URI || process.env.PORT) {
  var genPath = path.join(__dirname, 'settings', 'general.json');
  var genSettings = {};

  try {
    if (fs.existsSync(genPath)) {
      genSettings = JSON.parse(fs.readFileSync(genPath, 'utf8'));
    }
  } catch (e) {}

  if (process.env.REDIS_URI) {
    genSettings.redis = process.env.REDIS_URI;
  }

  fs.writeFileSync(genPath, JSON.stringify(genSettings, null, 2));
}

// 3. Iniciar el arranque normal de LynxChan sin dependencias circulares
require('./kernel');
