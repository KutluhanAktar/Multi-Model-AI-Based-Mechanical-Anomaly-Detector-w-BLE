<?php

// Include the Twilio PHP Helper Library. 
require_once 'twilio-php-main/src/Twilio/autoload.php';
use Twilio\Rest\Client;

// Define the _main class and its functions:
class _main {
	public $conn;
	private $twilio;
	
	public function __init__($conn){
		$this->conn = $conn;
		// Define the Twilio account information and object.
		$_sid    = "<__SID__>";
        $token  = "<__TOKEN__>";
        $this->twilio = new Client($_sid, $token);
		// Define the user and the Twilio-verified phone numbers.
		$this->user_phone = "+___________";
		$this->from_phone = "+___________";
	}
	
    // Database -> Insert Model Detection Results:
	public function insert_new_results($date, $img_name, $class){
		$sql_insert = "INSERT INTO `detections`(`date`, `img_name`, `class`) 
		               VALUES ('$date', '$img_name', '$class');"
			          ;
		if(mysqli_query($this->conn, $sql_insert)){ return true; } else{ return false; }
	}
	
	// Retrieve all model detection results and the assigned resulting image names from the detections database table, transferred by FireBeetle 2 ESP32-S3.
	public function get_model_results(){
		$date=[]; $class=[]; $img_name=[];
		$sql_data = "SELECT * FROM `detections` ORDER BY `id` DESC";
		$result = mysqli_query($this->conn, $sql_data);
		$check = mysqli_num_rows($result);
		if($check > 0){
			while($row = mysqli_fetch_assoc($result)){
				array_push($date, $row["date"]);
				array_push($class, $row["class"]);
				array_push($img_name, $row["img_name"]);
			}
			return array($date, $class, $img_name);
		}else{
			return array(["Not Found!"], ["Not Found!"], ["waiting.png"]);
		}
	}
	
	// Send an SMS to the registered phone number via Twilio so as to inform the user of the model detection results.
	public function Twilio_send_SMS($body){
		// Configure the SMS object.
        $sms_message = $this->twilio->messages
			->create($this->user_phone,
				array(
					   "from" => $this->from_phone,
                       "body" => $body
                     )
                );
		// Send the SMS.
		echo("SMS SID: ".$sms_message->sid);	  
	}
}

// Define database and server settings:
$server = array(
	"name" => "localhost",
	"username" => "root",
	"password" => "",
	"database" => "mechanical_anomaly"
);

$conn = mysqli_connect($server["name"], $server["username"], $server["password"], $server["database"]);

?>