#include "ResponseHandler.hpp"

/* ............................... CONSTRUCTOR ...............................*/

ResponseHandler::ResponseHandler(RequestHandler& reqHandler, int receivedPort)
    : _requestHandler(reqHandler),_method(NULL), _port(0), _uploadLeftOver(0) {
  this->init(reqHandler, receivedPort);
}

/* ..............................COPY CONSTRUCTOR.............................*/

/* ................................ DESTRUCTOR ...............................*/

ResponseHandler::~ResponseHandler(void) {
  if (_method != NULL) delete _method;
}

/* ................................. METHODS .................................*/

void ResponseHandler::init(RequestHandler& reqHandler, int receivedPort) {
  if (_method != NULL) delete _method;
  _method = NULL;

  _port = receivedPort;
  _requestHandler = reqHandler;
  if (_requestHandler._req.is_ok()) {
    _req = _requestHandler._req.unwrap();
    switch (_req.method) {
      case methods::GET:
        _method = new (std::nothrow) GetMethod(*this);
        break;
      case methods::POST:
        _method = new (std::nothrow) PostMethod(*this);
        break;
      case methods::DELETE:
        _method = new (std::nothrow) DeleteMethod(*this);
        break;

      default:
        break;
    }
    if (_method == NULL)
      GetMethod(*this).makeStandardResponse(status::InternalServerError);
  }
}

void ResponseHandler::processRequest() {
  if (_requestHandler._req.is_err()) {
    return GetMethod(*this).makeStandardResponse(_requestHandler._req.unwrap_err());
  }
  std::string host = getReqHeader("Host");
  if (host.empty()) {
    return GetMethod(*this).makeStandardResponse(status::BadRequest);
  }
  _serv = network::ServerPool::getServerMatch(host, _port);
  _loc = network::ServerPool::getLocationMatch(_serv, _req.target);

  // Check if the location resolved allows the requested method
  if (_loc.get_methods().has(_req.method) == false) {
    _method->makeStandardResponse(status::MethodNotAllowed);
    std::stringstream allowed;
    allowed << _loc.get_methods();
    return _resp.setHeader(headerTitle::Allow, allowed.str());
  }

  // If any payload, check if acceptable size
  if (_loc.get_body_size() < _req.get_body().size()) {
    return _method->makeStandardResponse(status::PayloadTooLarge);
  }

  // Check if the location resolved has a redirection in place
  redirect red = _loc.get_redirect();
  if (red.status != 0) {
    return _method->manageRedirect(red);
  }

  if (_loc.get_root().empty()) {
    return _method->makeStandardResponse(status::Forbidden);
  }
  return _method->handler();
}

// safely returns the value of a header if it exists, an empty string otherwise
std::string ResponseHandler::getReqHeader(const std::string& target) {
  return _req.get_header(target).unwrap_or("");
}

void ResponseHandler::doWriteBody( void ) {
  int uploadFd = _resp.getUploadFd();

  if (uploadFd != UNSET) {
    const std::vector<char>& body = _req.get_body();
    int ret = 0;
    if (body.size() > 0) {
      int offset = body.size() - _uploadLeftOver;
      ret = write(uploadFd, body.data() + offset, _uploadLeftOver);
      _uploadLeftOver -= ret;
    }
    if (_uploadLeftOver == 0 || ret < 1) {
      close(uploadFd);
      _resp.setUploadFd(UNSET);
      if (ret < 0)
        _method->makeStandardResponse(status::InternalServerError);
    }
  }
}

int ResponseHandler::doSend(int fdDest, int flags) {
#if __APPLE__
  int set = 1;
  setsockopt(fdDest, SOL_SOCKET, SO_NOSIGPIPE, (void*)&set, sizeof(int));
#endif
  int& state = _resp.getState();
  if (state == respState::emptyResp ||
      state & (respState::entirelySent | respState::ioError)) {
    _resp.getFileInst().closeFile();
    return RESPONSE_SENT_ENTIRELY;
  }
  if (state & respState::cgiResp) {
    sendFromCgi(fdDest, flags);
  } else if (state & respState::fileResp)
    sendFromFile(fdDest, flags);
  else if (state & respState::buffResp)
    sendFromBuffer(fdDest, flags);
  else if (state & respState::noBodyResp)
    sendHeaders(fdDest, flags);

  if (state & (respState::entirelySent | respState::ioError)) {
    _resp.getFileInst().closeFile();
    return RESPONSE_SENT_ENTIRELY;
  } else {
    return RESPONSE_AVAILABLE;
  }
}

void ResponseHandler::sendHeaders(int fdDest, int flags) {
  int& state = _resp.getState();
  if ((state & respState::headerSent) == false) {
    logData();
    std::stringstream output;
    output << _resp;
    if ((state & respState::cgiResp) == false) output << "\r\n";
    send(fdDest, output.str().c_str(), output.str().length(), flags);
    if (state & respState::noBodyResp)
      state |= respState::headerSent | respState::entirelySent;
    else
      state |= respState::headerSent;
  }
}

void ResponseHandler::checkCgiTimeout() {
  cgi_status::status cgiStatus = _resp.getCgiInst().status();
  if (cgiStatus == cgi_status::NON_INIT)
    return ;

  if (STATUS_IS_ERROR(cgiStatus)) {
    int uploadFd = _resp.getUploadFd();
    if (uploadFd != UNSET) {
      close(uploadFd);
      _resp.setUploadFd(UNSET);
    }
    return handleCgiError(cgiStatus);
  }
}

