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


namespace Nichts_store {

	Derivation parse_derivation(const string &s)
	{
		Derivation drv;

		char *p = raw;




    std::istringstream str(s);
    expect(str, "Derive([");

    /* Parse the list of outputs. */
    while (!endOfList(str)) {
        DerivationOutput out;
        expect(str, "("); string id = parseString(str);
        expect(str, ","); out.path = parsePath(str);
        expect(str, ","); out.hashAlgo = parseString(str);
        expect(str, ","); out.hash = parseString(str);
        expect(str, ")");
        drv.outputs[id] = out;
    }

    /* Parse the list of input derivations. */
    expect(str, ",[");
    while (!endOfList(str)) {
        expect(str, "(");
        Path drvPath = parsePath(str);
        expect(str, ",[");
        drv.inputDrvs[drvPath] = parseStrings(str, false);
        expect(str, ")");
    }

    expect(str, ",["); drv.inputSrcs = parseStrings(str, true);
    expect(str, ","); drv.platform = parseString(str);
    expect(str, ","); drv.builder = parseString(str);

    /* Parse the builder arguments. */
    expect(str, ",[");
    while (!endOfList(str))
        drv.args.push_back(parseString(str));

    /* Parse the environment variables. */
    expect(str, ",[");
    while (!endOfList(str)) {
        expect(str, "("); string name = parseString(str);
        expect(str, ","); string value = parseString(str);
        expect(str, ")");
        drv.env[name] = value;
    }

    expect(str, ")");
    return drv;
	};

	corruption:

};

#endif