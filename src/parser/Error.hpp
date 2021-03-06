//
// Created by alena on 17/06/2021.
//

#ifndef WEBSERV_ERROR_HPP
#define WEBSERV_ERROR_HPP

#include <vector>
#include <string>
#include <iostream>
#include "Constants.hpp"
#include "slice.hpp"
#include "utils.hpp"
#include "utils/Logger.hpp"

#include "HTTP/Status.hpp"

template<typename Data = Empty>
class Error
{
private:
	Data										_data;
	std::string									_msg;
	std::vector<std::pair<slice, std::string> >	_stack;

	typedef std::vector<std::pair<slice, std::string> >::const_iterator iterator;

	std::string 	escape(const std::string& str) const
	{
		std::string		out;

		for (size_t i = 0; i < str.length(); i++) {
			if (isprint(str[i]))
				out += str[i];
			else
			{
				unsigned int hex = (unsigned int)(unsigned char)(str[i]);
				if (hex == '\n')
					out += std::string("\\n");
				else if (hex == '\t')
					out += std::string("\\t");
				else if (hex == '\r')
					out += std::string("\\r");
				else {
					std::stringstream stream;
					stream << std::hex << hex;
					std::string code = stream.str();
					out += std::string("\\x") + (code.size() < 2 ?"0" : "") + code;
				}
			}
		}
		return out;
	}

public:

	Error(): _msg("No info") { }
	explicit Error(std::string msg): _msg(msg), _stack() { }
	explicit Error(std::string msg, Data data): _data(data), _msg(msg), _stack() { }

	Error(const Error& other)
	{
		if (&other == this)
			return ;
		this->_msg = other._msg;
		this->_stack = other._stack;
		this->_data = other._data;
	}

	Data	content() const {
		return this->_data;
	}

	void	replace(Data value) {
		this->_data = value;
	}

	/*
	 * Insert a new element in the stacktrace
	 */
	Error		at(const slice &where, std::string info)
	{
		this->_stack.push_back(std::make_pair(where, info));
		return *this;
	}

	friend std::ostream	&operator<<(std::ostream &stream, const Error &err)
	{
		stream << err.escape(err._msg) << std::endl;
		return stream;
	}

	void 	trace(slice start, LogStream &stream) const {
		for (Error::iterator it = _stack.begin(); it != _stack.end(); it++)
			stream << "\tat: (l" << it->first.lines(start) << "c" << it->first.character(start) << "): " << escape(it->first.take(10).until('\n').to_string()) << " (" << it->second << ")\n";
	}
};


Error<status::StatusCode>	error(std::string msg, status::StatusCode = status::None);

#endif //WEBSERV_ERROR_HPP
