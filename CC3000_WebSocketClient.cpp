/*
 CC3000_WebsocketClient, a websocket client for Arduino and the CC3000 WiFi chip
 
 Copyright 2014 Marco Schwartz
 http://marcoschwartz.com

 Based on the code of Kevin Rohling
 Copyright 2011 Kevin Rohling
 http://kevinrohling.com
 
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 */

// Include libraries
#include <CC3000_WebSocketClient.h>

CC3000_WebSocketClient::CC3000_WebSocketClient(char * protocol){
  _protocol = protocol;
}

byte CC3000_WebSocketClient::nextByte() {
  //while(_client.available() == 0);
  byte b = _client.read();
  //Serial.print(b);
  return b;
}

void CC3000_WebSocketClient::onMessage(OnMessage fn) {
  _onMessage = fn;
}

void CC3000_WebSocketClient::onOpen(OnOpen fn) {
  _onOpen = fn;
}

void CC3000_WebSocketClient::onClose(OnClose fn) {
  _onClose = fn;
}

void CC3000_WebSocketClient::onError(OnError fn) {
  _onError = fn;
}

void CC3000_WebSocketClient::processMonitor() {
if (_client.available()) {
    //Serial.print("Reading data ...");
    byte hdr = nextByte();
    bool fin = hdr & 0x80;
    
    int opCode = hdr & 0x0F;
    
    hdr = nextByte();
    bool mask = hdr & 0x80;
    int len = hdr & 0x7F;
    if(len == 126) {
      len = nextByte();
      len <<= 8;
      len += nextByte();
    } else if (len == 127) {
      len = nextByte();
      for(int i = 0; i < 7; i++) { // NOTE: This may not be correct.  RFC 6455 defines network byte order(??). (section 5.2)
        len <<= 8;
        len += nextByte();
      }
    }
    
    if(mask) { // skipping 4 bytes for now.
      for(int i = 0; i < 4; i++) {
        nextByte();
      }
    }
    
    if(mask) {

      
      if(_onError != NULL) {
        _onError(*this, "Masking not supported");
      }
      free(_packet);
      return;
    }
    
    if(!fin) {
      if(_packet == NULL) {
        _packet = (char*) malloc(len);
        for(int i = 0; i < len; i++) {
          _packet[i] = nextByte();
        }
        _packetLength = len;
        _opCode = opCode;
      } else {
        int copyLen = _packetLength;
        _packetLength += len;
        char *temp = _packet;
        _packet = (char*)malloc(_packetLength);
        for(int i = 0; i < _packetLength; i++) {
          if(i < copyLen) {
            _packet[i] = temp[i];
          } else {
            _packet[i] = nextByte();
          }
        }
        free(temp);
      }
      return;
    }
    
    if(_packet == NULL) {
      _packet = (char*) malloc(len + 1);
      for(int i = 0; i < len; i++) {
        _packet[i] = nextByte();
      }
      _packet[len] = 0x0;
    } else {
      int copyLen = _packetLength;
      _packetLength += len;
      char *temp = _packet;
      _packet = (char*) malloc(_packetLength + 1);
      for(int i = 0; i < _packetLength; i++) {
        if(i < copyLen) {
          _packet[i] = temp[i];
        } else {
          _packet[i] = nextByte();
        }
      }
      _packet[_packetLength] = 0x0;
      free(temp);
    }
    
    if(opCode == 0 && _opCode > 0) {
      opCode = _opCode;
      _opCode = 0;
    }
    
    switch(opCode) {
      case 0x00:
        
        break;
        
      case 0x01:
        
        if (_onMessage != NULL) {
          _onMessage(*this, _packet);
        }
        break;
        
      case 0x02:
        
        if(_onError != NULL) {
          _onError(*this, "Binary Messages not supported");
        }
        break;
        
      case 0x09:
        
        _client.write(0x8A);
        _client.write(byte(0x00));
        break;
        
      case 0x0A:
        
        break;
        
      case 0x08:
        
        unsigned int code = ((byte)_packet[0] << 8) + (byte)_packet[1];
        
        if(_onClose != NULL) {
          _onClose(*this, code, (_packet + 2));
        }
        _client.stop();
        break;
    }
    free(_packet);
    _packet = NULL;
  }
}

