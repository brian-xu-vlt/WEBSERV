#ifndef LOGGER_HPP
# define LOGGER_HPP

# include <iostream>
# include <sstream>
# include <string>

class LogStream;

class Logger
{
	public:

		// log a new line
		static bool		log( std::string const & message, int dest = Logger::_defaultDest);

		// set Verbose, true by default
		static void		setVerbose(bool setting = true);
		static void		setDefaultDest(int setting = Logger::toFile);

		// instanciate a new logger with: verbose = true and defaultDest = to file.
		static Logger&	getInstance(std::string const destFile = "./webserv.log",
														const int defaultDest = Logger::toFile);
		static const int	toConsole;
		static const int	toFile;

		static int 			dest();

		~Logger();

	private:

		static bool				_verbose;
		static int				_defaultDest;
		static std::string 		_fileName;

		Logger( std::string const destFile, int defaultDest);
		static std::string		_makeLogEntry(std::string rawEntry);
};

class LogStream
{
	public:
		LogStream(int dest = Logger::getInstance().dest());

		std::string		raw();

		~LogStream();

		template<typename T>
		LogStream	&operator<<(const T& data)
		{
			_inner << data;
			return *this;
		}

	private:
		std::stringstream	_inner;
		int 				_dest;
};

#endif /* ********************************************************** LOGGER_H */
