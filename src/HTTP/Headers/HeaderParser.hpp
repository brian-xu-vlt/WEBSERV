//
// Created by alena on 28/07/2021.
//

#ifndef WEBSERV_HEADERPARSER_HPP
#define WEBSERV_HEADERPARSER_HPP

#include "Header.hpp"

template<typename Name = FieldName, typename P = FieldValue>
class HeaderParser: public Parser<Header>
{
private:
	P				_field;
	Name			_name;

public:
	HeaderParser(): _field(FieldValue()), _name(FieldName()) { }
	HeaderParser(std::string name, P parser): _field(parser), _name(Tag(name)) { }

	result_type		operator()(const slice &input)
	{
		typename Name::result_type name = terminated(_name, sequence(Char(':'), ows))(input);
		if (name.is_err())
			return name.template convert<Header>();
		slice	left = name.left();
		typename P::result_type	value = as_slice(_field)(left);
		if (value.is_err())
			return value.failure().map_err(left.size ? status::BadRequest : status::None).template convert<Header>();
		return result_type::ok(value.left(), Header(name.unwrap(), value.unwrap()));
	}
};

template<typename Value>
HeaderParser<Tag, Value>	header(std::string name, Value parser)
{
	return HeaderParser<Tag, Value>(name, parser);
}

#endif //WEBSERV_HEADERPARSER_HPP