#include <WiFi.h>
#include <WebServer.h>

const char* ssid = "LoRaDashboard2";
const char* password = "12345678";

WebServer server(80);
#include <SPI.h>
#include <LoRa.h>

#define SS 5
#define RST 14
#define DIO0 2

#define NODE_ID 2  // CHANGE TO 1 ON OTHER ESP32
int nextMessageID = 1;
const unsigned long ACK_TIMEOUT = 1000;
int receivedIDs[50];
int receivedCount = 0;
int deliveredCount = 0;
struct HistoryMessage
{
    int id;
    int from;
    int to;
    int rssi;
    String text;
    String status;
};

HistoryMessage history[100];
int historyCount = 0;
struct QueuedMessage
{
  int id;
  int priority;
  int destination;
  String text;

  bool waitingAck;
  unsigned long sendTime;
  unsigned long timestamp;
};

QueuedMessage queue[120];
int queueSize = 0;

unsigned long lastRetry = 0;
const unsigned long RETRY_INTERVAL = 1000;
void addToQueue(int destination, String text);
void sendMessage(int index);
void handleRoot()
{
   String page = "";

page += R"rawliteral(
<style>
body{
background:#f4f6f8;
font-family:Arial;
}
.card{
background:white;
padding:15px;
margin:10px;
border-radius:12px;
box-shadow:0 2px 10px rgba(0,0,0,.1);
}
.chat{
height:350px;
overflow-y:auto;
background:white;
padding:10px;
border-radius:12px;
scroll-behavior:smooth;
}
.sent{
background:#dcf8c6;
padding:8px;
margin:5px;
border-radius:10px;
text-align:right;
}
.received{
background:#ffffff;
padding:8px;
margin:5px;
border-radius:10px;
}
input[type=submit]{
background:#1976d2;
color:white;
border:none;
padding:10px 20px;
border-radius:8px;
cursor:pointer;
}

textarea{
width:100%;
height:80px;
}

input{
width:100%;
padding:8px;
}
table{
width:100%;
border-collapse:collapse;
}
td,th{
border:1px solid #ddd;
padding:8px;
}
h1{
text-align:center;
color:#1976d2;
}

.card{
max-width:1200px;
margin:auto;
margin-bottom:15px;
}
</style>
)rawliteral";
page += "<h1> LoRa Disaster Network</h1>";

page += "<div class='card'>";
page += "<h2>Node Status</h2>";

page += "Node ID: ";
page += NODE_ID;
page += "<br>";

page += "Queue Size: ";
page += queueSize;
page += "<br>";

page += "Delivered: ";
page += deliveredCount;
page += "<br>";

page += "Received: ";
page += receivedCount;
page += "<br>";

page += "</div>";
page += "<div class='card'>";
page += "<h2>Live Chat</h2>";
page += "<div class='chat'>";

for(int i=0;i<historyCount;i++)
{
    if(history[i].from == NODE_ID)
        page += "<div class='sent'>";
    else
        page += "<div class='received'>";

    page += history[i].text;
    page += "<br>RSSI: ";
    page += history[i].rssi;
    page += "<br>Status: ";
    page += history[i].status;

    page += "</div>";
}

page += "</div>";
page += "</div>";
page += "<div class='card'>";
page += "<h2>Pending Queue</h2>";

page += "<table>";
page += "<tr><th>ID</th><th>FROM</th><th>TO</th><th>RSSI</th><th>STATUS</th><th>MESSAGE</th></tr>";

for(int i=0;i<historyCount;i++)
{
    page += "<tr>";

    page += "<td>" + String(history[i].id) + "</td>";
    page += "<td>" + String(history[i].from) + "</td>";
    page += "<td>" + String(history[i].to) + "</td>";
    page += "<td>" + String(history[i].rssi) + "</td>";
    page += "<td>" + history[i].status + "</td>";
    page += "<td>" + history[i].text + "</td>";

    page += "</tr>";
}

page += "</table>";
page += "</div>";
page += "<table>";

for(int i=0;i<queueSize;i++)
{
    page += "<tr>";

    page += "<td>" + String(queue[i].id) + "</td>";

    String priColor;

    if(queue[i].priority == 1)
        priColor = "red";
    else if(queue[i].priority == 2)
        priColor = "orange";
    else if(queue[i].priority == 3)
        priColor = "gold";
    else
        priColor = "green";

    page += "<td><span style='background:";
    page += priColor;
    page += ";color:white;padding:5px 10px;border-radius:15px;'>";

    if(queue[i].priority == 1)
        page += "SOS";
    else if(queue[i].priority == 2)
        page += "FIRE";
    else if(queue[i].priority == 3)
        page += "MEDICAL";
    else
        page += "NORMAL";

    page += "</span></td>";

    page += "<td>" + String(queue[i].destination) + "</td>";
    page += "<td>" + queue[i].text + "</td>";
    page += "<td>" +
        String((millis()-queue[i].timestamp)/1000) +
        " sec</td>";

    page += "</tr>";
}

page += "</table>";
page += "</div>";
page += "<div class='card'>";

