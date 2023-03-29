#include <HardwareSerial.h>
/**
 * HTTPClient.cpp
 *
 * Created on: 02.11.2015
 *
 * Copyright (c) 2015 Markus Sattler. All rights reserved.
 * This file is part of the HTTPClient for Arduino.
 * Port to ESP32 by Evandro Luis Copercini (2017),
 * changed fingerprints to CA verification.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Adapted in October 2018
 */

#include <Arduino.h>
#include <esp32-hal-log.h>

#ifdef HTTPCLIENT_1_1_COMPATIBLE
#include <WiFi.h>
#endif

#include <StreamString.h>
#include <base64.h>

#include "HTTPSClient2.h"

#ifdef HTTPCLIENT_1_1_COMPATIBLE

class TLSTraits
{
public:
    TLSTraits(const char* CAcert, const char* clicert = NULL, const char* clikey = NULL) :
        _cacert(CAcert), _clicert(clicert), _clikey(clikey)
    {
    }

  std::unique_ptr<WiFiClientSecure> create()
    {
      std::unique_ptr<WiFiClientSecure> wcs{new WiFiClientSecure()};
      wcs->setCACert(_cacert);
      wcs->setCertificate(_clicert);
      wcs->setPrivateKey(_clikey);
      return wcs;
    }

    bool verify(WiFiClient& client, const char* host)
    {
         WiFiClientSecure& wcs = static_cast<WiFiClientSecure&>(client);
         wcs.setCACert(_cacert);
	 wcs.setCertificate(_clicert);
         wcs.setPrivateKey(_clikey);
         return true;
    }

protected:
    const char* _cacert;
    const char* _clicert;
    const char* _clikey;
};
#endif // HTTPCLIENT_1_1_COMPATIBLE

/**
 * constructor
 */
HTTPSClient::HTTPSClient()
{
}

/**
 * destructor
 */
HTTPSClient::~HTTPSClient()
{
    if(_client) {
        _client->stop();
    }
    if(_currentHeaders) {
        delete[] _currentHeaders;
    }
}

void HTTPSClient::clear()
{
    _returnCode = 0;
    _size = -1;
    _headers = "";
}



#ifdef HTTPCLIENT_1_1_COMPATIBLE
bool HTTPSClient::begin(String url, const char* CAcert)
{
  log_d("in begin(url, CAcert) with CAcert = %s", CAcert);
    if(_client && !_tcpDeprecated) {
        log_d("mix up of new and deprecated api");
        _canReuse = false;
        end();
    }

    _port = 443;
    if (!beginInternal(url, "https")) {
      return false;
    }
    _secure = true;
    _TLSTraits = TLSTraitsPtr(new TLSTraits(CAcert));
    if(!_TLSTraits) {
        log_e("could not create transport traits");
        return false;
    }

    return true;
}

#endif // HTTPCLIENT_1_1_COMPATIBLE

bool HTTPSClient::beginInternal(String url, const char* expectedProtocol)
{
    log_v("url: %s", url.c_str());
    clear();

    // check for : (http: or https:
    int index = url.indexOf(':');
    if(index < 0) {
        log_e("failed to parse protocol");
        return false;
    }

    _protocol = url.substring(0, index);
    if (_protocol != expectedProtocol) {
        log_w("unexpected protocol: %s, expected %s", _protocol.c_str(), expectedProtocol);
        return false;
    }

    url.remove(0, (index + 3)); // remove http:// or https://

    index = url.indexOf('/');
    String host = url.substring(0, index);
    url.remove(0, index); // remove host part

    // get Authorization
    index = host.indexOf('@');
    if(index >= 0) {
        // auth info
        String auth = host.substring(0, index);
        host.remove(0, index + 1); // remove auth part including @
        _base64Authorization = base64::encode(auth);
    }

    // get port
    index = host.indexOf(':');
    if(index >= 0) {
        _host = host.substring(0, index); // hostname
        host.remove(0, (index + 1)); // remove hostname + :
        _port = host.toInt(); // get port
    } else {
        _host = host;
    }
    _uri = url;
    log_d("host: %s port: %d url: %s", _host.c_str(), _port, _uri.c_str());
    return true;
}

