#ifndef _NICHTS_STORE__DERIVATION_H_
#define _NICHTS_STORE__DERIVATION_H_

//#include <nichts_store_session/nichts_store_session.h>

/*Genode includes. */
#include <base/env.h>
#include <util/list.h>
#include <util/string.h>
#include <util/token.h>


namespace Nichts_store {

	template <typename T>
	static void clear(Genode::List<T> list)
	{
		while (list.first()) {
			T *e = list.first();
			list.remove(e);
			destroy(Genode::env()->heap(), e);
		}
	}

	class Derivation {

		public:

			struct Invalid : public Genode::Exception { };

			typedef ::Genode::Token<Genode::Scanner_policy_identifier_with_underline> Token;

		private:

		Token eat_list(Token &t)
		{
			if (*(t.start()) == '[') return t.next();
			throw Invalid();
		}

		Token end_list(Token &t)
		{
			if (*(t.start()) == ']') return t.next();
			throw Invalid();
		}

		Token eat_tuple(Token &t)
		{
			if (*(t.start()) == '(') return t.next();
			throw Invalid();
		}

		Token end_tuple(Token &t)
		{
			if (*(t.start()) == ')') return t.next();
			throw Invalid();
		}

		Token eat_string(Token &t)
		{
			if (t.type() == Token::STRING) return t.next();
			throw Invalid();
		}
	
		Token eat_comma(Token &t)
		{
			if (*(t.start()) == ',') return t.next();
			throw Invalid();
		}

		/* TODO: PATH_MAX_LEN is already defined in the RPC session. */
		enum { ID_MAX_LEN = 128 };
		enum { PATH_MAX_LEN = 512 };
		enum { ARG_MAX_LEN = 1024 };

		class Id :
			public Genode::String<ID_MAX_LEN>,
			public Genode::List<Id>::Element
		{
			public:
				Id() { };
				Id(Token &t)
					: Genode::String<ID_MAX_LEN>(t.start(), t.len()) { };
		};

		class Path :
			public Genode::String<PATH_MAX_LEN>,
			public Genode::List<Path>::Element
		{
			public:
				Path() { };
				Path(Token &t)
					: Genode::String<PATH_MAX_LEN>(t.start(), t.len()) { };
		};

		class Arg :
			public Genode::String<ARG_MAX_LEN>,
			public Genode::List<Arg>::Element
		{
			public:
				Arg() { };
				Arg(Token &t)
					: Genode::String<ARG_MAX_LEN>(t.start(), t.len()) { };
		};

		struct Output : public Genode::List<Output>::Element {
			Id id;
			Path path;
			Token hash_algo;
			Token hash;
		};

		struct Input : public Genode::List<Input>::Element {
			Token path;
			Genode::List<Id> ids;

			~Input()
			{
				while (ids.first()) {
					auto t = ids.first();
					ids.remove(t);
					destroy(Genode::env()->heap(), t);
				}
			}
		};

		public:

			struct Env_pair : public Genode::List<Env_pair>::Element {
				Token key, value;
				Env_pair (Token &k, Token &v) : key(k), value(v) { };
			};

			Genode::List<Output>   outputs;
			Genode::List<Input>    inputs;
			Genode::List<Path>     sources;
			Token                  platform;
			Path                   _builder;
			Genode::List<Arg>      args;
			Genode::List<Env_pair> _env;

			/**
			 * Constructor
			 */
			Derivation(char const *buf, int len)
			{
				if (Genode::strcmp(buf, "Derive", sizeof("Derive") -1 )) {
					throw Invalid();
				}
				Token t(buf, len);
				t = t.next(); t = eat_tuple(t);

				/*******************
				 ** Read outputs. **
				 *******************/
				t = eat_list(t);
				for (t = eat_tuple(t); ; t = eat_tuple(t)) {
					Output *o = new (Genode::env()->heap()) Output;
					o->id = Id(t);                   t = eat_string(t);
					t = eat_comma(t);
					o->path = Path(t);           t = eat_string(t);

					t = o->hash_algo = eat_comma(t); t = eat_string(t);
					t = o->hash = eat_comma(t);      t = eat_string(t);
					outputs.insert(o);

					t = end_tuple(t);
					if (t[0] != ',') break;
					t = eat_comma(t);
				};
				t = end_list(t);
				t = eat_comma(t);

				/*****************************
				 ** Read input derivations. **
				 *****************************/
				t = eat_list(t);
				for (t = eat_tuple(t); ; t = eat_tuple(t)) {
					Input *i = new (Genode::env()->heap()) Input;
					i->path = t; t = eat_string(t);
					t = eat_comma(t);
					t = eat_list(t);
					while (1) {
						Id *id = new (Genode::env()->heap()) Id(t);
						t = eat_string(t);
						i->ids.insert(id);

						if (t[0] != ',') break;
						t = eat_comma(t);
					}
					inputs.insert(i);
					t = end_list(t);
					t = end_tuple(t);

					if (t[0] != ',') break;
					t = eat_comma(t);
				}
				t = end_list(t);
				t = eat_comma(t);

				/*************************
				 ** Read simple inputs. **
				 *************************/
				t = eat_list(t);
				while (1) {
					Path *p = new (Genode::env()->heap()) Path(t);
					sources.insert(p);
					t = eat_string(t);

					if (t[0] != ',') break;
					t = eat_comma(t);
				}
				t = end_list(t);
				t = eat_comma(t);


				/********************
				 ** Read platform. **
				 ********************/
				platform = t;
				t = eat_string(t);
				t = eat_comma(t);

				/*******************
				 ** Read builder. **
				 *******************/
				_builder = Path(t);
				t = eat_string(t);
				t = eat_comma(t);


				/****************
				 ** Read args. **
				 ****************/
				t = eat_list(t);
				while (1) {
					Arg *a = new (Genode::env()->heap()) Arg(t);
					args.insert(a);
					t = eat_string(t);

					if (t[0] != ',') break;
					t = eat_comma(t);
				}
				t = end_list(t);
				t = eat_comma(t);

				/***********************
				 ** Read environment. **
				 ***********************/
				t = eat_list(t);
				while (1) {
					Token k, v;
					t = k = eat_tuple(t);
					t = eat_string(t);
					t = v = eat_comma(t);
					t = eat_string(t);

					Env_pair *p =  new (Genode::env()->heap())
						Env_pair { k, v };

					_env.insert(p);

					t = end_tuple(t);
					if (t[0] != ',') break;
					t = eat_comma(t);
				}
				t = end_list(t);

				t = end_tuple(t);
			}

			~Derivation()
			{
				clear<Output>(outputs);

				while (inputs.first()) {
					Input *i = inputs.first();
					clear<Id>(i->ids);
					inputs.remove(i);
					destroy(Genode::env()->heap(), i);
				}

				clear<Path>(sources);
				clear<Arg>(args);
				clear<Env_pair>(_env);
			}

			char const *builder()
			{
				return _builder.string();
			}

			char const *path()
			{
				Output *o = outputs.first();
				return o->path.string();
			}

			Env_pair *env() { return _env.first(); }
	};

};

#endif
