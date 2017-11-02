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

$icon_per_col = $_COOKIE["iconGridCol"];
$icon_per_row = $_COOKIE["iconGridRow"];

$icons_per_page = $icon_per_col * $icon_per_row;
$current_page = isset($_GET["page"]) == true ? $_GET["page"] : 0;


$cell_height=100/$icon_per_row;
$cell_width=100/$icon_per_col;

//Some parts of the code doesn't set the submenu variable when the user is at the Main Menu which is relected in the "top" variable
$submenu = isset($_GET["submenu"]) == true ? $_GET["submenu"] : "main_menu" ;

//A value of -1 disables the next/previous page arrow
$previous_page = ($current_page != 0) ? $current_page - 1 : -1;
$next_page = (($current_page+1)*$icons_per_page <  count($var[$submenu]["apps"])) ? $current_page + 1 : -1;

//Only enable exit link if your currently not in the main menu
$enable_main_menu_link = $submenu != "main_menu";

$submenu_entry = get_submenu($var,$submenu);

//$menu_title = ($submenu == "main_menu") ? "FORLINX DEMOS P".($current_page+1) : $submenu_entry["Name"]." Submenu p".($current_page+1);
$menu_title = ($submenu == "main_menu") ? "FORLINX DEMOS" : $submenu_entry["Name"];

$start_index = $current_page * $icons_per_page;
$end_index = $start_index + $icons_per_page - 1;

if(count($var[$submenu]["apps"]) - 1 < $end_index)
	$end_index = count($var[$submenu]["apps"]) - 1;
?>

<style type="text/css">
.icons_cell
{
	height:<?php echo $cell_height; ?>%;
	width:<?php echo $cell_width; ?>%;
}
</style>

<?php include "menubar.php"; ?>

<table id = "iconlist" > 

<?php
	for($x = 0,$i = $start_index;$x<$icon_per_row;$x++)
	{
		echo "<tr>";
		for($y = 0;$y<$icon_per_col;$y++,$i++)
		{
			echo "<td class = 'icons_cell' align = 'center' >";

			if($i<=$end_index) 
			{
				$current_app = $var[$submenu]["apps"][$i];
				$img_src = $current_app["Icon"];
				$app_title = $current_app["Name"];
				$type = strtolower($current_app["Type"]);
				$class = "";
				$disable_link = false;
				if($type=="directory")
				{
					$category = $current_app["Category"];
					$link = "submenu.php?submenu=$category";

					if(isset($var[$category]["apps"]) == false)
					{
						$link = "coming_soon.php?submenu=$category";
					}
				}
				elseif($type=="application")
				{
					
					$has_description_page = $current_app["Description_Link"] != -1;

					//This check to see if the application doesn't have a description page. If it doesn't then directly launch the application"
					if($has_description_page == false)
					{	
						$link =  "run_script.php?&submenu=".urlencode($submenu)."&app=".urlencode($app_title);
						
						//Determine if the application is GUI based. If it is then add a class to the link so the javascript code can 
						//manipulate the link if it needs to
						if($var[$submenu]["apps"][$i]["ProgramType"]=="gui")
							$class = "class = 'is_gui_app'";
					}
					else	
						$link =  "app_description.php?submenu=".urlencode($submenu)."&app=".urlencode($app_title);
				}
					
				echo "<a href = '$link' $class><img src= '$img_src' ></a>";
				echo "<p>$app_title</p>";
			}
			echo "</td>";
		}
		echo "</tr>";
	}
	
echo "</table>";
?>

<script>
//Don't launch GUI based application directly if the application is being launched remotely 
//or if the target doesn't have an attached graphic device
if(client_is_host == false || has_graphics == false)
{
	$('.is_gui_app').each(function(index) {
		var link = $(this).attr("href");
		var new_link = link.substr(link.indexOf("&submenu="));
		new_link = "app_description.php?" + new_link;
		$(this).attr("href",new_link);
	});
}
</script>
