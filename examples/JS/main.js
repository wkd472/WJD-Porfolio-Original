// JavaScript Document
var detect = {
    browser: function() {
        $('#browser').text(BrowserDetect.browser);
        $('#version').text(BrowserDetect.version); 
    }        
};


$(function() {
    detect.browser();
    
    $("button").button();
});



