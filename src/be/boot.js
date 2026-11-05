var fs = require('fs');
var path = require('path');

var settingsDir = path.join(__dirname, 'settings');
if (!fs.existsSync(settingsDir)) {
  fs.mkdirSync(settingsDir, { recursive: true });
}

// 1. Inyectar MONGODB_URI en db.json
if (process.env.MONGODB_URI) {
  var dbPath = path.join(settingsDir, 'db.json');
  var dbSettings = {};

  try {
    if (fs.existsSync(dbPath)) {
      dbSettings = JSON.parse(fs.readFileSync(dbPath, 'utf8'));
    }
  } catch (e) {}

  dbSettings.address = process.env.MONGODB_URI;
  fs.writeFileSync(dbPath, JSON.stringify(dbSettings, null, 2));
}

// 2. Inyectar Bind Address (0.0.0.0) y Puerto en general.json
var genPath = path.join(settingsDir, 'general.json');
var genSettings = {};

try {
  if (fs.existsSync(genPath)) {
    genSettings = JSON.parse(fs.readFileSync(genPath, 'utf8'));
  }
} catch (e) {}

if (process.env.REDIS_URI) {
  genSettings.redis = process.env.REDIS_URI;
}

// Forzar binding publico y asignar el puerto dinamico de Render
genSettings.address = '0.0.0.0';
genSettings.port = process.env.PORT ? parseInt(process.env.PORT, 10) : 10000;
genSettings.fePath = genSettings.fePath || '../fe';

fs.writeFileSync(genPath, JSON.stringify(genSettings, null, 2));

// 3. Iniciar LynxChan
require('./kernel');
