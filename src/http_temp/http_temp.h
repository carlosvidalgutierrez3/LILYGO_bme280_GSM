#include "HTTPSClient2.h"
#include "http_temp_config.h"
#include <string>

const char* root_ca = ROOT_CA_;

void http_init(HTTPSClient& http) {
  http.setAuthorization(AUTHUSER_, AUTHPSW_);
 // String HTTPstr_ = static_cast <String> HTTP_ ;
  http.begin(URL_ , root_ca);
  http.setTimeout(HTTPCLIENT_DEFAULT_TCP_TIMEOUT);
}

int http_put(HTTPSClient& http, String contentType, uint8_t (&msgBuffer)[128]) {
  int httpCode = http.PUT(msgBuffer, sizeof(msgBuffer));

  if (!(httpCode > 0)) {
#ifdef DEBUG_MODE_
    Serial.print("Error on HTTP request: ");
    Serial.println(httpCode);
#endif
  }
  http.end();
  return httpCode;
}
