#define IDLE_TIMEOUT_MS  3000
#define RETRY_TIMEOUT 3000

// Include libraries
#include <Adafruit_CC3000.h>

class CC3000_WebSocketClient {
public:
  typedef void (*OnMessage)(CC3000_WebSocketClient client, char* message);
  typedef void (*OnOpen)(CC3000_WebSocketClient client);
  typedef void (*OnClose)(CC3000_WebSocketClient client, int code, char* message);
  typedef void (*OnError)(CC3000_WebSocketClient client, char* message);
  CC3000_WebSocketClient(char * protocol);

  void connect(Adafruit_CC3000& cc3000, uint32_t ip, int port);
  void connect(Adafruit_CC3000& cc3000, char * server, int port);

  void monitor(Adafruit_CC3000& cc3000, uint32_t ip, int port);
  void monitor(Adafruit_CC3000& cc3000, char * server, int port);

  void sendHandshake();
  void readHandshake();

  void processMonitor();
 
  byte nextByte();
  void onOpen(OnOpen function);
  void onClose(OnClose function);
  void onMessage(OnMessage function);
  void onError(OnError function);
  bool send(char* message);
  bool sendMessage(char * message);
  void generateHash(char* buffer, size_t bufferlen);
  size_t base64Encode(byte* src, size_t srclength, char* target, size_t targetsize);
 
private:

  Adafruit_CC3000_Client _client;

  char * _protocol;
  char * _server;
  uint32_t _ip;
  String monitor_answer;
  char* _packet;
  OnOpen _onOpen;
  OnClose _onClose;
  OnMessage _onMessage;
  OnError _onError;
  unsigned int _packetLength;
  byte _opCode;
  bool _canConnect;
  bool _reconnecting;
  unsigned long _retryTimeout;
};

const char b64Alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";