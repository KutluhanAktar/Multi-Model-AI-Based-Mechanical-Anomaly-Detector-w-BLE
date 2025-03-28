<?php

include_once "assets/class.php";

// Define the new 'anomaly ' object:
$anomaly  = new _main();
$anomaly->__init__($conn);

# Get the current date and time.
$date = date("Y_m_d_H_i_s");

# Define the resulting image file name.
$img_file = "%s_".$date;

// If FireBeetle 2 ESP32-S3 sends the model detection results, save the received information to the detections MySQL database table.
if(isset($_GET["results"]) && isset($_GET["class"]) && isset($_GET["x"])){
	$img_file = sprintf($img_file, $_GET["class"]);
	if($anomaly->insert_new_results($date, $img_file.".jpg", $_GET["class"])){
		echo "Detection Results Saved Successfully!";
	}else{
		echo "Database Error!";
	}
}

// If FireBeetle 2 ESP32-S3 transfers a resulting image after running the object detection model, save the received raw image buffer (RGB565) as a TXT file to the detections folder.
if(!empty($_FILES["resulting_image"]['name'])){
	// Image File:
	$received_img_properties = array(
	    "name" => $_FILES["resulting_image"]["name"],
	    "tmp_name" => $_FILES["resulting_image"]["tmp_name"],
		"size" => $_FILES["resulting_image"]["size"],
		"extension" => pathinfo($_FILES["resulting_image"]["name"], PATHINFO_EXTENSION)
	);
	
    // Check whether the uploaded file's extension is in the allowed file formats.
	$allowed_formats = array('jpg', 'png', 'bmp', 'txt');
	if(!in_array($received_img_properties["extension"], $allowed_formats)){
		echo 'FILE => File Format Not Allowed!';
	}else{
		// Check whether the uploaded file size exceeds the 5 MB data limit.
		if($received_img_properties["size"] > 5000000){
			echo "FILE => File size cannot exceed 5MB!";
		}else{
			// Save the uploaded file (image).
			move_uploaded_file($received_img_properties["tmp_name"], "./detections/".$img_file.".".$received_img_properties["extension"]);
			echo "FILE => Saved Successfully!";
		}
	}
	
	// Convert the recently saved RGB565 buffer (TXT file) to a JPG image file by executing the rgb565_converter.py file.
	// Transmit the passed bounding box measurements (query parameters) as Python Arguments.
	$raw_convert = shell_exec('python "C:\Users\kutlu\New E\xampp\htdocs\mechanical_anomaly_detector\detections\rgb565_converter.py" --x='.$_GET["x"].' --y='.$_GET["y"].' --w='.$_GET["w"].' --h='.$_GET["h"]);
	print($raw_convert);

	// After generating the JPG file, remove the converted TXT file from the server.
	unlink("./detections/".$img_file.".txt");
	
	// After saving the generated JPG file and the received model detection results to the MySQL database table successfully,
	// send an SMS to the given user phone number via Twilio in order to inform the user of the latest detection results, including the resulting image.
	$message_body = "⚠️🚨⚙️ Anomaly Detected ⚠️🚨⚙️"
	                ."\n\r\n\r📌 Faulty Part: ".$_GET["class"]
			        ."\n\r\n\r⏰ Date: ".$date
				    ."\n\r\n\r🌐 🖼️ http://192.168.1.22/mechanical_anomaly_detector/detections/images/".$img_file.".jpg"
				    ."\n\r\n\r📲 Please refer to the Android application to inspect the resulting image index.";
	$anomaly->Twilio_send_SMS($message_body);
}

?>