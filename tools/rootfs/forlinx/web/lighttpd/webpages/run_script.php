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

require("helper_functions.php");

$i = 0;
$lock_string = "";
$currently_locked = false;

$var = read_desktop_file();

if($var==null)
{
	echo "Json.txt file is empty or doesn't exist.";
	return;
}

$submenu = isset($_GET["submenu"]) == true ? $_GET["submenu"] : "main_menu" ;


$found_app = get_application($var,$submenu,$_GET["app"]);

$lock_list = $found_app["Lock"];

//Verify that there is a lock specified for this application
if($lock_list != -1 )
{
	//Convert the string into an array. 

	//This might be backwards*********CHECKOUT********
	$lock_list_array = explode($lock_list," ");

	//Check if the lock list only has one lock. If so add to the array
	//Since index 0 will be empty

	if(count($lock_list_array) == 1)
		$lock_list_array[0] = $lock_list;

	//Check to see if any of the locks exist.
	for($x = 0;$x<count($lock_list_array);$x++)
	{
		if(file_exists ("lock/".$lock_list_array[$x])==true)
		{
			$currently_locked = true;
			break;
		}

	}
}
else
	$lock_list = "";

if($currently_locked==false)
{
	$script_link = $found_app["Exec"];

	$random_string = strval(rand());
	$random_string .= strval(rand());

	$script_command = "./execute_command.sh \"".addslashes($script_link). "\" ".$random_string.".txt ".$lock_list;

	$last_line = system($script_command." > /dev/null 2>/dev/null & ", $retval);
}

$menu_title = $found_app["Name"];
?>

<?php include "menubar.php"; ?>

<?php if($currently_locked==false){ ?>

<div id="container"></div>

<script>
	<?php echo "var uri_link = \"$random_string\";"; ?>

	var fail_count = 0;

	function update()
	{
	
		//This is a fix for IE browsers. IE likes to cache Ajax results. Therefore, adding a random string will prevent the browser from caching the Ajax request. 
		var uri = "tmp/"+uri_link+".txt?rand="+Math.round((Math.random()*2356))+Math.round((Math.random()*4321))+Math.round((Math.random()*3961));
	
		$.get(uri, function(data) 
		{
			fail_count = 0;
			data = jQuery.trim(data);
   
			//The execute_command script adds the text "_?!!MATRIX_SCRIPT_COMPLETED!!?_" to the end of the file to mark that the command is done executing
			var script_complete = data.indexOf("_?!!MATRIX_SCRIPT_COMPLETED!!?_");
			if(script_complete != -1)
			{
				//Just replace the end of file text to Script Complete which 
				data = data.replace("_?!!MATRIX_SCRIPT_COMPLETED!!?_", "Script Complete");
			}

			<?php 
				//GUI applications shouldn't have any output that needs to be displayed. Therefore, don't include the code that will update the HTML container
				//with the program's output. This prevents some flickering problems that occur in some GUI applications even if the HTML container is being passed no information.
				if($found_app["ProgramType"]!="gui"){ ?>
				$('#container').html("<pre>"+data+"</pre>");
				$('#container').scrollTop(document.getElementById("container").scrollHeight);		
			<?php } ?>

			if(script_complete != -1)
			{
				//Display the exit and back button.
				$('#main_menu_link').removeClass("hide_link");
				$('#back_link').removeClass("hide_link");

				<?php if($found_app["ProgramType"]=="gui"){ ?>
					$('#back_link').click();		
				<?php } ?>
			}
			else
				setTimeout("update()",1000);

		})
		//If a file cant be read or some other error related to trying to retrieve this file then JQuery executes the error function.
		//This sometimes occurs when the browser tries to read the output file before output file is even created. 
		.error(function() 
		{ 
			//This function will attempt to read the file 3 times with an increase delay for each attempt. If the file still hasn't 
			//been able to be read then an error is displayed and the Exit and Back Link is enabled.

			fail_count++;
			if(fail_count==3)
			{
				$('#container').html("Failed to read output file");
				$('#main_menu_link').removeClass("hide_link");
				$('#back_link').removeClass("hide_link");
			}
			else
			{
				//Every time it fails wait a signifcantly longer amount of time
				setTimeout("update()",fail_count*1500);
			}
		});

	
	}
	
	//Wait 500ms before trying to read the application output
	setTimeout("update()",500);
</script>
<?php }else{?>

This program can't run since a program is already running that contains a lock that this program is trying to use
<script>
	//Display the back and exit button since the application couldn't run
	$('#main_menu_link').removeClass("hide_link");
	$('#back_link').removeClass("hide_link");
</script>

<?php } ?>
