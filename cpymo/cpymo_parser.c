#include "cpymo_parser.h"
#include <ctype.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

static char cpymo_parser_readchar(cpymo_parser *parser) 
{
	if (parser->cur_pos >= parser->stream.len) {
		parser->is_line_end = true;
		return '\0';
	}

	char ret = parser->stream.begin[parser->cur_pos];
	parser->cur_pos++;
	parser->is_line_end = false;

	if (ret == '\r') return cpymo_parser_readchar(parser);
	else { 
		if (ret == '\n') {
			parser->cur_line++;
			parser->is_line_end = true;
		}
		return ret; 
	}
}

void cpymo_parser_init(cpymo_parser *parser, const char *stream, size_t len)
{
	parser->stream.begin = stream;
	parser->stream.len = len;
	cpymo_parser_reset(parser);
}

void cpymo_parser_reset(cpymo_parser *parser)
{
	parser->cur_pos = 0;
	parser->cur_line = 0;
	parser->is_line_end = false;

	// Remove UTF8 BOM
	if (parser->stream.len >= 3) {
		if ((unsigned char)parser->stream.begin[0] == 0xEF &&
			(unsigned char)parser->stream.begin[1] == 0xBB &&
			(unsigned char)parser->stream.begin[2] == 0xBF) 
			parser->cur_pos += 3;
	}
}

bool cpymo_parser_next_line(cpymo_parser * parser)
{
	while (!parser->is_line_end) {
		const char ch = cpymo_parser_readchar(parser);

		if (ch == '\0') return false;
	}

	if (parser->cur_pos < parser->stream.len) {
		parser->is_line_end = false;
		return true;
	}
	else return false;
}

char cpymo_parser_curline_readchar(cpymo_parser * parser)
{
	if (parser->is_line_end) return '\0';
	else {
		char ch = cpymo_parser_readchar(parser);
		if (ch == '\n') return '\0';
		else return ch;
	}
}

cpymo_parser_stream_span cpymo_parser_curline_readuntil(cpymo_parser * parser, char until)
{
	return cpymo_parser_curline_readuntil_or(parser, until, '\0');
}

cpymo_parser_stream_span cpymo_parser_curline_readuntil_or(cpymo_parser * parser, char until1, char until2)
{
	return cpymo_parser_curline_readuntil_or3(parser, until1, until2, '\0');
}

cpymo_parser_stream_span cpymo_parser_curline_readuntil_or3(cpymo_parser * parser, char until1, char until2, char until3)
{
	cpymo_parser_stream_span span;
	span.begin = parser->stream.begin + parser->cur_pos;
	span.len = 0;

	char ch;
	while ((ch = cpymo_parser_curline_readchar(parser))) {
		if (ch == until1 || ch == until2 || ch == until3) break;
		span.len++;
	}

	return span;
}

cpymo_parser_stream_span cpymo_parser_curline_pop_commacell(cpymo_parser * parser)
{
	cpymo_parser_stream_span span = cpymo_parser_curline_readuntil(parser, ',');
	cpymo_parser_stream_span_trim(&span);
	return span;
}

cpymo_parser_stream_span cpymo_parser_curline_pop_command(cpymo_parser * parser)
{
	cpymo_parser_stream_span before_command = cpymo_parser_curline_readuntil(parser, '#');
	cpymo_parser_stream_span_trim(&before_command);
	if (before_command.len == 0) {
		cpymo_parser_stream_span command = cpymo_parser_curline_readuntil_or(parser, ' ', '\t');
		cpymo_parser_stream_span_trim(&command);
		return command;
	} 
	else {
		cpymo_parser_stream_span ret;
		ret.begin = NULL;
		ret.len = 0;
		return ret;
	}
}

cpymo_parser_stream_span cpymo_parser_stream_span_pure(const char *s)
{
	cpymo_parser_stream_span r;
	r.begin = s;
	r.len = strlen(s);
	return r;
}

void cpymo_parser_stream_span_trim_start(cpymo_parser_stream_span * span)
{
	size_t i;
	for (i = 0; i < span->len; ++i)
		if (span->begin[i] < 0 || !isblank(span->begin[i]))
			break;

	span->begin += i;
	span->len -= i;
}

void cpymo_parser_stream_span_trim_end(cpymo_parser_stream_span * span)
{
	if (span->len) {
		if (span->begin[span->len - 1] > 0 && isblank(span->begin[span->len - 1])) {
			span->len--;
			cpymo_parser_stream_span_trim_end(span);
		}
	}
}

void cpymo_parser_stream_span_copy(char *dst, size_t buffer_size, cpymo_parser_stream_span span)
{
	size_t copy_count = buffer_size - 1;
	if (span.len < copy_count) copy_count = span.len;

	strncpy(dst, span.begin, copy_count);
	dst[copy_count] = '\0';
}

int cpymo_parser_stream_span_atoi(cpymo_parser_stream_span span)
{
	char buf[16];
	cpymo_parser_stream_span_trim(&span);
	cpymo_parser_stream_span_copy(buf, sizeof(buf), span);
	return atoi(buf);
}

void cpymo_parser_stream_span_trim(cpymo_parser_stream_span *span) 
{
	cpymo_parser_stream_span_trim_end(span);
	cpymo_parser_stream_span_trim_start(span);
}

static uint8_t from_hex_char(char c)
{
	if (c >= '0' && c <= '9') return c - '0';
	else if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	else if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	else return 0;
}

cpymo_color cpymo_parser_stream_span_as_color(cpymo_parser_stream_span span) 
{
	cpymo_parser_stream_span_trim(&span);
	if (span.len != 7) return cpymo_color_error();

	if (span.begin[0] != '#') return cpymo_color_error();

	for (size_t i = 0; i < 6; ++i) 
		if (span.begin[i + 1] < 0 || !isxdigit((int)span.begin[i + 1]))
			return cpymo_color_error();
	
	cpymo_color c;
	c.r = from_hex_char(span.begin[1]) * 16 + from_hex_char(span.begin[2]);
	c.g = from_hex_char(span.begin[3]) * 16 + from_hex_char(span.begin[4]);
	c.b = from_hex_char(span.begin[5]) * 16 + from_hex_char(span.begin[6]);

	return c;
}

bool cpymo_parser_stream_span_equals_str(cpymo_parser_stream_span span, const char * str)
{
	if (*str == '\0' && span.len == 0) return true;
	else if (*str == '\0' || span.len == 0) return false;
	else if (*str == span.begin[0]) {
		span.begin++;
		span.len--;
		return cpymo_parser_stream_span_equals_str(span, str + 1);
	}
	else return false;
}

bool cpymo_parser_stream_span_equals(cpymo_parser_stream_span a, cpymo_parser_stream_span b)
{
	if (a.len == 0 && b.len == 0) return true;
	else if (a.len == 0 || b.len == 0) return false;
	else if (a.begin[0] == b.begin[0]) {
		a.begin++; a.len--;
		b.begin++; b.len--;
		return cpymo_parser_stream_span_equals(a, b);
	}
	else return false;
}
