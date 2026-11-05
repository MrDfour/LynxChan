var fs = require('fs');
var path = require('path');

var settingsDir = path.join(__dirname, 'settings');
if (!fs.existsSync(settingsDir)) {
  fs.mkdirSync(settingsDir, { recursive: true });
}

// Configuración de base de datos
if (process.env.MONGODB_URI) {
  var dbPath = path.join(settingsDir, 'db.json');
  var dbSettings = {};

  try {
    if (fs.existsSync(dbPath)) {
      dbSettings = JSON.parse(fs.readFileSync(dbPath, 'utf8'));
    }
  } catch (e) {}

  // Parsear la cadena mongodb+srv para inyectar los campos nativos de LynxChan
  var uri = process.env.MONGODB_URI;
  
  // Si es un enlace mongodb+srv o mongodb completo
  if (uri.startsWith('mongodb')) {
    dbSettings.address = uri;
    // Forzamos a que LynxChan pase la URI directa al MongoClient
    dbSettings.connectUrl = uri;
  }

  fs.writeFileSync(dbPath, JSON.stringify(dbSettings, null, 2));
}

// Configuración general
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

// Reemplazar el método de inicialización de DB en tiempo de ejecución para evitar el ensamblado manual de URLs
var dbModule = require('./db');
var origInit = dbModule.init;

dbModule.init = function(callback) {
  if (process.env.MONGODB_URI) {
    var mongoClient = require('mongodb').MongoClient;
    mongoClient.connect(process.env.MONGODB_URI, function(err, client) {
      if (err) {
        console.error('Error al conectar a MongoDB Atlas:', err);
        if (callback) callback(err);
        return;
      }
      // Si la conexión directa a Atlas es exitosa, dejamos que continúe el init normal
      origInit.call(dbModule, callback);
    });
  } else {
    origInit.call(dbModule, callback);
  }
};

// Iniciar el kernel de LynxChan
require('./kernel');
