// JavaScript Document
// Create a object to control an HTML5 audio object
// also set a audio source
var audioObj =
{	
	// Initialize a variable to hold the audio element object
	audioElement: "",
	init: function() {
			// Get the audio object
			this.audioElement = document.getElementById('playscript');
			
	},
	setSourcePlay: function (source) {
						
			// Set the audio new source file
			this.audioElement.setAttribute('src', source);
			
			// Set the audio MIME type
			this.audioElement.setAttribute('type',"audio/mpeg");
			
			// Hide the audio controls
			this.audioElement.removeAttribute("controls") ;
			
			// Load the audio file
			this.audioElement.load();
			
			// Play the audio
			this.audioElement.play();
	}
}
