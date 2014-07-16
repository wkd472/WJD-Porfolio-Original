/*
 *
 * This is a nodejs server to serve up a html page on port 8124
 *
 * WJD : 07/12/2013
 *
 */

// Define the modules to use and the base path to the 
// html file.
var  http = require('http'),
     fs = require('fs'),
     base = '/var/www/test';

// Create a nodejs server to serve up our test page.
http.createServer(function(req, res) {

      // Build our working path
      pathname = base + req.url;

      // See if we can find our html file
      fs.exists(pathname, function(exists) {

        // Do we have an error
        if (!exists) {
            // Yep, display an error
            res.writeHead(404);
            res.write('Bad request 404\n');
            res.end();
      } else {
                 // HTML file found, now display it
                 res.setHeader('Content-Type', 'text/html');

                          // 200 status - found, no errors
                          res.statusCode = 200;

                          // create and pipe readable stream
                          var file = fs.createReadStream(pathname);
                            file.on("open", function() {
                                    file.pipe(res);
                            });
                            
                            // Open file error
                            file.on("error", function(err) {
                              console.log(err);
                            });
              } 
      });
}).listen(8124);

console.log('Server running at 8124');

