/** The MIT License (MIT)

Copyright (c) 2018 David Payne

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

// Additional Contributions:
/* 15 Jan 2019 : Owen Carter : Add psucontrol query via POST api call */

#include "PrusaLinkClient.h"

PrusaLinkClient::PrusaLinkClient(String ApiKey, String server, int port, String user, String pass, boolean psu) {
  updatePrintClient(ApiKey, server, port, user, pass, psu);
}

void PrusaLinkClient::updatePrintClient(String ApiKey, String server, int port, String user, String pass, boolean psu) {
  server.toCharArray(myServer, 100);
  myApiKey = ApiKey;
  myPort = port;
  encodedAuth = "";
  if (user != "") {
    String userpass = user + ":" + pass;
    base64 b64;
    encodedAuth = b64.encode(userpass, true);
  }
  pollPsu = psu;
}

boolean PrusaLinkClient::validate() {
  boolean rtnValue = false;
  printerData.error = "";
  if (String(myServer) == "") {
    printerData.error += "Server address is required; ";
  }
  if (myApiKey == "") {
    printerData.error += "ApiKey is required; ";
  }
  if (printerData.error == "") {
    rtnValue = true;
  }
  return rtnValue;
}

WiFiClient PrusaLinkClient::getSubmitRequest(String apiGetData) {
  WiFiClient printClient;
  printClient.setTimeout(5000);

  Serial.println("Getting PrusaLink Data via GET");
  Serial.println(apiGetData);
  result = "";
  if (printClient.connect(myServer, myPort)) {  //starts client connection, checks for connection
    printClient.println(apiGetData);
    printClient.println("Host: " + String(myServer) + ":" + String(myPort));
    printClient.println("X-Api-Key: " + myApiKey);
    if (encodedAuth != "") {
      printClient.print("Authorization: ");
      printClient.println("Basic " + encodedAuth);
    }
    printClient.println("User-Agent: ArduinoWiFi/1.1");
    printClient.println("Connection: close");
    if (printClient.println() == 0) {
      Serial.println("Connection to " + String(myServer) + ":" + String(myPort) + " failed.");
      Serial.println();
      resetPrintData();
      printerData.error = "Connection to " + String(myServer) + ":" + String(myPort) + " failed.";
      return printClient;
    }
  } 
  else {
    Serial.println("Connection to PrusaLink failed: " + String(myServer) + ":" + String(myPort)); //error message if no client connect
    Serial.println();
    resetPrintData();
    printerData.error = "Connection to PrusaLink failed: " + String(myServer) + ":" + String(myPort);
    return printClient;
  }

  // Check HTTP status
  char status[32] = {0};
  printClient.readBytesUntil('\r', status, sizeof(status));
  if (strcmp(status, "HTTP/1.1 200 OK") != 0 && strcmp(status, "HTTP/1.1 409 CONFLICT") != 0) {
    Serial.print(F("Unexpected response: "));
    Serial.println(status);
    printerData.state = "";
    printerData.error = "Response: " + String(status);
    return printClient;
  }

  // Skip HTTP headers
  char endOfHeaders[] = "\r\n\r\n";
  if (!printClient.find(endOfHeaders)) {
    Serial.println(F("Invalid response"));
    printerData.error = "Invalid response from " + String(myServer) + ":" + String(myPort);
    printerData.state = "";
  }

  return printClient;
}

WiFiClient PrusaLinkClient::getPostRequest(String apiPostData, String apiPostBody) {
  WiFiClient printClient;
  printClient.setTimeout(5000);

  Serial.println("Getting PrusaLink Data via POST");
  Serial.println(apiPostData + " | " + apiPostBody);
  result = "";
  if (printClient.connect(myServer, myPort)) {  //starts client connection, checks for connection
    printClient.println(apiPostData);
    printClient.println("Host: " + String(myServer) + ":" + String(myPort));
    printClient.println("Connection: close");
    printClient.println("X-Api-Key: " + myApiKey);
    if (encodedAuth != "") {
      printClient.print("Authorization: ");
      printClient.println("Basic " + encodedAuth);
    }
    printClient.println("User-Agent: ArduinoWiFi/1.1");
    printClient.println("Content-Type: application/json");
    printClient.print("Content-Length: ");
    printClient.println(apiPostBody.length());
    printClient.println();
    printClient.println(apiPostBody);
    if (printClient.println() == 0) {
      Serial.println("Connection to " + String(myServer) + ":" + String(myPort) + " failed.");
      Serial.println();
      resetPrintData();
      printerData.error = "Connection to " + String(myServer) + ":" + String(myPort) + " failed.";
      return printClient;
    }
  } 
  else {
    Serial.println("Connection to PrusaLink failed: " + String(myServer) + ":" + String(myPort)); //error message if no client connect
    Serial.println();
    resetPrintData();
    printerData.error = "Connection to PrusaLink failed: " + String(myServer) + ":" + String(myPort);
    return printClient;
  }

  // Check HTTP status
  char status[32] = {0};
  printClient.readBytesUntil('\r', status, sizeof(status));
  if (strcmp(status, "HTTP/1.1 200 OK") != 0 && strcmp(status, "HTTP/1.1 409 CONFLICT") != 0) {
    Serial.print(F("Unexpected response: "));
    Serial.println(status);
    printerData.state = "";
    printerData.error = "Response: " + String(status);
    return printClient;
  }

  // Skip HTTP headers
  char endOfHeaders[] = "\r\n\r\n";
  if (!printClient.find(endOfHeaders)) {
    Serial.println(F("Invalid response"));
    printerData.error = "Invalid response from " + String(myServer) + ":" + String(myPort);
    printerData.state = "";
  }

  return printClient;
}

