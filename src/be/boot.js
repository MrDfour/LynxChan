// Carga las variables de entorno de Render si existen
var settingsHandler = require('./settingsHandler');

// Si existe MONGODB_URI en Render, sobrescribe la configuración en memoria
if (process.env.MONGODB_URI) {
  var dbSettings = settingsHandler.getDbSettings();
  dbSettings.address = process.env.MONGODB_URI;
}

// Inicia el boot normal de LynxChan
require('./kernel');
