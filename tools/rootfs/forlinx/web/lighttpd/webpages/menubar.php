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
/*
----Note----
This file is meant to be included in the php file of another page. Therefore, some variables used in this file is assumed to be set within the page that includes this file.
You will notice that these variables are always checked to make sure they are set (set from the other page). If the variable isn't set then the variable is given a default value
which usually is meant to deactivate something. This is done to allow pages that are including this file to only set the variables they care about
*/

//Some parts of the code doesn't set the submenu variable when the user is at the Main Menu which is relected in the "top" variable
$submenu = isset($_GET["submenu"]) == true ? $_GET["submenu"] : "main_menu" ;

$previous_page = isset($previous_page) == true ? $previous_page : -1;
$enable_previous_link = $previous_page >= 0;

$next_page = isset($next_page) == true ? $next_page : -1;
$enable_next_link = $next_page > 0;

$enable_main_menu_link = isset($enable_main_menu_link) == true ? $enable_main_menu_link : false;
?>

<table width = "100%" style = "margin-bottom:10px;">
	<tr>
		<td  align = "left" >
			<?php 
				$link = "submenu.php?submenu=".$submenu."&page=".$previous_page;
				$css_string = ($enable_previous_link == false) ? "hide_link" : "";

				echo "<a href = '$link' class = 'previous_arrow $css_string' ><img id = 'previous_arrow_img' src= 'images/left-arrow-icon.png'></a>";

				//Added these two html elements so that each side will be even
				echo "<a href = '#'  class = 'hide_link'><img id = 'exit_button_img' src= 'images/multi-icon.png'></a>";
				echo "<a href = '#'  class = 'hide_link'><img id = 'next_arrow_img' src= 'images/right-arrow-icon.png'></a>";
			?>

		</td>
		<td  align = "center" id = "banner" >
			<?php  
				echo "<table> <tr>";
				echo "<td>";
				echo "<img id = 'logo_img' src= 'images/logo.png'>";
				echo "</td><td>";
				echo $menu_title;
				echo "</td>";
				echo "</tr> </table>";
			?>
		</td>
		<td  align = "right" >
			<?php
				$css_string = ($enable_main_menu_link == false) ? "hide_link" : "";

				echo "<a  id = 'back_link' class = '$css_string' href = '#'><img id = 'back_button_img' src= 'images/back-arrow-icon.png'></a>";
				echo "<a  id = 'main_menu_link' class = '$css_string' href = 'submenu.php?submenu=main_menu&page=0'  ><img id = 'exit_button_img' src= 'images/multi-icon.png'></a>";

				$css_string = ($enable_next_link == true) ? "" : "hide_link";

				$link = "submenu.php?submenu=".$submenu."&page=".$next_page;

				echo "<a href = '$link' class = '$css_string' ><img id = 'next_arrow_img' src= 'images/right-arrow-icon.png'></a>";

			?>
		</td>
	</tr>
</table>
