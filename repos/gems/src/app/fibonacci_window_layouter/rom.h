#ifndef _INCLUDE__NIM__ROM_H_
#define _INCLUDE__NIM__ROM_H_

#include <rom_session/connection.h>
#include <util/reconstructible.h>

namespace Nim {
	class _RomConnection;
	typedef void (Rom_callback)(Genode::Dataspace_capability);
	typedef Genode::Constructible<_RomConnection> RomConnection;
}

class Nim::_RomConnection
{
	private:

		Genode::Rom_connection _rom;

		Rom_callback *_callback;

		void _handle_rom()
		{
			_rom.update();
			(_callback)(_rom.dataspace());
		}

		Genode::Io_signal_handler<_RomConnection>_handler;

	public:

		_RomConnection(Genode::Env &env, char const *label, Rom_callback *cb)
		: _callback(cb), _rom(env, label), _handler(env.ep(), *this, &_RomConnection::_handle_rom)
		{
			_rom.sigh(_handler);
			(_callback)(_rom.dataspace());
		}
};

#endif
