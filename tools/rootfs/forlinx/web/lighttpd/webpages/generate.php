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
function get_contents($field_name,$filestring)
{	
	$pos = 	stripos($filestring,$field_name."=");
	
	if($pos != false)
	{
		$pos += strlen($field_name."=");
		$newlinepos = stripos($filestring,"\n",$pos);

		if($newlinepos == -1)
			$newlinepos = stripos($filestring,"\r",$pos);

		$returnedstring = substr($filestring,$pos,$newlinepos-$pos);
		return $returnedstring;
	} 
	return -1;
}

system("find  -name '*.desktop' -print > catdesktop.txt");

if(filesize("catdesktop.txt") == 0)
{
	echo "No .desktop files found";
	return;
}

$handle = fopen("catdesktop.txt", "rb");
$contents = fread($handle,filesize("catdesktop.txt"));
fclose($handle);
unlink('catdesktop.txt');

$contents = explode("\n",$contents);

for($x = 0;$x<count($contents)&&strlen($contents[$x])>0;$x++)
{
	$handle = fopen($contents[$x], "rb");
	$dotdesktop = fread($handle,filesize($contents[$x]));
	fclose($handle);
	
	$top["Name"] = get_contents("Name",$dotdesktop);

	if(get_contents("X-MATRIX-DisplayPriority",$dotdesktop)!=-1)
		$top["Order"] = get_contents("X-MATRIX-DisplayPriority",$dotdesktop);
	else
		$top["Order"] = 999;

	$icon_path = get_contents("Icon",$dotdesktop);
	
	$webserver_root = "/forlinx/web/lighttpd/webpages/";


	$top["Icon"] = substr($icon_path,strlen($webserver_root));

	$type = strtolower(get_contents("Type",$dotdesktop));

	$top["Type"] = $type;

	$category = get_contents("Categories",$dotdesktop);

	$category =  trim(strtolower($category));

	if($type == "directory")
	{
		$top["Category"] = get_contents("X-MATRIX-CategoryTarget",$dotdesktop);

	}elseif($type == "application")
	{
		$top["Exec"] = get_contents("Exec",$dotdesktop);

		$top["ProgramType"] = get_contents("ProgramType",$dotdesktop);
		
		$top["Description_Link"] = get_contents("X-MATRIX-Description",$dotdesktop);

		$top["Lock"] = get_contents("X-MATRIX-LOCK",$dotdesktop);
	} 

	if($category == -1)
		$application["main_menu"]["apps"][] = $top;	
	else
		$application[$category]["apps"][] = $top;

	unset($top);
}

function cmp($a, $b)
{
	if($a["Order"] < $b["Order"])
		return -1;
	elseif($a["Order"] == $b["Order"])
		return 0;
	elseif($a["Order"] > $b["Order"])
		return 1;
}

foreach ($application as $key => $value) {
    usort($application[$key]["apps"], "cmp");
}

$ourFileName = "json.txt";
$ourFileHandle = fopen($ourFileName, 'w') or die("can't open file");
fwrite($ourFileHandle,json_encode($application));
fclose($ourFileHandle);
?>

