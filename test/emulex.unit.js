var emulex = require("../lib/emulex.js");
var as = require("assert");
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
    it("emulex", function (done) {
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