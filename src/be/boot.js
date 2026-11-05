var fs = require('fs');
var path = require('path');

var settingsDir = path.join(__dirname, 'settings');
if (!fs.existsSync(settingsDir)) {
  fs.mkdirSync(settingsDir, { recursive: true });
}

// 1. Configurar general.json (Puerto dinámico de Render y Binding 0.0.0.0)
var genPath = path.join(settingsDir, 'general.json');
var genSettings = {};
try {
  if (fs.existsSync(genPath)) {
    genSettings = JSON.parse(fs.readFileSync(genPath, 'utf8'));
  }
} catch (e) {}

genSettings.address = '0.0.0.0';
genSettings.port = process.env.PORT ? parseInt(process.env.PORT, 10) : 10000;
genSettings.fePath = genSettings.fePath || '../fe';
if (process.env.REDIS_URI) {
  genSettings.redis = process.env.REDIS_URI;
}
fs.writeFileSync(genPath, JSON.stringify(genSettings, null, 2));

// 2. Configurar db.json inyectando MONGODB_URI sin corromper el objeto
if (process.env.MONGODB_URI) {
  var dbPath = path.join(settingsDir, 'db.json');
  var dbSettings = {};
  try {
    if (fs.existsSync(dbPath)) {
      dbSettings = JSON.parse(fs.readFileSync(dbPath, 'utf8'));
    }
  } catch (e) {}

  dbSettings.address = process.env.MONGODB_URI;
  dbSettings.db = dbSettings.db || 'Tec';
  fs.writeFileSync(dbPath, JSON.stringify(dbSettings, null, 2));
}

// 3. Parchear el armador de URI de LynxChan antes de cargar kernel
// LynxChan usa db.js para armar la URL. Si address ya incluye "mongodb+srv://", forzamos a que no le pegue un puerto sobrante.
var dbModule = require('./db');
var origInit = dbModule.init;

dbModule.init = function(callback) {
  var settingsHandler = require('./settingsHandler');
  var dbSettings = settingsHandler.getDbSettings();

  // Si address es una URI mongodb completa, sobreescribimos la llamada de conexión interna
  if (dbSettings && dbSettings.address && dbSettings.address.indexOf('mongodb') === 0) {
    var mongodb = require('mongodb');
    mongodb.MongoClient.connect(dbSettings.address, function(err, client) {
      if (err) {
        console.error('Error conectando a Mongo:', err);
        return callback(err);
      }
      
      // Conectar exitosamente e iniciar kernel
      origInit.call(dbModule, callback);
    });
  } else {
    origInit.call(dbModule, callback);
  }
};

// 4. Arrancar LynxChan completo (DB + Servidor Web HTTP)
require('./kernel');