page += "<h2>Send Message</h2>";

page += "<form action='/send'>";

page += "Destination:<br>";
page += "<input name='dest'><br><br>";

page += "Message:<br>";
page += "<textarea name='msg'></textarea><br><br>";

page += "<input type='submit' value='Send'>";

page += "</form>";

page += "</div>";
server.send(200, "text/html", page);
}

void handleSend()
{
    int dest = server.arg("dest").toInt();
    String msg = server.arg("msg");

    addToQueue(dest,msg);
    
    sendMessage(queueSize - 1);

    handleRoot();
}
void setup()
{
  Serial.begin(9600);

  LoRa.setPins(SS, RST, DIO0);

  if (!LoRa.begin(433E6))
  {
    Serial.println("LoRa init failed!");
    while (1);
  }

  LoRa.setSpreadingFactor(7);
  LoRa.setSignalBandwidth(125E3);

  LoRa.receive();
  WiFi.softAP(ssid, password);
  server.on("/", handleRoot);
server.on("/send", handleSend);

server.begin();
Serial.print("IP Address: ");
Serial.println(WiFi.softAPIP());
  Serial.println("--------------------------------");
  Serial.print("NODE ");
  Serial.print(NODE_ID);
  Serial.println(" READY");
  Serial.println("STORE|DEST|MESSAGE");
  Serial.println("Example: 1|SOS: Help");
  Serial.println("--------------------------------");
}

int getPriority(String msg)
{
  msg.toUpperCase();

  if (msg.startsWith("SOS")) return 1;
  if (msg.startsWith("FIRE")) return 2;
  if (msg.startsWith("MEDICAL")) return 3;

  return 4;
}

void sortQueue()
{
  for (int i = 0; i < queueSize - 1; i++)
  {
    for (int j = i + 1; j < queueSize; j++)
    {
      if (queue[j].priority < queue[i].priority)
      {
        QueuedMessage temp = queue[i];
        queue[i] = queue[j];
        queue[j] = temp;
      }
    }
  }
}

void addToQueue(int destination, String text)
{
  if(queueSize >= 20)
  {
    Serial.println("QUEUE FULL");
    return;
  }

  queue[queueSize].id = nextMessageID++;
  queue[queueSize].priority = getPriority(text);
  queue[queueSize].destination = destination;
  queue[queueSize].text = text;

  queue[queueSize].waitingAck = false;
  queue[queueSize].sendTime = 0;
  queue[queueSize].timestamp = millis();
  queueSize++;
  sortQueue();
  Serial.print("MESSAGE STORED ID=");
  Serial.println(queue[queueSize - 1].id);
}

void printQueue()
{
  Serial.println();
  Serial.println("===== QUEUE =====");

  for (int i = 0; i < queueSize; i++)
  {
    Serial.print("ID=");
Serial.print(queue[i].id);
Serial.print(" P");
Serial.print(queue[i].priority);
Serial.print(" -> ");
Serial.println(queue[i].text);
Serial.print("   AGE(ms): ");
Serial.println(millis() - queue[i].timestamp);
  }

  if (queueSize == 0)
  {
    Serial.println("EMPTY");
  }

  Serial.println("=================");
  Serial.println();
}
void removeMessage(int id)
{
  for(int i=0;i<queueSize;i++)
  {
    if(queue[i].id == id)
    {
      for(int j=i;j<queueSize-1;j++)
      {
        queue[j] = queue[j+1];
      }

      queueSize--;
      deliveredCount++;
      Serial.print("DELIVERED ID=");
      Serial.println(id);
      return;
    }
  }
}
void sendMessage(int index)
{
  String packet =
    "MSG|" +
    String(NODE_ID) + "|" +
    String(queue[index].destination) + "|" +
    String(queue[index].id) + "|" +
    queue[index].text;

  queue[index].waitingAck = true;
  queue[index].sendTime = millis();

  LoRa.idle();
  LoRa.beginPacket();
  LoRa.print(packet);
  LoRa.endPacket();
  LoRa.receive();

  Serial.print("TX -> ");
  Serial.println(packet);
  if(historyCount < 100)
{
    history[historyCount].id = queue[index].id;
    history[historyCount].from = NODE_ID;
    history[historyCount].to = queue[index].destination;
    history[historyCount].rssi = 0;
    history[historyCount].text = queue[index].text;
    history[historyCount].status = "SENT";

    historyCount++;
}
}

