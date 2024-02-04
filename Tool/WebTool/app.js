// Create a destination with exchange type is topic and the routing key is gpcoder-stomp
const REGISTER_TOPIC = "/topic/register_topic";
const DATA_TOPIC = "/topic/data_topic";

var client;
var app = {
    printLogDebug: function (message) {
        let div = $('#logDebug');
        div.append($("<code>").text(message));
        div.scrollTop(div.scrollTop() + 10000);
    },
    printReceivedMessage: function (message) {
        let div = $('#logReceivedMessage');
        div.append($("<code>").text(message));
        div.scrollTop(div.scrollTop() + 10000);
    },
    send: function(topic, data) {
        // destination, headers, body
        client.send(topic, { "content-type": "text/plain" }, data);
    },
    subscribe: function() {
        // destination, callback, headers
        client.subscribe(REGISTER_TOPIC, function (response) {
            app.printReceivedMessage(response.body);
        });        
        
        // destination, callback, headers
        client.subscribe(DATA_TOPIC, function (response) {
            app.printReceivedMessage(response.body);
        });

    },
    bindSubmittingForm: function() {
        $('#first form').submit(function () {
            let input = $('#first input');
            let data = input.val();
            app.send(data);
            input.val('');
            return false;
        });
    },
    onConnectCallback: function () {
        app.bindSubmittingForm();
        app.subscribe();
    },
    onErrorCallback: function () {
        console.log('error');
    },
    createRabbitClient: function() {
        // Create STOMP client over websocket
        return Stomp.client('ws://toantrungcloud.com:15674/ws');	
    },
    start: function() {
        // Create RabbitMQ client using STOMP over websocket
        client = app.createRabbitClient(); // asign the created client as global for sending or subscribing messages
         
        // Enable debug
        client.debug = app.printLogDebug;
 
        // username, password, connectCallback, errorCallback, host
        client.connect('Device', 'Device@12345', app.onConnectCallback, app.onErrorCallback, '/');
    }
};
 
// Start application
app.start();

function getData()
{
    // Lấy giá trị từ vùng input
    var inputValue = document.getElementById("deviceID").value;

    if(inputValue.length > 0)
    {
        client.send("/topic/" + inputValue + ".command", { "content-type": "text/plain" }, "{\"command\":\"get_data\"}");
    }
}

function ota()
{
    // Lấy giá trị từ vùng input
    var inputValue_1 = document.getElementById("deviceID_OTA").value;
    var inputValue_2 = document.getElementById("link_OTA").value;

    if(inputValue_1.length > 0 && inputValue_2.length > 0)
    {
        client.send("/topic/" + inputValue_1 + ".command", { "content-type": "text/plain" }, "{\"command\":\"ota\",\"link\":\"" + inputValue_2 + "\"}");
    }
}