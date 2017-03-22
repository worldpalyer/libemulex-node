var emulex = null;
// try {
emulex = require(__dirname + '/../build/Release/emulex.node');
// } catch (error) {
//     emulex = require(__dirname + '/../build/default/emulex.node');
// }
exports.bootstrap = emulex.bootstrap;
exports.ed2k_server_connect = emulex.ed2k_server_connect;
exports.ed2k_add_dht_node = emulex.ed2k_add_dht_node;