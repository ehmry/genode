#ifndef _INCLUDE__NIM__REPORT_H_
#define _INCLUDE__NIM__REPORT_H_

#include <report_session/connection.h>
#include <base/attached_dataspace.h>
#include <util/reconstructible.h>

namespace Nim {
	class _ReportConnection;
	typedef Genode::Constructible<_ReportConnection> ReportConnection;
}

class Nim::_ReportConnection
{
	private:

		Report::Connection _report;

		Genode::Attached_dataspace _report_ds;

	public:

		_ReportConnection(Genode::Env &env, char const *label, Genode::size_t buffer_size)
		: _report(env, label, buffer_size), _report_ds(env.rm(), _report.dataspace()) { }

		void submit(char const *data, Genode::size_t len)
		{
			len = Genode::min(len, _report_ds.size());
			Genode::memcpy(_report_ds.local_addr<char>(), data, len);
			_report.submit(len);
		}
};

#endif
