// JavaScript Document
// Create an object to control an HTML5 audio object
// also set an audio source
var audioObj =
{	
	setSourcePlay: function (source) {
			
			// Get the audio object
			var audioElement = document.getElementById('playscript');
			
			// Set the new source file
			audioElement.setAttribute('src', source);
			
			// Set the MIME type
			audioElement.setAttribute('type',"audio/mpeg");
			
			// Hide the controls
			audioElement.removeAttribute("controls") ;
			
			// Load the audio
			audioElement.load();
			
			// Play the audio
			audioElement.play();
	}
}