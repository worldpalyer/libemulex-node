var emulex = require("../lib/emulex.js");
var as = require("assert");
emulex.bootstrap({
    ed2k: {
        port: 4668,
        callback: function (key, vals) {
            console.log(key, vals);
            if (key == "server_initialized") {
            } else if (key == "server_shared") {
            }
        },
    },
});
var filepath = __dirname + "/run.js";
emulex.parse_hash({
    path: filepath,
    bmd4: true,
    bmd5: true,
    bsha1: true,
}, function (err, hash) {
    as.equal(err, "");
    console.log(hash);
    emulex.add_transfer({
        path: filepath,
        size: hash.size,
        hash: hash.md4,
        // resources: JSON.stringify({
        //     md5: hash.md5,
        //     sha1: hash.sha1,
        // }),
        parts: hash.parts,
        seed: true,
    });
    emulex.ed2k_server_connect("offer", "14.23.162.173", 4122, true);
});
var express = require('express');
var app = express();

app.get('/', function (req, res) {
    res.send('Hello World');
});

app.listen(3000);