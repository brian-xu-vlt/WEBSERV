#include "Response.hpp"

/* ............................... CONSTRUCTOR ...............................*/

Response::Response()
    : _respState(respState::emptyResp),
      _version(Version('1', '1')),
      _statusCode(status::None),
      _uploadFd(UNSET) {
  setHeader("Cache-Control", "no-cache");
  setHeader("Connection", "close");
  setHeader(headerTitle::Date, Timer::getTimeNow());
  setHeader(headerTitle::Server, WEBSERV_NAME);
}

Response::Response(Version version, status::StatusCode statusCode)
    : _respState(respState::emptyResp),
      _version(version),
      _statusCode(statusCode),
      _uploadFd(UNSET) {
  setHeader("Cache-Control", "no-cache");
  setHeader("Connection", "close");
  setHeader(headerTitle::Date, Timer::getTimeNow());
  setHeader(headerTitle::Server, WEBSERV_NAME);
}

/* ..............................COPY CONSTRUCTOR.............................*/

/* ................................ DESTRUCTOR ...............................*/

Response::~Response() {}

/* ................................. METHODS .................................*/

void Response::reset(Version const& vers, status::StatusCode code) {
  _respState = respState::emptyResp;
  _file.closeFile();
  _version = vers;
  _statusCode = code;
  _headers.clear();
}

void Response::setVersion(const Version& version) { _version = version; }
void Response::setStatus(status::StatusCode code) { _statusCode = code; }

void Response::setHeader(std::string const& field, std::string value) {
  _headers.insert(std::make_pair(field, std::make_pair(field, value)));
}

void Response::setHeader(headerTitle::Title title, std::string value) {
  std::string titleStr = headerTitle::HeaderTitleField::get(title);
  _headers.insert(std::make_pair(titleStr, std::make_pair(titleStr, value)));
}

void Response::setHeader(std::string const& field, int value) {
  std::stringstream strValue;
  strValue << value;
  setHeader(field, strValue.str());
}

void Response::setHeader(headerTitle::Title title, int value) {
  std::string titleStr = headerTitle::HeaderTitleField::get(title);
  std::stringstream strValue;
  strValue << value;
  setHeader(titleStr, strValue.str());
}

files::File & Response::setFile(std::string const& filePath) {
  _file.init(filePath);
  return _file;
}

files::File & Response::setUploadFile(std::string const& filePath) {
  _uploadFile.init(filePath, O_TRUNC | O_WRONLY, 0644);
  _uploadFd = _uploadFile.getFD();
  return _file;
}

CGI & Response::getCgiInst(void)  { return _cgi; }
CGI const & Response::getCgiInst(void) const { return _cgi; }
int Response::getCgiFD(void) const { return _cgi.get_readable_pipe(); }

files::File const& Response::getFileInst(void) const { return _file; }
files::File & Response::getFileInst(void) { return _file; }
int Response::getFileFD(void) const { return _file.getFD(); }

void	Response::setUploadFd( int fd ) { _uploadFd = fd; }
int		Response::getOutputFd( void ) const {
  if (getState() & respState::cgiResp)
    return _cgi.get_readable_pipe();
  else
    return _file.getFD() ;
  }
int		Response::getUploadFd( void ) const { return _uploadFd; }
status::StatusCode Response::getStatusCode(void) const { return _statusCode; }
std::string& Response::getBuffer(void) { return _htmlBuffer; }
int& Response::getState(void) { return _respState; }
int const & Response::getState(void) const { return _respState; }

void Response::loadErrorHtmlBuffer(const status::StatusCode& code,
                                   const std::string& optionalMessage) {

  if (optionalMessage.empty()) {
    std::stringstream tmpBuff;
    tmpBuff << "<html>" << '\n'
            << "<head>" << '\n'
            << "	<title>" << code;
  if (status::StatusMessage::get(code).empty() == false) {
    tmpBuff << ' ' << status::StatusMessage::get(code);
  }
    tmpBuff << "</title>" << '\n'
            << "</head>" << '\n'
            << "<body>" << '\n'
            << "	<center>" << '\n';

  if (code >= 400) {
    tmpBuff << "		<h1> Oopsy ! That's an error. </h1>" << '\n';
  }

    tmpBuff << "		<h1> " << code;
  if (status::StatusMessage::get(code).empty() == false) {
    tmpBuff << ' ' << status::StatusMessage::get(code);
  }
    tmpBuff << " </h1>" << '\n'
            << "  </center>" << '\n'
            << "	<hr>" << '\n'
            << "	<center>Webserv Team ABC</center>" << '\n'
            << "</body>" << '\n'
            << "</html>" << '\n';
    _htmlBuffer.assign(tmpBuff.str());
  } else {
    _htmlBuffer.assign(optionalMessage);
  }
}

/* ................................. ACCESSOR ................................*/

/* ................................. OVERLOAD ................................*/

std::ostream& operator<<(std::ostream& o, Response const& i) {
  // Writes status line
  o << "HTTP/" << i._version << " " << i._statusCode << " "
    << status::StatusMessage::get(i._statusCode) << "\r\n";

  // Writes headers
  if (i._headers.empty() == false) {
    Response::headerMap_t::const_iterator it = i._headers.begin();
    for (; it != i._headers.end(); it++) {
      o << it->first << ": " << it->second.second << "\r\n";
    }
  }

  return o;
}

/* ................................... DEBUG .................................*/

/* ................................. END CLASS................................*/
