// JavaScript Document
// Create an object that holds the functions
			var Picture = 
			{
				init: function()
				{
					// Now set the click events using JQuery to enlarge the pix 
					$("#pic1").bind("click", Picture.picOnClickListner);
					$("#pic2").bind("click", Picture.picOnClickListner);
					$("#hide").bind("click", Picture.picHideListner);
				},
				picOnClickListner: function(event)
				{
					// Get the ID of the event caller
					var id = event.target.id;
					
					// Large Front Picture
					if(id == "pic1")
					{
						Picture.showLargePic('IMG_7717.JPG');
					}
					else // Large Back Picture
					{
						Picture.showLargePic('IMG_7718.JPG');
					}
					event.preventDefault();
				},
				picHideListner: function(event)
				{
					Picture.hideLargePic();	
					event.preventDefault();		
				},				
				/* WJD : Code added to show the window */
				showLargePic: function(pic) {	
					
					// Get the img elements and set the new src
					document.getElementById("pic").src=pic;		
					
					// Get the picture div elements
					var el = document.getElementById("largepic");
									
					// Now display the div
					el.style.visibility = (el.style.visibility == "visible") ? "hidden" : "visible";
			
				},		
				/* WJD : Code added to hide the window*/
				hideLargePic: function() {		
					// Get the img elements and set the new src
					document.getElementById("pic").src="'data:image/gif;base64,R0lGODlhAQABAIAAAAAAAP///ywAAAAAAQABAAACAUwAOw=='";		
				
					// Get the picture div elements		
					var el = document.getElementById("largepic");
					el.style.visibility = (el.style.visibility == "visible") ? "hidden" : "visible";
				}
			};
			
			// Now call the Picture objects init function to set everything up
			Picture.init();