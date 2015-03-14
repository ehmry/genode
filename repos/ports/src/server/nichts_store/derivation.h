/*
 * \brief  Aterm formated derivation parsing
 * \author Emery Hemingway
 * \date   2015-03-13
 */

#ifndef _NICHTS_STORE__DERIVATION_H_
#define _NICHTS_STORE__DERIVATION_H_

//#include <nichts_store_session/nichts_store_session.h>

/*Genode includes. */
#include <base/env.h>
#include <util/list.h>
#include <util/string.h>
#include <util/token.h>

#include "aterm_parser.h"


using namespace Aterm;


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

	typedef Genode::List<Aterm::Parser::String> String_list;

	class Derivation {

		public:

			struct Output : Genode::List<Output>::Element
			{
				Parser::String id;
				Parser::String path;
				Parser::String algo;
				Parser::String hash;
			};

			struct Input : Genode::List<Input>::Element
			{
				Nichts_store::Path path;
				String_list        ids;

				~Input()
				{
					while (ids.first()) {
						Parser::String *id = ids.first();
						ids.remove(id);
						destroy(Genode::env()->heap(), id);
					}
				}
			};

			struct Env_pair : Genode::List<Env_pair>::Element
			{
				Parser::String key;
				Parser::String value;

				Env_pair(Parser::String k, Parser::String v)
				: key(k), value(v) { }
			};

		private:

			Genode::List<Output>   _outputs;
			Genode::List<Input>    _inputs;
			String_list            _sources;
			Parser::String         _platform;
			Parser::String         _builder;
			String_list            _args;
			Genode::List<Env_pair> _env;
			char                  *_buf;

		public:

			void load(char const *buf, int len)
			{
				_buf = new (Genode::env()->heap()) char[len];

				memcpy(_buf, buf, len);
				Aterm::Parser parser(_buf, len);

				parser.constructor("Derive", [&]
				{
					parser.list([&]
					{
						parser.tuple([&]
						{
							Output *o = new (Genode::env()->heap()) Output;

							o->id   = parser.string();
							o->path = parser.string();
							o->algo = parser.string();
							o->hash = parser.string();

							_outputs.insert(o);
						});
					});

					parser.list([&]
					{
						parser.tuple([&]
						{
							Input *i = new (Genode::env()->heap()) Input;
							Parser::String p = parser.string();
							i->path = Path(p.start(), p.len());
							parser.list([&] {
								i->ids.insert(new (Genode::env()->heap())
									Parser::String(parser.string()));
							});
							_inputs.insert(i);
						});
					});

					parser.list([&]
					{
						_sources.insert(new (Genode::env()->heap())
							Parser::String(parser.string()));
					});

					_platform = parser.string();
					_builder  = parser.string();
				
					String_list reverse;
					parser.list([&]
					{
						Parser::String *arg = new (Genode::env()->heap())
							Parser::String(parser.string());

						reverse.insert(arg);
					});
					while (Parser::String *arg = reverse.first()) {
						reverse.remove(arg);
						_args.insert(arg);
					}

					parser.list([&]
					{
						parser.tuple([&]
						{
							Env_pair *p = new (Genode::env()->heap())
								Env_pair(parser.string(), parser.string());

							_env.insert(p);
						});
					});
				});
			}

			~Derivation()
			{
				clear<Output>(_outputs);

				while (_inputs.first()) {
					Input *i = _inputs.first();
					clear<Parser::String>(i->ids);
					_inputs.remove(i);
					destroy(Genode::env()->heap(), i);
				}

				clear<Parser::String>(_sources);
				clear<Parser::String>(_args);
				clear<Env_pair>(_env);

				destroy(Genode::env()->heap(), _buf);
			}

			/**
			 * Return the front of the outputs linked list.
			 */
			Output *output() { return _outputs.first(); }

			/**
			 * Return the front of the inputs linked list.
			 */
			Input *input() { return _inputs.first(); }

			/**
			 * Return the front of the sources linked list.
			 */
			Parser::String *source() { return _sources.first(); }

			/**
			 * Return the builder platform.
			 */
			void platform(char *dest, int len)
			{
				_platform.string(dest, len);
			}

			/**
			 * Return the builder executable filename.
			 */
			void builder(char *dest, int len)
			{
				_builder.string(dest, len);
			}

			/**
			 * Return the store path of the first output.
			 */
			void path(char *dest, size_t len)
			{
				Output *o = _outputs.first();
				if (!o) throw Build_failure();
				o->path.string(dest, len);
			}

			/**
			 * Return a pointer to the front of the
			 * builder arguments linked list.
			 */
			Parser::String *arg() { return _args.first(); }

			/**
			 * Return a pointer to the front of the
			 * builder environment linked list.
			 */
			Env_pair *env() { return _env.first(); }

	};

};

#endif