#ifdef HTTPCLIENT_1_1_COMPATIBLE
bool HTTPSClient::begin(String host, uint16_t port, String uri, const char* CAcert)
{
    if(_client && !_tcpDeprecated) {
        log_d("mix up of new and deprecated api");
        _canReuse = false;
        end();
    }

    clear();
    _host = host;
    _port = port;
    _uri = uri;

    if (strlen(CAcert) == 0) {
        return false;
    }
    _secure = true;
    _TLSTraits = TLSTraitsPtr(new TLSTraits(CAcert));
    return true;
}

bool HTTPSClient::begin(String host, uint16_t port, String uri, const char* CAcert, const char* cli_cert, const char* cli_key)
{
    if(_client && !_tcpDeprecated) {
        log_d("mix up of new and deprecated api");
        _canReuse = false;
        end();
    }

    clear();
    _host = host;
    _port = port;
    _uri = uri;

    if (strlen(CAcert) == 0) {
        return false;
    }
    _secure = true;
    _TLSTraits = TLSTraitsPtr(new TLSTraits(CAcert, cli_cert, cli_key));
    return true;
}
#endif // HTTPCLIENT_1_1_COMPATIBLE

/**
 * end
 * called after the payload is handled
 */
void HTTPSClient::end(void)
{
    disconnect(false);
    clear();
}



/**
 * disconnect
 * close the TCP socket
 */
void HTTPSClient::disconnect(bool preserveClient)
{
    if(connected()) {
        if(_client->available() > 0) {
            log_d("still data in buffer (%d), clean up.\n", _client->available());
            while(_client->available() > 0) {
                _client->read();
            }
        }

        if(_reuse && _canReuse) {
            log_d("tcp keep open for reuse\n");
        } else {
            log_d("tcp stop\n");
            _client->stop();
            if(!preserveClient) {
                _client = nullptr;
            }
#ifdef HTTPCLIENT_1_1_COMPATIBLE
            if(_tcpDeprecated) {
                _TLSTraits.reset(nullptr);
                _tcpDeprecated.reset(nullptr);
            }
#endif
        }
    } else {
        log_d("tcp is closed\n");
    }
}


/**
 * connected
 * @return connected status
 */
bool HTTPSClient::connected()
{
    if(_client) {
        return ((_client->available() > 0) || _client->connected());
    }
    return false;
}

/**
 * try to reuse the connection to the server
 * keep-alive
 * @param reuse bool
 */
void HTTPSClient::setReuse(bool reuse)
{
    _reuse = reuse;
}

/**
 * set User Agent
 * @param userAgent const char *
 */
void HTTPSClient::setUserAgent(const String& userAgent)
{
    _userAgent = userAgent;
}

/**
 * set the Authorizatio for the http request
 * @param user const char *
 * @param password const char *
 */
void HTTPSClient::setAuthorization(const char * user, const char * password)
{
    if(user && password) {
        String auth = user;
        auth += ":";
        auth += password;
        _base64Authorization = base64::encode(auth);
    }
}

/**
 * set the Authorizatio for the http request
 * @param auth const char * base64
 */
void HTTPSClient::setAuthorization(const char * auth)
{
    if(auth) {
        _base64Authorization = auth;
    }
}

/**
 * set the timeout (ms) for establishing a connection to the server
 * @param connectTimeout int32_t
 */
void HTTPSClient::setConnectTimeout(int32_t connectTimeout)
{
    _connectTimeout = connectTimeout;
}

/**
 * set the timeout for the TCP connection
 * @param timeout unsigned int
 */
void HTTPSClient::setTimeout(uint16_t timeout)
{
    _tcpTimeout = timeout;
    if(connected()) {
        _client->setTimeout((timeout + 500) / 1000);
    }
}

/**
 * use HTTP1.0
 * @param use
 */
void HTTPSClient::useHTTP10(bool useHTTP10)
{
    _useHTTP10 = useHTTP10;
    _reuse = !useHTTP10;
}

/**
 * send a GET request
 * @return http code
 */
int HTTPSClient::GET()
{
    return sendRequest("GET");
}

/**
 * sends a post request to the server
 * @param payload uint8_t *
 * @param size size_t
 * @return http code
 */
int HTTPSClient::POST(uint8_t * payload, size_t size)
{
    return sendRequest("POST", payload, size);
}

int HTTPSClient::POST(String payload)
{
    return POST((uint8_t *) payload.c_str(), payload.length());
}

/**
 * sends a patch request to the server
 * @param payload uint8_t *
 * @param size size_t
 * @return http code
 */
