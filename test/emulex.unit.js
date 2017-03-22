var emulex = require("../lib/emulex.js");
var as = require("assert");
describe('emulex', function () {
    it("emulex", function (done) {
        emulex.bootstrap({
            ed2k: {
                callback: function (key, vals) {
                    console.log(key, vals);
                    if (key == "server_initialized") {
                        // setTimeout(function () {
                        emulex.shutdown(done);
                        // }, 3000);
                    }
                },
            },
        });
        emulex.ed2k_server_connect("abc", "loc.w", 4122, true);
        // setTimeout(function () {
        //     emulex.shutdown();
        // }, 3000);
        // emulex.shutdown();
    });
});