void CC3000_WebSocketClient::monitor(Adafruit_CC3000& cc3000, char * server, int port){

  if(!_canConnect) {
    return;
  }
  
  if(_reconnecting) {
    return;
  }
  
  if(!_client.connected() && millis() > _retryTimeout) {
    _retryTimeout = millis() + RETRY_TIMEOUT;
    _reconnecting = true;
    connect(cc3000, server, port);
    _reconnecting = false;
    return;
  }

  processMonitor();

}

void CC3000_WebSocketClient::monitor(Adafruit_CC3000& cc3000, uint32_t ip, int port){

  if(!_canConnect) {
    return;
  }
  
  if(_reconnecting) {
    return;
  }
  
  if(!_client.connected() && millis() > _retryTimeout) {
    _retryTimeout = millis() + RETRY_TIMEOUT;
    _reconnecting = true;
    connect(cc3000, ip, port);
    _reconnecting = false;
    return;
  }

  processMonitor();

}

void CC3000_WebSocketClient::connect(Adafruit_CC3000& cc3000, uint32_t ip, int port){

  // Set parameters
  _retryTimeout = millis();
  _canConnect = true;
  _server = "server";

  // Get IP
  _ip = ip;
  
  // Connect
  _client = cc3000.connectTCP(_ip, port);

  // Sending Handshake
  sendHandshake();

  // Reading Handshake
  readHandshake();
}

void CC3000_WebSocketClient::connect(Adafruit_CC3000& cc3000, char * server, int port){

  // Set parameters
  _retryTimeout = millis();
  _canConnect = true;
  _server = server;

  // Get IP
  _ip = 0;

  // Try looking up the website's IP address
  Serial.print(server); Serial.print(F(" -> "));
  while (_ip == 0) {
    if (! cc3000.getHostByName(server, &_ip)) {
      Serial.println(F("Couldn't resolve!"));
    }
    delay(500);
  }
  cc3000.printIPdotsRev(_ip);
  Serial.println();
  
  // Connect
  _client = cc3000.connectTCP(_ip, port);

  // Sending Handshake
  sendHandshake();

  // Reading Handshake
  readHandshake();
}

void CC3000_WebSocketClient::sendHandshake() {

  if (_client.connected()) {
    Serial.print(F("Sending handshake"));
  
  _client.fastrprint(F("GET "));
  _client.fastrprint(F("/"));  
  _client.fastrprint(F(" HTTP/1.1"));
  _client.fastrprint(F("\r\n"));
  Serial.print(F("."));

  _client.fastrprint(F("Host: "));
  _client.fastrprint(_server);
  _client.fastrprint(F("\r\n"));  
  Serial.print(F("."));

  _client.fastrprint(F("Connection: Upgrade"));
  _client.fastrprint(F("\r\n"));
  Serial.print(F("."));
    
  _client.fastrprint(F("Upgrade: websocket"));
  _client.fastrprint(F("\r\n"));
  Serial.print(F("."));

  _client.fastrprint(F("Sec-WebSocket-Key: "));
  char hash[45];  
  generateHash(hash, 45);
  _client.fastrprint(hash);
  _client.fastrprint(F("\r\n"));
  Serial.print(F("."));

  _client.fastrprint(F("Sec-WebSocket-Protocol: "));
  _client.fastrprint(_protocol);
  _client.fastrprint(F("\r\n"));
  Serial.print(F("."));
    
  _client.fastrprint(F("Sec-WebSocket-Version: 13"));
  _client.fastrprint(F("\r\n"));
  Serial.print(F("."));

  _client.fastrprint(F("Origin: Arduino"));
  _client.fastrprint(F("\r\n"));
  Serial.print(F("."));
    
  _client.fastrprint(F("\r\n"));
  Serial.println(F("done"));
  
  } else {
    Serial.println(F("Connection failed"));    
    return;
  }
}