void PrusaLinkClient::getPrinterJobResults() {
  if (!validate()) {
    return;
  }
  //**** get the Printer Job status
  String apiGetData = "GET /api/job HTTP/1.1";
  WiFiClient printClient = getSubmitRequest(apiGetData);
  if (printerData.error != "") {
    return;
  }
  const size_t bufferSize = JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(3) + 2*JSON_OBJECT_SIZE(5) + JSON_OBJECT_SIZE(6) + 710;
  DynamicJsonBuffer jsonBuffer(bufferSize);

  // Parse JSON object
  JsonObject& root = jsonBuffer.parseObject(printClient);
  if (!root.success()) {
    Serial.println("PrusaLink Data Parsing failed: " + String(myServer) + ":" + String(myPort));
    printerData.error = "PrusaLink Data Parsing failed: " + String(myServer) + ":" + String(myPort);
    printerData.state = "";
    return;
  }
  
  printerData.averagePrintTime = (const char*)root["job"]["averagePrintTime"];          //  no equivalent value
  printerData.estimatedPrintTime = (const char*)root["job"]["estimatedPrintTime"];      // ok as is
  printerData.fileName = (const char*)root["job"]["file"]["name"];                      // ok as is
  printerData.fileSize = (const char*)root["job"]["file"]["size"];                      //  no equivalent value
  printerData.lastPrintTime = (const char*)root["job"]["lastPrintTime"];                //  no equivalent value
  printerData.progressCompletion = (const char*)root["progress"]["completion"];         // changed
  printerData.progressCompletion = String(printerData.progressCompletion.toFloat()*100);// PrusaLink sends decimal values
  printerData.progressFilepos = (const char*)root["progress"]["completion"];            // changed, no equivalent value
  printerData.progressPrintTime = (const char*)root["progress"]["printTime"];           // ok
  printerData.progressPrintTimeLeft = (const char*)root["progress"]["printTimeLeft"];   // ok
  printerData.filamentLength = (const char*)root["job"]["filament"]["tool0"]["length"]; // no equivalent value
  printerData.state = (const char*)root["state"];
  
  if (isOperational()) {
    Serial.println("Status: " + printerData.state);  
  } else {
    Serial.println("Printer Not Operational");
  }

  //**** get the Printer Temps and Stat
  apiGetData = "GET /api/printer HTTP/1.1";
  printClient = getSubmitRequest(apiGetData);
  if (printerData.error != "") {
    return;
  }
  const size_t bufferSize2 = 3*JSON_OBJECT_SIZE(2) + 2*JSON_OBJECT_SIZE(3) + JSON_OBJECT_SIZE(9) + 300;
  DynamicJsonBuffer jsonBuffer2(bufferSize2);

  // Parse JSON object
  JsonObject& root2 = jsonBuffer2.parseObject(printClient);
  if (!root2.success()) {
    printerData.isPrinting = false;
    printerData.toolTemp = "";
    printerData.toolTargetTemp = "";
    printerData.bedTemp = "";
    printerData.bedTargetTemp = (const char*)root2["temperature"]["bed"]["target"];     // 
    //new line sw
    Serial.println("FAILED TO PARSE [JsonObject& root2]");
    return;
  }

  String printing = (const char*)root2["state"]["flags"]["printing"];                   // ok as is
  if (printing == "true") {
    printerData.isPrinting = true;
  } else {
    printerData.isPrinting = false;
  }
  printerData.toolTemp = (const char*)root2["temperature"]["tool0"]["actual"];          // ok as is
  printerData.toolTargetTemp = (const char*)root2["temperature"]["tool0"]["target"];    // ok as is
  printerData.bedTemp = (const char*)root2["temperature"]["bed"]["actual"];             // ok as is
  printerData.bedTargetTemp = (const char*)root2["temperature"]["bed"]["target"];       // ok as is

  if (isPrinting()) {
    Serial.println("Status: " + printerData.state + " " + printerData.fileName + "(" + printerData.progressCompletion + "%)");
  }
}

