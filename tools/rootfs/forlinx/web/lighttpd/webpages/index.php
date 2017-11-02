<?php
/*
 * Copyright (C) 2011 Texas Instruments Incorporated - http://www.ti.com/ 
 * 
 * 
 *  Redistribution and use in source and binary forms, with or without 
 *  modification, are permitted provided that the following conditions 
 *  are met:
 *
 *    Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer.
 *
 *    Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the   
 *    distribution.
 *
 *    Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
*/
?>

<?php
	# Check if json.txt file needs to be generated
	if(!file_exists("json.txt"))
	{
		//Generate the json.txt file
		system("php generate.php");
		//Remove the cache since it is based on the previous json.txt file
		system("rm -rf cache/*");
	}

	if(!file_exists("cache"))
	{
		mkdir("cache",6666);		
	}

	$supportedResolutions = null;
	

	if(file_exists("supported_resolutions.txt")==true)
	{
		$supportedResolutions = file ("supported_resolutions.txt",FILE_IGNORE_NEW_LINES | FILE_SKIP_EMPTY_LINES);
		
	}

	if($supportedResolutions == null || count($supportedResolutions) == 0)
	{
		echo "supported_resolutions.txt doesn't exist or is empty";
		exit; 
	}

	$client_is_host = $_SERVER['SERVER_NAME']==$_SERVER['REMOTE_ADDR']||$_SERVER['SERVER_NAME'] == "localhost";
?>

<html>
<head>
<title>FORLINX DEMOS</title>
<meta http-equiv="X-UA-Compatible" content="IE=EDGE" />

<link rel="stylesheet" type="text/css" href="css/fonts-min.css">
<script type="text/javascript" src="/javascript/jquery-latest.js"></script>   
<link rel='stylesheet' type='text/css' href='css/global.css'>

</head>


<body class="unselectable" style = "-webkit-user-select: none;-moz-user-select: none;">
<div id = "complete_container"></div>

<script>
var has_graphics = true;
var link_history = ["submenu.php?submenu=main_menu&page=0"];
var uri = "submenu.php?submenu=main_menu&page=0";
var previous_clicked = uri;

<?php
if($client_is_host == true)
	echo "var client_is_host = true;";
else
	echo "var client_is_host = false;";
?>

$(document).ready(function()
{



	var supportedResolutions=new Array(); 

	<?php
		for($x = 0;$x<count($supportedResolutions);$x++)
		{
			echo "supportedResolutions[".$x."]=\"".$supportedResolutions[$x]."\";";    
		}
	?>

	var screenWidth = 0;
	var screenHeight = 0;
	var iconGridCol = 0;
	var iconGridRow = 0;

	for(var i=0; i<supportedResolutions.length; i++) 
	{
		var value = supportedResolutions[i].split('x');

		screenWidth = value[0];
		screenHeight = value[1];
		iconGridCol = value[2];
		iconGridRow = value[3];

		if(screen.width >= screenWidth && screen.height >= screenHeight)
			break;
	}

	document.cookie="iconGridCol="+iconGridCol;
	document.cookie="iconGridRow="+iconGridRow;
	
	$('head').append('<link rel="stylesheet" type="text/css" href="/css/'+screenWidth+'x'+screenHeight+'.css" />'); 

	if(client_is_host==false)
	{
		var r=confirm("Does your target system have an attached display device?\nClick Ok and Remote Matrix will assume that a proper display device is attached to your target system.\nClick Cancel and Remote Matrix will assume that you do not have a display device attached to your target system.");
		if (r==true)
			has_graphics = true;
		else
			has_graphics = false;
	}

	$.get(uri, function(data) 
	{
				$('#complete_container').html(data);
				$("#back_link").attr("href",link_history[link_history.length-2]);
	});


});







$("#complete_container").delegate("img", "mousedown", function(e)
{
       e.preventDefault();
});

$("#complete_container").delegate("a", "click", function(e)
{
	e.preventDefault();
	e.stopPropagation();
	var className = $(this).attr('class');
	var idName = $(this).attr('id');
	var link =  $(this).attr('href');

	//Sometimes if a request is taking a long time you might try clicking a link more then once thinking that
	//your click request was not accepted. This causes multiple request for the same page to be sent which in turn
	//sometimes results in every link you click causing 2+ request to go through. This code checks to make sure
	//your requesting a new pageand not the same page twice
	if(link==previous_clicked)
		return false;

	previous_clicked = link;

	if(idName=="back_link")
	{
		link_history.pop();
	}
	else if(idName=="main_menu_link")
	{
		link_history = ["submenu.php?submenu=main_menu&page=0"];
	}
	else
		link_history.push(link);

	//Adds a random string to the end of the $_GET Query String for page accessed.
	//This prevents IE from caching the Ajax request.
	link = link + "&rand="+Math.round((Math.random()*2356))+Math.round((Math.random()*4321))+Math.round((Math.random()*3961));
	$.get(link, function(data) 
	{
		$('#complete_container').html(data);
		$("#back_link").attr("href",link_history[link_history.length-2]);
	});		
});

</script>

	</body>
</html>

