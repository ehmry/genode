#ifndef _NICHTS_STORE__DERIVATION_H_
#define _NICHTS_STORE__DERIVATION_H_

#include <string>

#include <nichts/types.h>


static void expect(std::istream & str, const string & s)
{
    char s2[s.size()];
    str.read(s2, s.size());
    if (string(s2, s.size()) != s)
        throw FormatError(format("expected string ‘%1%’") % s);
}


static string parse_string(std::istream & str)
{
    string res;
    expect(str, "\"");
    int c;
    while ((c = str.get()) != '"')
        if (c == '\\') {
            c = str.get();
            if (c == 'n') res += '\n';
            else if (c == 'r') res += '\r';
            else if (c == 't') res += '\t';
            else res += c;
        }
        else res += c;
    return res;
}


static Path parse_path(std::istream & str)
{
	string s = parse_string(str);
	if (s.size() == 0 || s[0] != '/') {
		PERR("bad path %s",  s.c_str());
		throw Nichts_store::Exception();
	}
	return s;
}


static String_set parse_strings(std::istream & str, bool arePaths)
{
    StringSet res;
    while (!end_of_list(str))
        res.insert(arePaths ? parse_path(str) : parse_string(str));
    return res;
}


static bool end_of_list(std::istream & str)
{
    if (str.peek() == ',') {
        str.get();
        return false;
    }
    if (str.peek() == ']') {
        str.get();
        return true;
    }
    return false;
}


namespace Nichts_store {

	Derivation parse_derivation(const string &s)
	{
		Derivation drv;

		std::istringstream str(s);
		expect(str, "Derive([");

		/* Parse the list of outputs. */
		while (!endOfList(str)) {
			DerivationOutput out;
			 expect(str, "("); string id = parse_string(str);
			expect(str, ","); out.path = parse_path(str);
			expect(str, ","); out.hashAlgo = parse_string(str);
			expect(str, ","); out.hash = parse_string(str);
			expect(str, ")");
			drv.outputs[id] = out;
		}

		/* Parse the list of input derivations. */
		 expect(str, ",[");
		while (!end_of_list(str)) {
			expect(str, "(");
			Path drvPath = parse_path(str);
			expect(str, ",[");
			drv.input_drvs[drvPath] = parse_strings(str, false);
			expect(str, ")");
		}

		expect(str, ",["); drv.input_sources = parse_strings(str, true);
		expect(str, ","); drv.platform = parse_string(str);
		expect(str, ","); drv.builder = parse_string(str);

		/* Parse the builder arguments. */
		expect(str, ",[");
		while (!endOfList(str))
			drv.args.push_back(parse_string(str));

		/* Parse the attribute set. */
		expect(str, ",[");
		while (!end_of_list(str)) {
			expect(str, "("); string name = parse_string(str);
			expect(str, ","); string value = parse_string(str);
			expect(str, ")");
			drv.attrs[name] = value;
		}

		 expect(str, ")");
		return drv;
	};

};

#endif