int HTTPSClient::PATCH(uint8_t * payload, size_t size)
{
    return sendRequest("PATCH", payload, size);
}

int HTTPSClient::PATCH(String payload)
{
    return PATCH((uint8_t *) payload.c_str(), payload.length());
}

/**
 * sends a put request to the server
 * @param payload uint8_t *
 * @param size size_t
 * @return http code
 */
int HTTPSClient::PUT(uint8_t * payload, size_t size) {
    return sendRequest("PUT", payload, size);
}

int HTTPSClient::PUT(String payload) {
    return PUT((uint8_t *) payload.c_str(), payload.length());
}

/**
 * sendRequest
 * @param type const char *     "GET", "POST", ....
 * @param payload String        data for the message body
 * @return
 */
int HTTPSClient::sendRequest(const char * type, String payload)
{
    return sendRequest(type, (uint8_t *) payload.c_str(), payload.length());
}

/**
 * sendRequest
 * @param type const char *     "GET", "POST", ....
 * @param payload uint8_t *     data for the message body if null not send
 * @param size size_t           size for the message body if 0 not send
 * @return -1 if no info or > 0 when Content-Length is set by server
 */
int HTTPSClient::sendRequest(const char * type, uint8_t * payload, size_t size)
{
    // connect to server
    if(!connect()) {
        return returnError(HTTPC_ERROR_CONNECTION_REFUSED);
    }
    if(payload && size > 0) {
        addHeader(F("Content-Length"), String(size));
    }
    // send Header
    if(!sendHeader(type)) {
        return returnError(HTTPC_ERROR_SEND_HEADER_FAILED);
    }
    // send Payload if needed
    if(payload && size > 0) {
        if(_client->write(&payload[0], size) != size) {
            return returnError(HTTPC_ERROR_SEND_PAYLOAD_FAILED);
        }
    }

    // handle Server Response (Header)
    return returnError(handleHeaderResponse());
}

/**
 * sendRequest
 * @param type const char *     "GET", "POST", ....
 * @param stream Stream *       data stream for the message body
 * @param size size_t           size for the message body if 0 not Content-Length is send
 * @return -1 if no info or > 0 when Content-Length is set by server
 */
int HTTPSClient::sendRequest(const char * type, Stream * stream, size_t size)
{
    if(!stream) {
        return returnError(HTTPC_ERROR_NO_STREAM);
    }

    // connect to server
    if(!connect()) {
        return returnError(HTTPC_ERROR_CONNECTION_REFUSED);
    }

    if(size > 0) {
        addHeader("Content-Length", String(size));
    }

    // send Header
    if(!sendHeader(type)) {
        return returnError(HTTPC_ERROR_SEND_HEADER_FAILED);
    }

    int buff_size = HTTP_TCP_BUFFER_SIZE;

    int len = size;
    int bytesWritten = 0;

    if(len == 0) {
        len = -1;
    }

    // if possible create smaller buffer then HTTP_TCP_BUFFER_SIZE
    if((len > 0) && (len < HTTP_TCP_BUFFER_SIZE)) {
        buff_size = len;
    }

    // create buffer for read
    uint8_t * buff = (uint8_t *) malloc(buff_size);

    if(buff) {
        // read all data from stream and send it to server
        while(connected() && (stream->available() > -1) && (len > 0 || len == -1)) {

            // get available data size
            int sizeAvailable = stream->available();

            if(sizeAvailable) {

                int readBytes = sizeAvailable;

                // read only the asked bytes
                if(len > 0 && readBytes > len) {
                    readBytes = len;
                }

                // not read more the buffer can handle
                if(readBytes > buff_size) {
                    readBytes = buff_size;
                }

                // read data
                int bytesRead = stream->readBytes(buff, readBytes);

                // write it to Stream
                int bytesWrite = _client->write((const uint8_t *) buff, bytesRead);
                bytesWritten += bytesWrite;

                // are all Bytes a writen to stream ?
                if(bytesWrite != bytesRead) {
                    log_d("short write, asked for %d but got %d retry...", bytesRead, bytesWrite);

                    // check for write error
                    if(_client->getWriteError()) {
                        log_d("stream write error %d", _client->getWriteError());

                        //reset write error for retry
                        _client->clearWriteError();
                    }

                    // some time for the stream
                    delay(1);

                    int leftBytes = (readBytes - bytesWrite);

                    // retry to send the missed bytes
                    bytesWrite = _client->write((const uint8_t *) (buff + bytesWrite), leftBytes);
                    bytesWritten += bytesWrite;

                    if(bytesWrite != leftBytes) {
                        // failed again
                        log_d("short write, asked for %d but got %d failed.", leftBytes, bytesWrite);
                        free(buff);
                        return returnError(HTTPC_ERROR_SEND_PAYLOAD_FAILED);
                    }
                }

                // check for write error
                if(_client->getWriteError()) {
                    log_d("stream write error %d", _client->getWriteError());
                    free(buff);
                    return returnError(HTTPC_ERROR_SEND_PAYLOAD_FAILED);
                }

                // count bytes to read left
                if(len > 0) {
                    len -= readBytes;
                }

                delay(0);
            } else {
                delay(1);
            }
        }

        free(buff);

        if(size && (int) size != bytesWritten) {
            log_d("Stream payload bytesWritten %d and size %d mismatch!.", bytesWritten, size);
            log_d("ERROR SEND PAYLOAD FAILED!");
            return returnError(HTTPC_ERROR_SEND_PAYLOAD_FAILED);
        } else {
            log_d("Stream payload written: %d", bytesWritten);
        }

    } else {
        log_d("too less ram! need %d", HTTP_TCP_BUFFER_SIZE);
        return returnError(HTTPC_ERROR_TOO_LESS_RAM);
    }

    // handle Server Response (Header)
    return returnError(handleHeaderResponse());
}