void PrusaLinkClient::getPrinterPsuState() {
  //**** get the PSU state (if enabled and printer operational)
  if (pollPsu && isOperational()) {
    if (!validate()) {
      printerData.isPSUoff = false; // we do not know PSU state, so assume on.
      return;
    }
    String apiPostData = "POST /api/plugin/psucontrol HTTP/1.1";
    String apiPostBody = "{\"command\":\"getPSUState\"}";
    WiFiClient printClient = getPostRequest(apiPostData,apiPostBody);
    if (printerData.error != "") {
      printerData.isPSUoff = false; // we do not know PSU state, so assume on.
      return;
    }
    const size_t bufferSize3 = JSON_OBJECT_SIZE(2) + 300;
    DynamicJsonBuffer jsonBuffer3(bufferSize3);
  
    // Parse JSON object
    JsonObject& root3 = jsonBuffer3.parseObject(printClient);
    if (!root3.success()) {
      printerData.isPSUoff = false; // we do not know PSU state, so assume on
      return;
    }
  
    String psu = (const char*)root3["isPSUOn"];
    if (psu == "true") {
      printerData.isPSUoff = false; // PSU checked and is on
    } else {
      printerData.isPSUoff = true; // PSU checked and is off, set flag
    }
    printClient.stop(); //stop client
  } else {
    printerData.isPSUoff = false; // we are not checking PSU state, so assume on
  }
}

// Reset all PrinterData
void PrusaLinkClient::resetPrintData() {
  printerData.averagePrintTime = "";
  printerData.estimatedPrintTime = "";    // job.job.estimatedPrintTime nnseconds
  printerData.fileName = "";              // job.job.file.name string or job.job.file.display string
  printerData.fileSize = "";
  printerData.lastPrintTime = "";
  printerData.progressCompletion = "";    // job.job.progress.completion 0.nn  or status.job.progress nn.n
  printerData.progressFilepos = "";       // ???? job.job.progress.completion 0.nn ????
  printerData.progressPrintTime = "";     // job.job.progress.printTime nnseconds
  printerData.progressPrintTimeLeft = ""; // job.job.progress.printTimeLeft nnseconds
  printerData.state = "";                 // printer.state.text string
  printerData.toolTemp = "";              // printer.telemetry.temp-nozzle nn.n or printer.temperature.toolN.actual n.nn or status.printer.temp_nozzle nnn.n
  printerData.toolTargetTemp = "";        // printer.temperature.toolN.target n.nn or status.printer.target_nozzle nnn.n
  printerData.filamentLength = "";
  printerData.bedTemp = "";               // printer.telemetry.temp-bed nn.n or printer.bed.actual nnn.n
  printerData.bedTargetTemp = "";         // printer.bed.target nnn.n
  printerData.isPrinting = false;         // printer.state.flags.printing bool
  printerData.isPSUoff = false;
  printerData.error = "";
}

String PrusaLinkClient::getAveragePrintTime(){
  return printerData.averagePrintTime;
}

String PrusaLinkClient::getEstimatedPrintTime() {
  return printerData.estimatedPrintTime;
}

String PrusaLinkClient::getFileName() {
  return printerData.fileName;
}

String PrusaLinkClient::getFileSize() {
  return printerData.fileSize;
}

String PrusaLinkClient::getLastPrintTime(){
  return printerData.lastPrintTime;
}

String PrusaLinkClient::getProgressCompletion() {
  return String(printerData.progressCompletion.toInt());
}

String PrusaLinkClient::getProgressFilepos() {
  return String(printerData.progressFilepos);  
}

String PrusaLinkClient::getProgressPrintTime() {
  return printerData.progressPrintTime;
}

String PrusaLinkClient::getProgressPrintTimeLeft() {
  String rtnValue = printerData.progressPrintTimeLeft;
  if (getProgressCompletion() == "100") {
    rtnValue = "0"; // Print is done so this should be 0 this is a fix for PrusaLink
  }
  return rtnValue;
}

String PrusaLinkClient::getState() {
  return printerData.state;
}

boolean PrusaLinkClient::isPrinting() {
  return printerData.isPrinting;
}

boolean PrusaLinkClient::isPSUoff() {
  return printerData.isPSUoff;
}

boolean PrusaLinkClient::isOperational() {
  boolean operational = false;
  if (printerData.state == "Operational" || isPrinting()) {
    operational = true;
  }
  return operational;
}

String PrusaLinkClient::getTempBedActual() {
  return printerData.bedTemp;
}

String PrusaLinkClient::getTempBedTarget() {
  return printerData.bedTargetTemp;
}

String PrusaLinkClient::getTempToolActual() {
  return printerData.toolTemp;
}

String PrusaLinkClient::getTempToolTarget() {
  return printerData.toolTargetTemp;
}

String PrusaLinkClient::getFilamentLength() {
  return printerData.filamentLength;
}

String PrusaLinkClient::getError() {
  return printerData.error;
}

String PrusaLinkClient::getValueRounded(String value) {
  float f = value.toFloat();
  int rounded = (int)(f+0.5f);
  return String(rounded);
}

String PrusaLinkClient::getPrinterType() {
  return printerType;
}

int PrusaLinkClient::getPrinterPort() {
  return myPort;
}

String PrusaLinkClient::getPrinterName() {
  return printerData.printerName;
}

void PrusaLinkClient::setPrinterName(String printer) {
  printerData.printerName = printer;
}
