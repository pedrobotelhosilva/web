<?php
echo "Content-type: text/html\n\n";
echo "<html><body>";
echo "<h1>POST Data from test2.php</h1>";
if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    echo "<p>Posted Data:</p>";
    echo "<pre>";
    print_r($_POST);
    echo "</pre>";
} else {
    echo "<p>No POST data received</p>";
}
echo "</body></html>";
?>