void CC3000_WebSocketClient::readHandshake(){

  Serial.println(F("Reading handshake..."));
  String answer;
  unsigned long lastRead = millis();
  while (_client.connected() && (millis() - lastRead < IDLE_TIMEOUT_MS)) {
    while (_client.available()) {
      char c = _client.read();
      Serial.print(c);
      answer = answer + c;
      lastRead = millis();
    }
    if (answer.startsWith("HTTP/1.1 101")){
      Serial.println(F("Handshake successful."));
      break;
    }
  }
  Serial.println(F("Handshake done, monitoring now."));
}

bool CC3000_WebSocketClient::send (char* message) {
  if(!_canConnect || _reconnecting) {
    return false;
  }
  int len = strlen(message);
  _client.write(0x81);
  if(len > 125) {
    _client.write(0xFE);
    _client.write(byte(len >> 8));
    _client.write(byte(len & 0xFF));
  } else {
    _client.write(0x80 | byte(len));
  }
  for(int i = 0; i < 4; i++) {
    _client.write((byte)0x00); // use 0x00 for mask bytes which is effectively a NOOP
  }
  sendMessage(message);
  return true;
}

bool CC3000_WebSocketClient::sendMessage (char* message) {

  uint8_t chunkSize = 25;
  uint8_t max_iteration = (int)(strlen(message)/chunkSize) + 1;

  // Send data
  for (uint8_t i = 0; i < max_iteration; i++) {
    char intermediate_buffer[chunkSize+1];
    memcpy(intermediate_buffer, message + i*chunkSize, chunkSize);
    intermediate_buffer[chunkSize] = '\0';
    _client.fastrprint(intermediate_buffer);
  }
}

void CC3000_WebSocketClient::generateHash(char buffer[], size_t bufferlen) {
  byte bytes[16];
  for(int i = 0; i < 16; i++) {
    bytes[i] = random(255);
  }
  base64Encode(bytes, 16, buffer, bufferlen);
}

size_t CC3000_WebSocketClient::base64Encode(byte* src, size_t srclength, char* target, size_t targsize) {
  
  size_t datalength = 0;
  char input[3];
  char output[4];
  size_t i;
  
  while (2 < srclength) {
    input[0] = *src++;
    input[1] = *src++;
    input[2] = *src++;
    srclength -= 3;
    
    output[0] = input[0] >> 2;
    output[1] = ((input[0] & 0x03) << 4) + (input[1] >> 4);
    output[2] = ((input[1] & 0x0f) << 2) + (input[2] >> 6);
    output[3] = input[2] & 0x3f;
    
    if (datalength + 4 > targsize) {
      return (-1);
    }
    
    target[datalength++] = b64Alphabet[output[0]];
    target[datalength++] = b64Alphabet[output[1]];
    target[datalength++] = b64Alphabet[output[2]];
    target[datalength++] = b64Alphabet[output[3]];
  }
  
  // Padding
  if (0 != srclength) {
    input[0] = input[1] = input[2] = '\0';
    for (i = 0; i < srclength; i++) {
      input[i] = *src++;
    }
    
    output[0] = input[0] >> 2;
    output[1] = ((input[0] & 0x03) << 4) + (input[1] >> 4);
    output[2] = ((input[1] & 0x0f) << 2) + (input[2] >> 6);
    
    if (datalength + 4 > targsize) {
      return (-1);
    }
    
    target[datalength++] = b64Alphabet[output[0]];
    target[datalength++] = b64Alphabet[output[1]];
    if (srclength == 1) {
      target[datalength++] = '=';
    } else {
      target[datalength++] = b64Alphabet[output[2]];
    }
    target[datalength++] = '=';
  }
  if (datalength >= targsize) {
    return (-1);
  }
  target[datalength] = '\0';
  return (datalength);
}