/**
 * size of message body / payload
 * @return -1 if no info or > 0 when Content-Length is set by server
 */
int HTTPSClient::getSize(void)
{
    return _size;
}

/**
 * returns the stream of the tcp connection
 * @return WiFiClient
 */
WiFiClient& HTTPSClient::getStream(void)
{
    if (connected()) {
        return *_client;
    }

    log_w("getStream: not connected");
    static WiFiClient empty;
    return empty;
}

/**
 * returns a pointer to the stream of the tcp connection
 * @return WiFiClient*
 */
WiFiClient* HTTPSClient::getStreamPtr(void)
{
    if(connected()) {
        return _client;
    }

    log_w("getStreamPtr: not connected");
    return nullptr;
}

/**
 * write all  message body / payload to Stream
 * @param stream Stream *
 * @return bytes written ( negative values are error codes )
 */
int HTTPSClient::writeToStream(Stream * stream)
{

    if(!stream) {
        return returnError(HTTPC_ERROR_NO_STREAM);
    }

    if(!connected()) {
        return returnError(HTTPC_ERROR_NOT_CONNECTED);
    }

    // get length of document (is -1 when Server sends no Content-Length header)
    int len = _size;
    int ret = 0;

    if(_transferEncoding == HTTPC_TE_IDENTITY) {
        ret = writeToStreamDataBlock(stream, len);

        // have we an error?
        if(ret < 0) {
            return returnError(ret);
        }
    } else if(_transferEncoding == HTTPC_TE_CHUNKED) {
        int size = 0;
        while(1) {
            if(!connected()) {
                return returnError(HTTPC_ERROR_CONNECTION_LOST);
            }
            String chunkHeader = _client->readStringUntil('\n');

            if(chunkHeader.length() <= 0) {
                return returnError(HTTPC_ERROR_READ_TIMEOUT);
            }

            chunkHeader.trim(); // remove \r

            // read size of chunk
            len = (uint32_t) strtol((const char *) chunkHeader.c_str(), NULL, 16);
            size += len;
            log_d(" read chunk len: %d", len);

            // data left?
            if(len > 0) {
                int r = writeToStreamDataBlock(stream, len);
                if(r < 0) {
                    // error in writeToStreamDataBlock
                    return returnError(r);
                }
                ret += r;
            } else {

                // if no length Header use global chunk size
                if(_size <= 0) {
                    _size = size;
                }

                // check if we have write all data out
                if(ret != _size) {
                    return returnError(HTTPC_ERROR_STREAM_WRITE);
                }
                break;
            }

            // read trailing \r\n at the end of the chunk
            char buf[2];
            auto trailing_seq_len = _client->readBytes((uint8_t*)buf, 2);
            if (trailing_seq_len != 2 || buf[0] != '\r' || buf[1] != '\n') {
                return returnError(HTTPC_ERROR_READ_TIMEOUT);
            }

            delay(0);
        }
    } else {
        return returnError(HTTPC_ERROR_ENCODING);
    }

//    end();
    disconnect(true);
    return ret;
}

