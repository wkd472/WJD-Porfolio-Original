// JavaScript Document
// Wait for document to be ready 
            $(document).ready(function() {
                
                $("#video5btn").click(function() {
                    
                    var src = "video/beatles1966.mp4";
                    
                    document.getElementById('video').pause();
                    $("#video").find("#video1").attr("src", "");
                    $("#video").find("#video2").attr("src", "");
                    $("#video")[0].load();
                    
                    if(BrowserDetect.browser === "Firefox")
                    {
                        
                        src = "video/beatles1966.ogv";
                    
                        $("#video").find("#video2").attr("src", src);
                    }
                    else
                    {
                        $("#video").find("#video1").attr("src", src);
                    }
                        
                    $("#video")[0].load(); 
                    
                    $("#video")[0].prop('loop', false);
                   
                    document.getElementById('video').currentTime=0;
                    document.getElementById('video').play();
                });
                
                $("#video4btn").click(function() {
                    
                    var src = "video/cartoonbeatles.mp4";
                    
                    document.getElementById('video').pause();
                    $("#video").find("#video1").attr("src", "");
                    $("#video").find("#video2").attr("src", "");
                    $("#video")[0].load();
                    
                    if(BrowserDetect.browser === "Firefox")
                    {
                        
                        src = "video/cartoonbeatles.ogv";
                    
                        $("#video").find("#video2").attr("src", src);
                    }
                    else
                    {
                        $("#video").find("#video1").attr("src", src);
                    }
                        
                    $("#video")[0].load(); 
                    $("#video")[0].prop('loop', false);
                    
                    document.getElementById('video').currentTime=0;
                    document.getElementById('video').play();
     
                });
                
                $("#video3btn").click(function() {
                    
                    var src = "video/fusionexplosion.mp4";
                    
                    document.getElementById('video').pause();
                    $("#video").find("#video1").attr("src", "");
                    $("#video").find("#video2").attr("src", "");
                    $("#video")[0].load();
                    
                    if(BrowserDetect.browser === "Firefox")
                    {
                        
                        src = "video/fusionexplosion.ogv";
                    
                        $("#video").find("#video2").attr("src", src);
                    }
                    else
                    {
                        $("#video").find("#video1").attr("src", src);
                    }
                        
                    $("#video")[0].load(); 
                    $("#video")[0].prop('loop', false);
                    
                    document.getElementById('video').currentTime=0;
                    document.getElementById('video').play();
     
                });
                
                $("#video2btn").click(function() {
                    
                    var src = "video/earthfrommoon.mp4";
                    
                    document.getElementById('video').pause();
                    $("#video").find("#video1").attr("src", "");
                    $("#video").find("#video2").attr("src", "");
                    $("#video")[0].load();
                    
                    if(BrowserDetect.browser === "Firefox")
                    {
                        
                        src = "video/earthfrommoon.ogv";
                    
                        $("#video").find("#video2").attr("src", src);
                    }
                    else
                    {
                        $("#video").find("#video1").attr("src", src);
                    }
                        
                    $("#video")[0].load(); 
                    $("#video")[0].prop('loop', false);
                    
                    document.getElementById('video').currentTime=0;
                    document.getElementById('video').play();
     
                });
                
                $("#video1btn").click(function() {
                    
                    var src = "video/earth24hourcycle.mp4";
                    
                    document.getElementById('video').pause();
                    $("#video").find("#video1").attr("src", "");
                    $("#video").find("#video2").attr("src", "");
                    $("#video")[0].load();
                    
                    if(BrowserDetect.browser === "Firefox")
                    {
                        
                        src = "video/earth24hourcycle.ogv";
                    
                        $("#video").find("#video2").attr("src", src);
                    }
                    else
                    {
                        $("#video").find("#video1").attr("src", src);
                    }
                        
                    $("#video")[0].load(); 
                    $("#video")[0].prop('loop', false);
                    
                    document.getElementById('video').currentTime=0;
                    document.getElementById('video').play();
     
                });
            });