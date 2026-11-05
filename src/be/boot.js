var fs = require('fs');
var path = require('path');

var settingsDir = path.join(__dirname, 'settings');
if (!fs.existsSync(settingsDir)) {
  fs.mkdirSync(settingsDir, { recursive: true });
}

// 1. Configuración mínima en db.json para pasar las validaciones internas
var dbPath = path.join(settingsDir, 'db.json');
if (!fs.existsSync(dbPath)) {
  fs.writeFileSync(dbPath, JSON.stringify({ db: 'Tec' }, null, 2));
}

// 2. Configurar general.json con host 0.0.0.0 y puerto de Render
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

// 3. Parche directo a db.js para ignorar el armado manual de URI si existe MONGODB_URI
if (process.env.MONGODB_URI) {
  var dbModule = require('./db');
  var mongodb = require('mongodb');

  dbModule.init = function(callback) {
    mongodb.MongoClient.connect(process.env.MONGODB_URI, function(err, client) {
      if (err) {
        console.error('Error al conectar a MongoDB Atlas:', err);
        return callback(err);
      }
      
      // LynxChan almacena la instancia de la base de datos en dbModule.db()
      var dbName = process.env.MONGODB_URI.split('/').pop().split('?')[0] || 'Tec';
      var database = client.db(dbName);
      
      // Asignar el cliente y base de datos interna de LynxChan
      dbModule.conn = function() {
        return database;
      };
      
      callback();
    });
  };
}

// 4. Arrancar el motor de LynxChan
require('./kernel');