/**
 * return all payload as String (may need lot of ram or trigger out of memory!)
 * @return String
 */
String HTTPSClient::getString(void)
{
    StreamString sstring;

    if(_size) {
        // try to reserve needed memmory
        if(!sstring.reserve((_size + 1))) {
            log_d("not enough memory to reserve a string! need: %d", (_size + 1));
            return "";
        }
    }

    writeToStream(&sstring);
    return sstring;
}

/**
 * converts error code to String
 * @param error int
 * @return String
 */
String HTTPSClient::errorToString(int error)
{
    switch(error) {
    case HTTPC_ERROR_CONNECTION_REFUSED:
        return F("connection refused");
    case HTTPC_ERROR_SEND_HEADER_FAILED:
        return F("send header failed");
    case HTTPC_ERROR_SEND_PAYLOAD_FAILED:
        return F("send payload failed");
    case HTTPC_ERROR_NOT_CONNECTED:
        return F("not connected");
    case HTTPC_ERROR_CONNECTION_LOST:
        return F("connection lost");
    case HTTPC_ERROR_NO_STREAM:
        return F("no stream");
    case HTTPC_ERROR_NO_HTTP_SERVER:
        return F("no HTTP server");
    case HTTPC_ERROR_TOO_LESS_RAM:
        return F("too less ram");
    case HTTPC_ERROR_ENCODING:
        return F("Transfer-Encoding not supported");
    case HTTPC_ERROR_STREAM_WRITE:
        return F("Stream write error");
    case HTTPC_ERROR_READ_TIMEOUT:
        return F("read Timeout");
    default:
        return String();
    }
}

/**
 * adds Header to the request
 * @param name
 * @param value
 * @param first
 */
void HTTPSClient::addHeader(const String& name, const String& value, bool first, bool replace)
{
    // not allow set of Header handled by code
    if(!name.equalsIgnoreCase(F("Connection")) &&
       !name.equalsIgnoreCase(F("User-Agent")) &&
       !name.equalsIgnoreCase(F("Host")) &&
       !(name.equalsIgnoreCase(F("Authorization")) && _base64Authorization.length())){

        String headerLine = name;
        headerLine += ": ";

        if (replace) {
            int headerStart = _headers.indexOf(headerLine);
            if (headerStart != -1) {
                int headerEnd = _headers.indexOf('\n', headerStart);
                _headers = _headers.substring(0, headerStart) + _headers.substring(headerEnd + 1);
            }
        }

        headerLine += value;
        headerLine += "\r\n";
        if(first) {
            _headers = headerLine + _headers;
        } else {
            _headers += headerLine;
        }
    }

}

void HTTPSClient::collectHeaders(const char* headerKeys[], const size_t headerKeysCount)
{
    _headerKeysCount = headerKeysCount;
    if(_currentHeaders) {
        delete[] _currentHeaders;
    }
    _currentHeaders = new RequestArgument[_headerKeysCount];
    for(size_t i = 0; i < _headerKeysCount; i++) {
        _currentHeaders[i].key = headerKeys[i];
    }
}

String HTTPSClient::header(const char* name)
{
    for(size_t i = 0; i < _headerKeysCount; ++i) {
        if(_currentHeaders[i].key == name) {
            return _currentHeaders[i].value;
        }
    }
    return String();
}

String HTTPSClient::header(size_t i)
{
    if(i < _headerKeysCount) {
        return _currentHeaders[i].value;
    }
    return String();
}

String HTTPSClient::headerName(size_t i)
{
    if(i < _headerKeysCount) {
        return _currentHeaders[i].key;
    }
    return String();
}

int HTTPSClient::headers()
{
    return _headerKeysCount;
}

bool HTTPSClient::hasHeader(const char* name)
{
    for(size_t i = 0; i < _headerKeysCount; ++i) {
        if((_currentHeaders[i].key == name) && (_currentHeaders[i].value.length() > 0)) {
            return true;
        }
    }
    return false;
}

/**
 * init TCP connection and handle ssl verify if needed
 * @return true if connection is ok
 */
