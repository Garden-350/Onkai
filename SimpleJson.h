#pragma once
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <fstream>
#include <sstream>
#include <cctype>
#include <stdexcept>

class JsonValue
{
public:
	enum class Type {
		Null,
		Bool,
		Number,
		String,
		Array,
		Object
	};

	Type type = Type::Null;
	bool boolValue = false;
	double numberValue = 0.0f;
	std::string stringValue;
	std::vector<JsonValue> arrayValue;
	std::map<std::string, JsonValue>objectValue;

	bool IsObject() const { return type == Type::Object; }
	bool IsArray() const { return type == Type::Array; }

	bool Has(const std::string& key) const
	{
		return type == Type::Object && objectValue.find(key) != objectValue.end();
	}

	const JsonValue& operator[](const std::string& key) const
	{
		static JsonValue nullVal;
		auto it = objectValue.find(key);
		if (it == objectValue.end()) return nullVal;
		return it->second;
	}
	float AsFloat(float def = 0.0f) const
	{
		return type == Type::Number ? static_cast<float>(numberValue) : def;
	}
	int AsInt(int def = 0) const
	{
		return type == Type::Number ? static_cast<int>(numberValue) : def;
	}
	std::string AsString(const std::string& def = "") const
	{
		return type == Type::String ? stringValue : def;
	}
};

class SimpleJson
{
public:
	//ファイルをまるごと読んでパースする。失敗ならfalse
	static bool ParseFile(const std::string& path, JsonValue& out, std::string* errorOut = nullptr)
	{
		std::ifstream file(path);
		if (!file.is_open())
		{
			if (errorOut) *errorOut = "file not found" + path;
			return false;
		}
		std::stringstream ss;
		ss << file.rdbuf();
		std::string text = ss.str();

		try
		{
			size_t pos = 0;
			SkipWhitespace(text, pos);
			out = ParseValue(text, pos);
			return true;
		}
		catch (const std::exception& e)
		{
			if (errorOut) *errorOut = e.what();
			return false;
		}
	}

private:
	static void SkipWhitespace(const std::string& s, size_t& pos)
	{
		while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t' || s[pos] == '\n' || s[pos] == '\r'))
		{
			pos++;
		}
	}

	static JsonValue ParseValue(const std::string& s, size_t& pos)
	{
		SkipWhitespace(s, pos);
		if (pos >= s.size()) throw std::runtime_error("unexpected end of JSON");

		char c = s[pos];
		if (c == '{') return ParseObject(s, pos);
		if (c == '[') return ParseArray(s, pos);
		if (c == '"') return ParseString(s, pos);
		if (c == 't' || c == 'f') return ParseBool(s, pos);
		if (c == 'n') { pos += 4; JsonValue v; v.type = JsonValue::Type::Null; return v; }
		return ParseNumber(s, pos);
	}

	static JsonValue ParseObject(const std::string& s, size_t& pos)
	{
		JsonValue v; 
		v.type = JsonValue::Type::Object;
		pos++; // '{'
		SkipWhitespace(s, pos);
		if (pos < s.size() && s[pos] == '}') { pos++; return v; }

		while (true)
		{
			SkipWhitespace(s, pos);
			JsonValue key = ParseString(s, pos);
			SkipWhitespace(s, pos);
			if (pos >= s.size() || s[pos] != ':') throw std::runtime_error("expected ':' in object");
			pos++; // ':'
			JsonValue val = ParseValue(s, pos);
			v.objectValue[key.stringValue] = val;

			SkipWhitespace(s, pos);
			if (pos < s.size() && s[pos] == ',') { pos++; continue; }
			if (pos < s.size() && s[pos] == '}') { pos++; break; }
			throw std::runtime_error("expected ',' or '}' in object");
		}
		return v;
	}

	static JsonValue ParseArray(const std::string& s, size_t& pos)
	{
		JsonValue v;
		v.type = JsonValue::Type::Array;
		pos++;	//'{'
		SkipWhitespace(s, pos);
		if (pos < s.size() && s[pos] == ']') { pos++; return v; }

		while (true)
		{
			JsonValue val = ParseValue(s, pos);
			v.arrayValue.push_back(val);

			SkipWhitespace(s, pos);
			if (pos < s.size() && s[pos] == ',') { pos++; continue; }
			if (pos < s.size() && s[pos] == ']') { pos++; break; }
			throw std::runtime_error("expected ',' or ']' in array");
		}
		return v;
	}

	static JsonValue ParseString(const std::string& s,size_t& pos)
	{
		if (s[pos] != '"') throw std::runtime_error("expected string");
		pos++;	//'"'
		std::string result;
		while(pos < s.size() && s[pos] != '"')
		{
			if (s[pos] == '\\' && pos + 1 < s.size())
			{
				pos++;
				switch (s[pos])
				{
				case 'n': result += '\n'; break;
				case 't': result += '\t'; break;
				case '"': result += '"'; break;
				case '\\': result += '\\'; break;
				case '/': result += '/'; break;
				default: result += s[pos]; break;
				}
				pos++;
			}
			else
			{
				result += s[pos];
				pos++;
			}
		}
		if (pos >= s.size()) throw std::runtime_error("unterminated string");
		pos++;	//closing '"'

		JsonValue v;
		v.type = JsonValue::Type::String;
		v.stringValue = result;
		return v;
	}

	static JsonValue ParseBool(const std::string& s, size_t& pos)
	{
		JsonValue v;
		v.type = JsonValue::Type::Bool;
		if (s.compare(pos, 4, "true") == 0) { v.boolValue = true; pos += 4; }
		else if (s.compare(pos, 5, "false") == 0) { v.boolValue = false; pos += 5; }
		else throw std::runtime_error("invalid literal");
		return v;
	}

	static JsonValue ParseNumber(const std::string& s, size_t& pos)
	{
		size_t start = pos;
		if (pos < s.size() && (s[pos] == '-' || s[pos] == '+')) pos++;
		while (pos < s.size() && (isdigit((unsigned char)s[pos]) || s[pos] == '.' ||
			s[pos] == 'e' || s[pos] == 'E' || s[pos] == '+' || s[pos] == '-'))
			pos++;
		std::string numStr = s.substr(start, pos - start);
		if (numStr.empty()) throw std::runtime_error("invalid number");

		JsonValue v; v.type = JsonValue::Type::Number;
		v.numberValue = std::stod(numStr);
		return v;
	}
};

