#include "CGI.hpp"

CGI::CGI(void) {
  _status = cgi_status::NON_INIT;
  _pipe = UNSET;
  _child_pid = UNSET;
  _child_return = 0;
}
CGI::~CGI() {
  if (_pipe != UNSET)
    close(_pipe);
}

int CGI::get_pid(void) const { return (_child_pid); }
int CGI::unset_pid(void) { return _child_pid = UNSET; }
int CGI::get_fd(void) const { return (_pipe); }

bool CGI::isPipeEmpty(int fd) const {
  unsigned long bytesAvailable = 0;
  if (ioctl(fd, FIONREAD, &bytesAvailable) < 0) {
    bytesAvailable = 0;
  }
  return bytesAvailable == 0;
}

cgi_status::status CGI::status(void) {

  if (_status == cgi_status::NON_INIT ||
      _status == cgi_status::DONE || STATUS_IS_ERROR(_status)) {
    return _status;
  }
  if (_cgiTimer.getTimeElapsed() >= CGI_TIMEOUT) {
    _status = cgi_status::TIMEOUT;
    return _status;
  }

  int ret = waitpid(_child_pid, &_child_return, WNOHANG);
  if (ret == _child_pid) {
    if (CGI_BAD_EXIT(_child_return)) {
      _status = cgi_status::CGI_ERROR;
    } else {
      _status = cgi_status::DONE;
    }
    unset_pid();
  } else if (ret < 0) {
    _status = cgi_status::SYSTEM_ERROR;
  } else if (ret == 0 && _status != cgi_status::READABLE && isPipeEmpty(_pipe) == false ) {
    _status = cgi_status::READABLE;
  }
  return (_status);
}

std::string const &CGI::getCgiHeader(void) const { return _cgiHeaders; }

void CGI::setCgiHeader(void) {
  if (_cgiHeaders.empty() && isPipeEmpty(_pipe) == false) {
    char cBuff;
    int retRead = 1;
    while ((retRead = read(_pipe, &cBuff, 1)) > 0) {
      _cgiHeaders += cBuff;
      if (_cgiHeaders.size() >= 3 &&
          _cgiHeaders[_cgiHeaders.length() - 3] == '\n' &&
          _cgiHeaders[_cgiHeaders.length() - 2] == '\r' &&
          _cgiHeaders[_cgiHeaders.length() - 1] == '\n')
        break;
    }
  }
}

int CGI::get_readable_pipe(void) const { return (_pipe); }

std::vector<char *> CGI::set_meta_variables(files::File const &file,
                                            Request const &req,
                                            config::Server const &serv) {
  RequestLine req_lines;
  std::vector<char *> variables;

  add_variable("AUTH_TYPE", "");
  add_variable("REMOTE_USER", "");
  add_variable("REMOTE_HOST", "");
  add_variable("REMOTE_IDENT", "");
  if (req.target.query.empty()) {
    add_variable("QUERY_STRING", "");
  } else {
    add_variable("QUERY_STRING", req.target.query);
  }
  add_variable("SERVER_SOFTWARE", "");
  add_variable("REDIRECT_STATUS", "");
  add_variable("PATH_INFO", file.getFileName());
  add_variable("SERVER_PROTOCOL", "HTTP/1.1");
  add_variable("GATEWAY_INTERFACE", "CGI/1.1");
  add_variable("SERVER_PORT", serv.get_port());
  add_variable("SERVER_NAME", serv.get_name());
  add_variable("REMOTE_ADDR", req.get_client_ip());
  add_variable("PATH_TRANSLATED", file.getPath());
  add_variable("SCRIPT_FILENAME", file.getPath());
  if (req.method == methods::GET)
    add_variable("REQUEST_METHOD", "GET");
  else if (req.method == methods::POST)
    add_variable("REQUEST_METHOD", "POST");
  else if (req.method == methods::DELETE)
    add_variable("REQUEST_METHOD", "DELETE");
  else {
    _status = cgi_status::UNSUPPORTED;
    return variables;
  }

  add_variable("CONTENT_LENGTH",
               req.get_header("Content-Length").unwrap_or(""));
  add_variable("CONTENT_TYPE", req.get_header("Content-Type").unwrap_or(""));

  return variables;
}

int CGI::execute_cgi(std::string const &cgi_path, files::File const &file,
                      Request const &req, config::Server const &serv) {

  _status = cgi_status::NON_INIT;

  int output[2];
  int input[2];
  if (pipe(output) < 0 || pipe(input) < 0) {
    _status = cgi_status::SYSTEM_ERROR;
    return UNSET;
  }

  size_t i = 0;
  set_meta_variables(file, req, serv);

  char *cgi = strdup(cgi_path.c_str());
  if (cgi == NULL || file.isGood() == false) {
    _status = cgi_status::SYSTEM_ERROR;
    free(cgi);
    return UNSET;
  }

  char *env[_variables.size() + 1];
  for (; i < _variables.size();) {
    env[i] = _variables[i];
    i++;
  }
  env[i] = NULL;

  char *args[] = {cgi, NULL};

  _child_pid = fork();
  if (_child_pid < 0) {
    perror("System Error : fork()");
    _status = cgi_status::SYSTEM_ERROR;
    free(cgi);
    for (i = 0; i < _variables.size(); i++) {
      free(env[i]);
    }
    return UNSET;
  }
  if (_child_pid == 0) {
    std::string exec_path = files::File::getDirFromPath(file.getPath());

    if (dup2(output[1], 1) < 0 || dup2(input[0], 0) < 0) {
      close(input[1]);
      close(input[0]);
      close(output[1]);
      close(output[0]);
      exit(-1);
    }
    if (chdir(exec_path.c_str()) < 0) {
      exit(-1);
    }
    close(input[1]);
    close(input[0]);
    close(output[1]);
    close(output[0]);
    execve(args[0], args, env);
    exit(-1);
  } else {

    _cgiTimer.start();
    _status = cgi_status::WAITING;
    free(cgi);
    for (i = 0; i < _variables.size(); i++) {
      free(env[i]);
    }
    close(input[0]);
    close(output[1]);
    _pipe = output[0];
    fcntl(_pipe, F_SETFL, O_NONBLOCK);
    if (req.method != methods::POST || req.get_body().empty()) {
      close(input[1]);
      return UNSET;
    } else {
      fcntl(input[1], F_SETFL, O_NONBLOCK);
      return input[1];
    }
  }
}
