var fs = require('fs');
var path = require('path');

var settingsDir = path.join(__dirname, 'settings');
if (!fs.existsSync(settingsDir)) {
  fs.mkdirSync(settingsDir, { recursive: true });
}

// Inyectar MONGODB_URI desde Render si existe
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

// Inyectar REDIS_URI y PORT desde Render si existen
if (process.env.REDIS_URI || process.env.PORT) {
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
  if (process.env.PORT) {
    genSettings.port = parseInt(process.env.PORT, 10);
  }
  genSettings.fePath = genSettings.fePath || '../fe';

  fs.writeFileSync(genPath, JSON.stringify(genSettings, null, 2));
}

// Iniciar LynxChan de forma estándar
require('./kernel');