bool HTTPSClient::connect(void)
{
    if(connected()) {
        if(_reuse) {
            log_d("already connected, reusing connection");
        } else {
            log_d("already connected, try reuse!");
        }
        while(_client->available() > 0) {
            _client->read();
        }
        return true;
    }

#ifdef HTTPCLIENT_1_1_COMPATIBLE
     if(_TLSTraits && !_client) {
        _tcpDeprecated = _TLSTraits->create();
        if(!_tcpDeprecated) {
            log_e("failed to create client");
            return false;
        }
	_client = _tcpDeprecated.get();

     }
#endif

    if (!_client) {
        log_d("HTTPClient::begin was not called or returned error");
        return false;
    }
    if(!_client->connect(_host.c_str(), _port, _connectTimeout)) {
        log_d("failed connect to %s:%u", _host.c_str(), _port);
        return false;
    }

    log_d(" connected to %s:%u", _host.c_str(), _port);

#ifdef HTTPCLIENT_1_1_COMPATIBLE
    if (_tcpDeprecated && !_TLSTraits->verify(*_client, _host.c_str())) {
        log_d("transport level verify failed");
        _client->stop();
        return false;
    }
#endif

/*
#ifdef ESP8266
    _client->setNoDelay(true);
#endif
 */
 return connected();
}

/**
 * sends HTTP request header
 * @param type (GET, POST, ...)
 * @return status
 */
bool HTTPSClient::sendHeader(const char * type)
{
    if(!connected()) {
        return false;
    }

    String header = String(type) + " " + _uri + F(" HTTP/1.");

    if(_useHTTP10) {
        header += "0";
    } else {
        header += "1";
    }

    header += String(F("\r\nHost: ")) + _host;
    if (_port != 80 && _port != 443)
    {
        header += ':';
        header += String(_port);
    }
    header += String(F("\r\nUser-Agent: ")) + _userAgent +
              F("\r\nConnection: ");

    if(_reuse) {
        header += F("keep-alive");
    } else {
        header += F("close");
    }
    header += "\r\n";

    if(!_useHTTP10) {
        header += F("Accept-Encoding: identity;q=1,chunked;q=0.1,*;q=0\r\n");
    }

    if(_base64Authorization.length()) {
        _base64Authorization.replace("\n", "");
        header += F("Authorization: Basic ");
        header += _base64Authorization;
        header += "\r\n";
    }

    header += _headers + "\r\n";

    return (_client->write((const uint8_t *) header.c_str(), header.length()) == header.length());
}

/**
 * reads the response from the server
 * @return int http code
 */
int HTTPSClient::handleHeaderResponse()
{

    if(!connected()) {
        return HTTPC_ERROR_NOT_CONNECTED;
    }

    clear();

    _canReuse = _reuse;

    String transferEncoding;

    _transferEncoding = HTTPC_TE_IDENTITY;
    unsigned long lastDataTime = millis();

    while(connected()) {
        size_t len = _client->available();
        if(len > 0) {
            String headerLine = _client->readStringUntil('\n');
            headerLine.trim(); // remove \r

            lastDataTime = millis();

            log_v("RX: '%s'", headerLine.c_str());

            if(headerLine.startsWith("HTTP/1.")) {
                if(_canReuse) {
                    _canReuse = (headerLine[sizeof "HTTP/1." - 1] != '0');
                }
                _returnCode = headerLine.substring(9, headerLine.indexOf(' ', 9)).toInt();
            } else if(headerLine.indexOf(':')) {
                String headerName = headerLine.substring(0, headerLine.indexOf(':'));
                String headerValue = headerLine.substring(headerLine.indexOf(':') + 1);
                headerValue.trim();

                if(headerName.equalsIgnoreCase("Content-Length")) {
                    _size = headerValue.toInt();
                }

                if(_canReuse && headerName.equalsIgnoreCase("Connection")) {
                    if(headerValue.indexOf("close") >= 0 && headerValue.indexOf("keep-alive") < 0) {
                        _canReuse = false;
                    }
                }

                if(headerName.equalsIgnoreCase("Transfer-Encoding")) {
                    transferEncoding = headerValue;
                }

                for(size_t i = 0; i < _headerKeysCount; i++) {
                    if(_currentHeaders[i].key.equalsIgnoreCase(headerName)) {
                        _currentHeaders[i].value = headerValue;
                        break;
                    }
                }
            }

            if(headerLine == "") {
                log_d("code: %d", _returnCode);

                if(_size > 0) {
                    log_d("size: %d", _size);
                }

                if(transferEncoding.length() > 0) {
                    log_d("Transfer-Encoding: %s", transferEncoding.c_str());
                    if(transferEncoding.equalsIgnoreCase("chunked")) {
                        _transferEncoding = HTTPC_TE_CHUNKED;
                    } else {
                        return HTTPC_ERROR_ENCODING;
                    }
                } else {
                    _transferEncoding = HTTPC_TE_IDENTITY;
                }

                if(_returnCode) {
                    return _returnCode;
                } else {
                    log_d("Remote host is not an HTTP Server!");
                    return HTTPC_ERROR_NO_HTTP_SERVER;
                }
            }

        } else {
            if((millis() - lastDataTime) > _tcpTimeout) {
                return HTTPC_ERROR_READ_TIMEOUT;
            }
            delay(10);
        }
    }

    return HTTPC_ERROR_CONNECTION_LOST;
}