void sendACK(int destination, int msgID)
{
  String ack =
    "ACK|" +
    String(NODE_ID) + "|" +
    String(destination) + "|" +
    String(msgID);

  LoRa.idle();
  LoRa.beginPacket();
  LoRa.print(ack);
  LoRa.endPacket();
  LoRa.receive();

  Serial.print("ACK SENT ID=");
  Serial.println(msgID);
}
void processPacket(String packet)
{
  if (packet.startsWith("MSG|"))
{
    int p1 = packet.indexOf('|');
    int p2 = packet.indexOf('|', p1 + 1);
    int p3 = packet.indexOf('|', p2 + 1);
    int p4 = packet.indexOf('|', p3 + 1);

    int sender =
      packet.substring(p1 + 1, p2).toInt();

    int receiver =
      packet.substring(p2 + 1, p3).toInt();

    int msgID =
      packet.substring(p3 + 1, p4).toInt();
    for(int i=0;i<receivedCount;i++)
   {
    if(receivedIDs[i] == msgID)
    {
        sendACK(sender, msgID);
        return;
    }
  }
    String text =
    packet.substring(p4 + 1);

if (receiver != NODE_ID)
    return;

if(historyCount < 100)
{
    history[historyCount].id = msgID;
    history[historyCount].from = sender;
    history[historyCount].to = receiver;
    history[historyCount].rssi = LoRa.packetRssi();
    history[historyCount].text = text;
    history[historyCount].status = "RECEIVED";

    historyCount++;
}
    if(receivedCount < 50)
{
    receivedIDs[receivedCount++] = msgID;
}
    Serial.println();
    Serial.println("===== MESSAGE =====");
    Serial.print("FROM : ");
    Serial.println(sender);

    Serial.print("TEXT : ");
    Serial.println(text);

    Serial.print("PRIORITY : ");
    Serial.println(getPriority(text));

    Serial.print("RSSI : ");
    Serial.println(LoRa.packetRssi());
    
    Serial.println("===================");
    Serial.println();

    delay(100);

    sendACK(sender, msgID);
  }
    else if (packet.startsWith("ACK|"))
{
    int p1 = packet.indexOf('|');
    int p2 = packet.indexOf('|', p1 + 1);
    int p3 = packet.indexOf('|', p2 + 1);

    int sender =
      packet.substring(p1 + 1, p2).toInt();

    int receiver =
      packet.substring(p2 + 1, p3).toInt();

    int msgID =
      packet.substring(p3 + 1).toInt();

    if(receiver != NODE_ID)
      return;

    Serial.println();
    Serial.println("***** ACK RECEIVED *****");
    Serial.print("FROM NODE ");
    Serial.println(sender);

    removeMessage(msgID);
    for(int i=0;i<historyCount;i++)
{
    if(history[i].id == msgID)
    {
        history[i].status = "DELIVERED";
    }
}

    Serial.println("************************");
    Serial.println();
}

}
void loop()
{
  server.handleClient();
  if (millis() - lastRetry > RETRY_INTERVAL)
  {
    lastRetry = millis();

    if (queueSize > 0)
    {
      sortQueue();

      Serial.println("Retrying queued messages...");

      for (int i = 0; i < queueSize; i++)
      {
        
          sendMessage(i);
        
      }
    }
  }
for(int i=0;i<queueSize;i++)
{
    if(queue[i].waitingAck)
    {
        if(millis() - queue[i].sendTime > ACK_TIMEOUT)
        {
            Serial.print("NO ACK -> ");
            Serial.println(queue[i].id);

            queue[i].waitingAck = false;
        }
    }
}
  int packetSize = LoRa.parsePacket();

  if (packetSize)
  {
    String incoming = "";

    while (LoRa.available())
    {
      incoming += (char)LoRa.read();
    }

    processPacket(incoming);
  }

  if (Serial.available())
  {
    String input =
      Serial.readStringUntil('\n');

    input.trim();

    if (input == "QUEUE")
    {
      printQueue();
      return;
    }
    if(input == "STATUS")
   {
    Serial.print("NODE ID: ");
    Serial.println(NODE_ID);

    Serial.print("QUEUE SIZE: ");
Serial.println(queueSize);

Serial.print("NEXT MSG ID: ");
Serial.println(nextMessageID);

Serial.print("RECEIVED IDS: ");
Serial.println(receivedCount);
  Serial.print("DELIVERED: ");
Serial.println(deliveredCount);
Serial.print("QUEUE USED: ");
Serial.print(queueSize);
Serial.println("/20");
    return;
    }
    if(input == "HELP")
{
    Serial.println("QUEUE");
    Serial.println("STATUS");
    Serial.println("CLEAR");
    Serial.println("STORE|message");
    Serial.println("DEST|message");
    return;
}
    if(input == "CLEAR")
   {
    queueSize = 0;
    receivedCount = 0;
    Serial.println("QUEUE CLEARED");

    return;
   }
    if (input.startsWith("STORE|"))
{
    int p = input.indexOf('|', 6);

    if(p == -1)
    {
        Serial.println("Use: STORE|DEST|MESSAGE");
        return;
    }

    int destination =
        input.substring(6, p).toInt();

    String msg =
        input.substring(p + 1);

    addToQueue(destination, msg);

    printQueue();

    return;
}

    int sep = input.indexOf('|');

    if (sep == -1)
    {
      Serial.println("Use format:");
      Serial.println("1|SOS: Help");
      return;
    }

    int destination =
      input.substring(0, sep).toInt();

    String message =
      input.substring(sep + 1);

    addToQueue(destination, message);
    sendMessage(queueSize - 1);
  }

}