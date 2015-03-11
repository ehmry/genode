#ifndef _NICHTS__DERIVATION_H_
#define _NICHTS__DERIVATION_H_


/*Genode includes. */
#include <util/list.h>
#include <util/token.h>

/* Stdcxx */
#include <sstream>
#include <string>

#include "types.h"

using std::string;

namespace Nichts {

static void expect(std::istream & str, const string & s)
{
    char s2[s.size()];
    str.read(s2, s.size());
    if (string(s2, s.size()) != s)
		throw Nichts_store::Build_failure();
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


static String_set parse_strings(std::istream & str, bool arePaths)
{
    String_set res;
    while (!end_of_list(str))
        res.insert(arePaths ? parse_path(str) : parse_string(str));
    return res;
}

	class Derivation {

		typedef ::Genode::Token<Genode::Scanner_policy_identifier_with_underline> Token;

		struct Output : public Genode::List<Output>::Element {
			Token id;
			Token path;
			Token hash_algo;
			Token hash;
		};

		struct Input : public Genode::List<Input>::Element {
			Token path;
			Genode::List<Genode::List_element<Token> > ids;		

			~Input()
			{
				while (i->ids.first()) {
					Token *t = ids.first();
					ids.remove(t);
					destroy(Genode::env()->heap(), t);
				}
			}
		};

		private:

			Genode::List<Output> outputs;
			Genode::List<Input> inputs;
			Derivation_inputs  input_drvs; /* inputs that are sub-derivations */
			Path_set           input_sources;
			string             platform;
			Path               builder;
			Strings            args;
			String_map         attrs;

			static Token eat_comma(Token t)
			{
				if (t[0] == ',')
					return t.next();
				throw Build_failure();
			}

		public:

			/**
			 * Constructor
			 */
			Derivation(char const *str)
			{
				if (strncmp(s, "Derive", sizeof("Derive"))) {
					PERR("bad derivation %s", s);
					throw Build_failure();
				}
				Token t(s, strlen(s)).next();

				for (t = t.next(); t[0] != ']'; t = t.next()) {
					Output *o = new (Genode::env()->heap()) Output;

					o->id = t = t.next();
					o->path = t = eat_comma(t);
					o->hash_algo = t = eat_comma(t);
					o->hash = t = eat_comma(t);
					outputs.insert(o);
				}

				for (t = t.next(); t[0] != ']'; t = t.next()) {
					Input *i = new (Genode::env()->heap()) Input;
					i->path = t = t.next();
					for (t = t.next(); t[0] != ']'; t = t.next()) {
						
					}
					inputs.insert(i);
				}

			}

			~Derivation()
			{
				
				while (outputs.first()) {
					Output *o = outputs.first();
					outputs.remove(o);
					destroy(Genode::env()->heap(), o);
				}

				while (inputs.first()) {
					Input *i = inputs.first();
					inputs.remove(i);
					destroy(Genode::env()->heap(), i);
				}

			}
	}

	Derivation parse_derivation(string const &s)
	{
		Derivation drv;
		std::istringstream str(s);
		expect(str, "Derive([");

		/* Parse the list of outputs. */
		while (!end_of_list(str)) {
			Derivation_output out;
			 expect(str, "("); string id = parse_string(str);
			expect(str, ","); out.path = parse_path(str);
			expect(str, ","); out.hash_algo = parse_string(str);
			expect(str, ","); out.hash = parse_string(str);
			expect(str, ")");
			drv.outputs[id] = out;
		}

		/* Parse the list of input derivations. */
		 expect(str, ",[");
		while (!end_of_list(str)) {
			expect(str, "(");
			Nichts::Path drv_path = parse_path(str);
			expect(str, ",[");
			drv.input_drvs[drv_path] = parse_strings(str, false);
			expect(str, ")");
		}

		expect(str, ",["); drv.input_sources = parse_strings(str, true);
		expect(str, ","); drv.platform = parse_string(str);
		expect(str, ","); drv.builder = parse_string(str);

		/* Parse the builder arguments. */
		expect(str, ",[");
		while (!end_of_list(str))
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

#endif /* _NICHTS__DERIVATION_H_ */
