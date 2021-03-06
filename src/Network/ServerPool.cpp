#include "ServerPool.hpp"

namespace network {

std::vector<config::Server> ServerPool::_serverPool;

/* ............................... CONSTRUCTOR ...............................*/

/* ..............................COPY CONSTRUCTOR.............................*/

/* ................................ DESTRUCTOR ...............................*/

ServerPool::~ServerPool(void) {}

/* ................................. METHODS .................................*/

/*
 * Parses configuration file and update locations blocks with server
 * inherited data where needed
 */
void ServerPool::init(const std::string &configFilePath) {
  _serverPool = config::parse(configFilePath);
  if (isServerPoolValid() == false) exit(1);
  std::for_each(_serverPool.begin(), _serverPool.end(),
                ServerPool::locationsInit);
}

/*
 *  Checks if any server have the same port and name
 */
bool ServerPool::isServerPoolValid() {
  std::vector<config::Server>::const_iterator it = _serverPool.begin();
  std::vector<config::Server>::const_iterator ite = _serverPool.end();
  std::map<std::string, std::vector<int> > processed;

  for (; it != ite; ++it) {
    std::vector<int> &serverPorts = processed[it->get_name()];
    if (std::find(serverPorts.begin(), serverPorts.end(), it->get_port()) ==
        serverPorts.end()) {
      serverPorts.push_back(it->get_port());
    } else {
      std::cerr << RED
                << "Configuration error, some server definitions are "
                   "overlaping on port "
                << it->get_port() << NC << std::endl;
      return false;
    }
  }
  return true;
}

/*
 * Inherite data from server in each locations where it was not explicitly set
 */
void ServerPool::locationsInit(config::Server &serv) {
  std::vector<LocationConfig> &locations = serv.get_locations();
  if (locations.empty() == false) {
    std::vector<LocationConfig>::iterator it = locations.begin();
    std::vector<LocationConfig>::iterator ite = locations.end();
    for (; it != ite; it++) {
      std::string & path = it->get_path();
      if (path != "/" && path[path.length() - 1] == '/')
        path.resize(path.length() - 1);
      it->_root.assign((it->_root.empty()) ? serv.get_root() : it->_root);
      it->_index.assign((it->_index.empty()) ? serv.get_index() : it->_index);
      it->_body_size = (it->_body_size == LocationConfig::SIZE_UNSET)
                           ? serv.get_body_size()
                           : it->_body_size;
    }
  }
}

/* ................................. ACCESSOR ................................*/

const std::vector<config::Server> &ServerPool::getPool(void) {
  return _serverPool;
}

std::set<int> ServerPool::getPorts(void) {
  std::set<int> ports;
  std::vector<config::Server>::iterator it = _serverPool.begin();
  for (; it != _serverPool.end(); it++) {
    ports.insert(it->get_port());
  }
  return ports;
}

config::Server const &ServerPool::getServerMatch(std::string hostHeader,
                                                 int receivedPort) {
  // clean the host header to get the IP/name part only
  size_t portPos = hostHeader.find(':');
  if (portPos != std::string::npos) hostHeader.resize(portPos);

  // iterate in reverse order on vector to get the best match
  std::vector<config::Server>::reverse_iterator it = _serverPool.rbegin();
  std::vector<config::Server>::reverse_iterator bestCandidate =
      _serverPool.rend();
  for (; it != _serverPool.rend(); it++) {
    if (it->get_port() == receivedPort) bestCandidate = it;

    if (it->get_port() == receivedPort &&
        (it->get_address() == hostHeader || it->get_name() == hostHeader))
      return *it;
  }
  return *bestCandidate;
}

/*
 * Try to match the target with any location path, if none, reduce one element
 * of the path and try again, until the target is empty, then no location can
 * be resolved so a location is created with the parent server infos.
 */
LocationConfig const ServerPool::getLocationMatch(config::Server const &serv,
                                                  Target const &target) {
  std::vector<LocationConfig> const &locs = serv.get_locations();

  if (locs.empty() == false) {
    std::vector<LocationConfig>::const_iterator it;
    std::string targetPath = target.decoded_path;

    if (files::File::isFileFromPath(targetPath)) {
      targetPath.resize(targetPath.find_last_of('/') + 1);
    }
    while (targetPath.empty() == false) {
      for (it = locs.begin(); it != locs.end(); it++) {
        if (targetPath == it->get_path() || targetPath + '/' == it->get_path())
          return *it;
      }
      if (targetPath.find_last_of('/') == targetPath.find_first_of('/') &&
          targetPath.size() > 1)
        targetPath.resize(targetPath.find_last_of('/') + 1);
      else
        targetPath.resize(targetPath.find_last_of('/'));
    }
  }
  return (LocationConfig(serv));
}

/* ................................. OVERLOAD ................................*/

/* ................................... DEBUG .................................*/

/* ................................. END CLASS................................*/

}  // end namespace network
