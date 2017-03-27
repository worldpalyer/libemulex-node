var emulex = require("../lib/emulex.js");
var as = require("assert");
var fs = require("fs");
describe('emulex', function () {
    it("hash", function (done) {
        emulex.parse_hash({
            path: __dirname + "/../build/Release/emulex.node",
            bmd4: true,
            bmd5: true,
            bsha1: true,
        }, function (err, hash) {
            as.equal(err, "");
            console.log(hash);
            done();
        });
    });
    it("hash_err", function (done) {
        emulex.parse_hash({
            path: __dirname + "/../build/Release/emulexxxsx.node",
            bmd4: true,
            bmd5: true,
            bsha1: true,
        }, function (err, hash) {
            as.equal(err.length > 0, true);
            console.log(err);
            done();
        });
    });
    it("nodes.dat", function (done) {
        try {
            var ns = emulex.load_node_dat({
                path: __dirname + "/nodes.dat"
            });
            console.log(ns);
            as.equal(ns.length > 0, true);
            done();
        } catch (e) {
            as.equal(e, null);
        }
    });
    it("servers.met", function (done) {
        try {
            var ns = emulex.load_server_met({
                path: __dirname + "/server.met"
            });
            console.log(ns);
            as.equal(ns.length > 0, true);
            done();
        } catch (e) {
            as.equal(e, null);
        }
    });
    it("emulex", function (done) {
        // if (true) {
        //     done();
        //     return;
        // }
        try {
            fs.unlinkSync("abc.txt");
        } catch (e) {

        }
        emulex.bootstrap({
            ed2k: {
                port: 4769,
                callback: function (key, vals) {
                    console.log(key, vals);
                    if (key == "server_initialized") {
                        emulex.search_file({
                            query: "abc.txt",
                        });
                        return;
                    } else if (key == "server_shared") {
                        emulex.add_transfer({
                            path: vals[0].name,
                            size: vals[0].size,
                            hash: vals[0].hash,
                        });
                    } else if (key == "finished_transfer") {
                        as.equal(vals.hash, "0C2BE0003F0DEBDCF644525BDAF6E45D");
                        as.equal(vals.name, "abc.txt");
                        done();
                    }
                    // emulex.parse_hash({
                    //     path: __dirname + "/../build/Release/emulex.node",
                    //     bmd4: true,
                    //     bmd5: true,
                    //     bsha1: true,
                    // }, function (err, hash) {
                    //     as.equal(err, "");
                    //     console.log(hash);
                    //     done();
                    // });
                    // // setTimeout(function () {
                    // emulex.shutdown(done);
                    // }, 3000);
                },
            },
        });
        emulex.ed2k_server_connect("abc", "a.loc.w", 4122, true);
        // setTimeout(function () {
        //     emulex.shutdown();
        // }, 3000);
        // emulex.shutdown();
    });
});