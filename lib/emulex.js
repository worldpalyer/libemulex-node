var emulex = null;
// try {
emulex = require(__dirname + '/../build/Release/emulex.node');
// } catch (error) {
//     emulex = require(__dirname + '/../build/default/emulex.node');
// }
exports.bootstrap = emulex.bootstrap;
exports.shutdown = emulex.shutdown;
exports.ed2k_server_connect = emulex.ed2k_server_connect;
exports.ed2k_add_dht_node = emulex.ed2k_add_dht_node;
exports.add_transfer = emulex.add_transfer;
exports.parse_hash = emulex.parse_hash;
exports.search_file = emulex.search_file;
exports.search_hash_file = emulex.search_hash_file;
exports.load_node_dat = emulex.load_node_dat;
exports.load_server_met = emulex.load_server_met;