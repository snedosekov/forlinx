<?php

$cachefile = "cache".$_SERVER['PHP_SELF']."?".$_SERVER['QUERY_STRING'];

function start_caching()
{
	//Use the global $cachefile variable
	global $cachefile;

	//There is a random string to the end of the $_GET Query String to
	//prevent IE from caching the Ajax request. The below line removes the random portion
	//of the query so we can cache the page properly in php
	if(stripos($cachefile, "&rand=")==true)
		$cachefile = substr($cachefile,0,stripos($cachefile, "&rand="));

	if (file_exists($cachefile)) 
	{
		// the page has been cached from an earlier request
		// output the contents of the cache file
		include($cachefile); 
		// exit the script, so that the rest isnt executed
		exit;
	}
	else
		ob_start();
}

function get_application($dot_desktop_array,$submenu,$app_name)
{
	for($i = 0;$i<count($dot_desktop_array[$submenu]["apps"]);$i++)
	{
		if($dot_desktop_array[$submenu]["apps"][$i]["Name"]==$app_name)
			return 	$dot_desktop_array[$submenu]["apps"][$i];
	}
	return NULL;
}

function end_caching()
{
	//Use the global $cachefile variable
	global $cachefile;

	//Disable Caching on Description Page
	// open the cache file "cache/home.html" for writing
	$fp = fopen($cachefile, 'w');
	// save the contents of output buffer to the file
	fwrite($fp, ob_get_contents());
	// close the file
	fclose($fp);
	// Send the output to the browser
	ob_end_flush();
}

function get_submenu($dot_desktop_array,$submenu_name)
{
	foreach ($dot_desktop_array as $k => $v) 
	{
		for($j = 0;$j<count($v["apps"]);$j++)
		{	
			$current_entry = $v["apps"][$j];

			if($current_entry["Type"]=="directory" && $current_entry["Category"] == $submenu_name)
				return $current_entry;
		}
	}
	return NULL;
}

function read_desktop_file()
{
	if(file_exists("json.txt") == true && filesize("json.txt") != 0)
	{
		$handle = fopen("json.txt", "rb");
		$contents = fread($handle,filesize("json.txt"));
		fclose($handle);

		return json_decode($contents,true);
	}
	else
		return null;
}

?>