/**
 * write one Data Block to Stream
 * @param stream Stream *
 * @param size int
 * @return < 0 = error >= 0 = size written
 */
int HTTPSClient::writeToStreamDataBlock(Stream * stream, int size)
{
    int buff_size = HTTP_TCP_BUFFER_SIZE;
    int len = size;
    int bytesWritten = 0;

    // if possible create smaller buffer then HTTP_TCP_BUFFER_SIZE
    if((len > 0) && (len < HTTP_TCP_BUFFER_SIZE)) {
        buff_size = len;
    }

    // create buffer for read
    uint8_t * buff = (uint8_t *) malloc(buff_size);

    if(buff) {
        // read all data from server
        while(connected() && (len > 0 || len == -1)) {

            // get available data size
            size_t sizeAvailable = _client->available();

            if(sizeAvailable) {

                int readBytes = sizeAvailable;

                // read only the asked bytes
                if(len > 0 && readBytes > len) {
                    readBytes = len;
                }

                // not read more the buffer can handle
                if(readBytes > buff_size) {
                    readBytes = buff_size;
                }

		// stop if no more reading
		if (readBytes == 0)
			break;

                // read data
                int bytesRead = _client->readBytes(buff, readBytes);

                // write it to Stream
                int bytesWrite = stream->write(buff, bytesRead);
                bytesWritten += bytesWrite;

                // are all Bytes a writen to stream ?
                if(bytesWrite != bytesRead) {
                    log_d("short write asked for %d but got %d retry...", bytesRead, bytesWrite);

                    // check for write error
                    if(stream->getWriteError()) {
                        log_d("stream write error %d", stream->getWriteError());

                        //reset write error for retry
                        stream->clearWriteError();
                    }

                    // some time for the stream
                    delay(1);

                    int leftBytes = (readBytes - bytesWrite);

                    // retry to send the missed bytes
                    bytesWrite = stream->write((buff + bytesWrite), leftBytes);
                    bytesWritten += bytesWrite;

                    if(bytesWrite != leftBytes) {
                        // failed again
                        log_w("short write asked for %d but got %d failed.", leftBytes, bytesWrite);
                        free(buff);
                        return HTTPC_ERROR_STREAM_WRITE;
                    }
                }

                // check for write error
                if(stream->getWriteError()) {
                    log_w("stream write error %d", stream->getWriteError());
                    free(buff);
                    return HTTPC_ERROR_STREAM_WRITE;
                }

                // count bytes to read left
                if(len > 0) {
                    len -= readBytes;
                }

                delay(0);
            } else {
                delay(1);
            }
        }

        free(buff);

        log_d("connection closed or file end (written: %d).", bytesWritten);

        if((size > 0) && (size != bytesWritten)) {
            log_d("bytesWritten %d and size %d mismatch!.", bytesWritten, size);
            return HTTPC_ERROR_STREAM_WRITE;
        }

    } else {
        log_w("too less ram! need %d", HTTP_TCP_BUFFER_SIZE);
        return HTTPC_ERROR_TOO_LESS_RAM;
    }

    return bytesWritten;
}

/**
 * called to handle error return, may disconnect the connection if still exists
 * @param error
 * @return error
 */
int HTTPSClient::returnError(int error)
{
    if(error < 0) {
        log_w("error(%d): %s", error, errorToString(error).c_str());
        if(connected()) {
            log_d("tcp stop");
            _client->stop();
        }
    }
    return error;
}
