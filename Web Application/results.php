<?php

include_once "assets/class.php";

// Define the new 'anomaly ' object:
$anomaly  = new _main();
$anomaly->__init__($conn);

// Obtain the latest model detection results from the detections MySQL database table, including the resulting image names.
$date=[]; $class=[]; $img_name=[];
list($date, $class, $img_name) = $anomaly->get_model_results();
// Print the retrieved results as a list separated by commas.
$web_app_img_path = "http://192.168.1.22/mechanical_anomaly_detector/detections/images/";
$data_packet = "";
for($i=0;$i<3;$i++){
	if(isset($date[$i])){
		$data_packet .= $class[$i].",".$date[$i].",".$web_app_img_path.$img_name[$i].",";
	}else{
		$data_packet .= "Not Found!,Not Found!,".$web_app_img_path."waiting.png,";
	}
}

echo($data_packet);

?>