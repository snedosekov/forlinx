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


$var = read_desktop_file();

if($var==null)
{
	echo "Json.txt file is empty or doesn't exist.";
	return;
}

//Some parts of the code doesn't set the submenu variable when the user is at the Main Menu which is relected in the "top" variable
$submenu = isset($_GET["submenu"]) == true ? $_GET["submenu"] : "main_menu" ;

$found_app =  get_application($var,$submenu,$_GET["app"]);

$menu_title = $found_app["Name"];
$enable_main_menu_link = true;
?>
	<?php include "menubar.php"; ?>

<?php
	

	$title = $found_app["Name"];
	
	$description = "No Description";
	$program_type = $found_app["ProgramType"];

	if($found_app["Description_Link"]!=-1)
	{
		$handle = fopen($found_app["Description_Link"], "rb");
		$description = fread($handle,filesize($found_app["Description_Link"]));
		fclose($handle);

		if(strlen($description)==0)
			$description = "Invalid link for description page";
	}

	$app_title = $found_app["Name"];

	$link =  "run_script.php?&submenu=".urlencode($submenu)."&app=".urlencode($app_title);
?>
	<div id = "descrip_text">
		<div id ="no_display" style = "display:none;">
			<h1 style = "color:red;">Sorry</h1>
			You can't run the GUI application <?php echo $title; ?>. The system has detected that your embedded system is not connected to a display device.
		</div>
		<div id = "running_remotely"  style = "display:none;">
			<h1 style = "color:yellow;">Warning</h1>
				You are currently running Matrix remotely and <?php echo $title; ?> is a GUI based application. <br>
				After clicking run, look at the display device connected to the embedded system to see and/or interact with the application
		</div>
		<div id = "run_application" style= "text-align:center;">
			<a href = "<?php echo $link;?>"><img id = "run_img" src= "images/run-icon.png"></a>
		</div>
		<div>
			<?php echo $description; ?>
		</div>
	</div>

<script>
	<?php 
		if($program_type=="gui")
			echo "var isgraphicalApp = true;";
		else
			echo "var isgraphicalApp = false;";
	?>
	
	if(isgraphicalApp == true)
	{
		if(has_graphics == false)
		{
		
			$("#no_display").show();
			$("#run_application").hide();
		}
		else if(client_is_host == false)
		{
			$("#running_remotely").show();
		}
	}
</script>
