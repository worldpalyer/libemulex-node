var emulex = require("../lib/emulex.js");
var as = require("assert");
describe('emulex', function () {
    it("emulex", function (done) {
        emulex.bootstrap({
            ed2k: {
                callback: function (key, vals) {
                    console.log(key, vals);
                    if (key == "server_initialized") {
                        done();
                    }
                },
            },
        });
        emulex.ed2k_server_connect("abc", "14.23.162.173", 4122, true);
    });
});