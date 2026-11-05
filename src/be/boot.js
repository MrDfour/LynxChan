var fs = require('fs');
var path = require('path');

// 1. Asegurar archivos de configuración mínimos para pasar el check inicial
var dbPath = path.join(__dirname, 'settings', 'db.json');
if (!fs.existsSync(dbPath)) {
  fs.writeFileSync(dbPath, JSON.stringify({ address: '127.0.0.1', port: 27017, db: 'Tec' }, null, 2));
}

var genPath = path.join(__dirname, 'settings', 'general.json');
if (!fs.existsSync(genPath)) {
  fs.writeFileSync(genPath, JSON.stringify({ port: 10000, address: '0.0.0.0', fePath: '../fe', verbose: true }, null, 2));
}

// 2. Parchear el handler de DB para que entregue la URI completa de Atlas si existe la variable
var dbModule = require('./db');
var originalInit = dbModule.init;

if (process.env.MONGODB_URI) {
  var settingsHandler = require('./settingsHandler');
  var origGetDbSettings = settingsHandler.getDbSettings;
  
  settingsHandler.getDbSettings = function() {
    var settings = origGetDbSettings.apply(this, arguments) || {};
    settings.url = process.env.MONGODB_URI;
    return settings;
  };
}

// 3. Arrancar LynxChan
require('./kernel');