status::StatusCode ResponseHandler::pickCgiError(
    cgi_status::status cgiStatus) const {
  switch (cgiStatus) {
    case cgi_status::TIMEOUT:
      return status::GatewayTimeout;
    case cgi_status::SYSTEM_ERROR:
      return status::InternalServerError;
    case cgi_status::UNSUPPORTED:
      return status::MethodNotAllowed;
    case cgi_status::CGI_ERROR:
      return status::BadGateway;

    default:
      return status::InternalServerError;
  }
}

void ResponseHandler::handleCgiError( cgi_status::status cgiStatus ) {
  int& respStatus = _resp.getState();
  if ((respStatus & respState::headerSent) == false) {
    int pid = _resp.getCgiInst().get_pid();
    if (pid != UNSET && cgiStatus == cgi_status::TIMEOUT && kill(pid, SIGKILL) == 0) {
        if (waitpid(pid, NULL, 0) == pid)
          _resp.getCgiInst().unset_pid();
    }
    return _method->makeStandardResponse(pickCgiError(cgiStatus));
  } else {
    respStatus = respState::ioError;
    return;
  }
}

void ResponseHandler::sendFromCgi(int fdDest, int flags) {
  int& respStatus = _resp.getState();
  if ((respStatus & respState::headerSent) == false)
    _resp.getCgiInst().setCgiHeader();

  cgi_status::status cgiStatus = _resp.getCgiInst().status();
  if (cgiStatus == cgi_status::WAITING) {
    return;
  } else if (STATUS_IS_ERROR(cgiStatus)) {
    return handleCgiError(cgiStatus);
  }

  if ((respStatus & respState::headerSent) == false) sendHeaders(fdDest, flags);
  int cgiPipe = _resp.getCgiInst().get_readable_pipe();
  if ((respStatus & respState::cgiHeadersSent) == false)
    sendCgiHeaders(fdDest, flags);
  doSendFromFD(cgiPipe, fdDest, flags);
}

void ResponseHandler::sendCgiHeaders(int fdDest, int flags) {
  if (_resp.getState() & respState::cgiHeadersSent) {
    return;
  }

  std::string const& cgiHeaders = _resp.getCgiInst().getCgiHeader();
  if (send(fdDest, cgiHeaders.c_str(), cgiHeaders.length(), flags) < 0)
    _resp.getState() |= respState::ioError;
  else
    _resp.getState() |= respState::cgiHeadersSent;
}

void ResponseHandler::sendFromFile(int fdDest, int flags) {
  if ((_resp.getState() & respState::headerSent) == false)
    sendHeaders(fdDest, flags);
  if (_resp.getFileInst().isGood()) {
    doSendFromFD(_resp.getFileInst().getFD(), fdDest, flags);
  } else {
    _resp.getState() = respState::ioError;
    return;
  }
}

void ResponseHandler::doSendFromFD(int fdSrc, int fdDest, int flags) {
  char buff[DEFAULT_SEND_SIZE + 2];
  bzero(buff, DEFAULT_SEND_SIZE + 2);
  ssize_t retRead = 0;
  ssize_t retSend = 0;
  int& state = _resp.getState();

  if ((retRead = read(fdSrc, buff, DEFAULT_SEND_SIZE)) < 0) {
    state = respState::ioError;
    return;
  }
  if (state & respState::chunkedResp) {
    std::stringstream chunkSize;
    chunkSize << std::hex << retRead << "\r\n";
    std::string chunkData(chunkSize.str());
    chunkData.reserve(chunkData.length() + DEFAULT_SEND_SIZE + 2);
    buff[retRead + 0] = '\r';
    buff[retRead + 1] = '\n';
    chunkData.insert(chunkData.end(), buff, buff + retRead + 2);
    retSend = send(fdDest, chunkData.data(), chunkData.length(), flags);
  } else {
    retSend = send(fdDest, buff, retRead, flags);
  }
  if (retRead == 0) {
    state |= respState::entirelySent;
  }
  if (retSend < 0) {
    state |= respState::ioError;
  }
}

void ResponseHandler::sendFromBuffer(int fdDest, int flags) {
  std::stringstream output;

  logData();
  output << _resp << "\r\n" << _resp.getBuffer();
  if (send(fdDest, output.str().c_str(), output.str().length(), flags) < 0)
    _resp.getState() = respState::ioError;
  else
    _resp.getState() = respState::entirelySent;
}

void ResponseHandler::logData(void) {
#if LOG_LEVEL == LOG_LEVEL_TRACE
  LogStream s;
  if (_requestHandler._req.is_ok())
    s << "REQUEST:\n" << _requestHandler._req.unwrap();
  else
    s << "REQUEST:\n" << _requestHandler._req.unwrap_err();
  s << "RESPONSE:\n" << _resp;
#endif
#if DEBUG_MODE == 1
  if (_requestHandler._req.is_ok())
    std::cout << GREEN << "REQUEST:\n"
              << _requestHandler._req.unwrap() << NC << std::endl;
  else
    std::cout << RED << "REQUEST:\n"
              << _requestHandler._req.unwrap_err() << NC << std::endl;
  std::cout << BLUE << "RESPONSE:\n" << _resp << NC << std::endl;
#endif
}
/* ................................. ACCESSOR ................................*/

/*
 * Returns the result processed. If no call to processRequest was made prior
 * to a call to getResult, result sould not be unwrapped.
 */
Response const& ResponseHandler::getResponse() const { return _resp; }
Request const& ResponseHandler::getRequest() const { return _req; }

/* ................................. OVERLOAD ................................*/

/* ................................... DEBUG .................................*/

/* ................................. END CLASS................................*/
