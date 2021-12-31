//Import dependencies
//let electron = require("electron");
let electronEjs = require("electron-ejs");

const { app, BrowserWindow } = require('electron');
const path = require('path');

//Initialize the app
//let app = electron.app;
//app.use('/videos/', express.static('./videos/'));
//app.use('/videos/', electronEjs.static('./videos/'));



//Initialize the ejs parser
let ejs = new electronEjs({"key": "my value"}, {});

//Now you can read EJS files
app.on("ready", function () {
    //Create the new window
    mainWindow = new BrowserWindow({
        "width": 800, 
        "height": 600
    });

    //More app configuration
    // ....
    app.setPath ('userData', __dirname);

    //Load the ejs file
    mainWindow.loadURL("file://" + __dirname + "/views/pages/index.ejs");
});

