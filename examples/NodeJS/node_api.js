/*
 *
 * node_api.js
 *
 * WJD : 07/11/2013
 *
 * This module contains the API code for reading and writing to
 * the mysql database
 *
 */

// Load the express module
var express = require('express');

// Load the mysql module
var mysql = require('mysql');

// Open the mysql DB connection
console.log("Creating a MySQL connection.");

var db = mysql.createConnection({
    host     : 'localhost',
    user     : 'root',
    password : 'root',
});

// Connect to the DB
db.connect(function(error) {

  // If we cannot connect to mysql
  if(error) {
    console.log("ERROR : Cannot connect to MySQL.");
  }

  // We have a mysql connection
  console.log("SUCCESS : We have a MySQL connection.");

  // Get a handle to the expressjs framework
  var app = express();

  // Parse the incoming posted data
  app.use(express.bodyParser());

  // API to query a table for a record
  app.get('/tabapi/dbquery/:query', function(req, res) {

    // Select the Tabeeb Hospital DB
    db.query('USE Hospital', function(err, result) {

      // Check if we have an error
      if (err) {
        console.log("!!! Use DB Hospital FAILED !!!");
        throw err;
      } 
    }); 

    console.log("< *** GET called with : " + req.params.query + " *** >");

    var query = req.params.query;

    // Run the query to retrieve the records
    db.query(query, function(err, result) {

      // Check if we have an error
      if (err) {
        console.log("Query for all records FAILED !!!");
        res.end();
      }
      // Convert the response to JSON
      console.log(JSON.stringify({'result': result}, undefined, 2));
      res.end();

      //res.writeHead(200, { 'Content-Type' : 'application/json' });
      //res.send(JSON.stringify({'result': result}));
    });
  });

  // API to insert a record into a table
  app.post('/tabapi/dbinsert', function(req, res) {

    // Select the Tabeeb Hospital DB
    db.query('USE Hospital', function(err, result) {

      // Check if we have an error
      if (err) {
        console.log("!!! Use DB Hospital FAILED !!!");
        throw err;
      } 
    });  
   
    // Create our query to insert a record into the DB
    query = "INSERT INTO " + req.body.db + " (title, summary, event) values (" + "'" + req.body.ctitle + "'" + ',' + "'" + 
    req.body.csummary + "'" + ',' +  "'" + 
    req.body.cevent + "'" + ')';
   
    // Run the query to insert the record
    db.query(query, function(err, result) {

      // Check if we have an error
      if (err) {
        console.log("Insert into table " + req.body.db + " FAILED !!!");
        res.end();
      }
      // Dieplay the inserted record ID
      console.log("Inserted ID "  + result.insertId + " into table " + req.body.db);
    });
    // End the response
    res.end();
  });

  // Listen for commands
  app.listen(3000, function() {
    console.log("API Server listening on port 3000.");
  }); 
});  
