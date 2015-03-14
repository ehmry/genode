/*
 * \brief  Nichts_store realise test client
 * \author Emery Hemingway
 * \date   2015-03-13
 */


/* Genode includes */
#include <base/printf.h>
#include <nichts_store_session/connection.h>
#include <os/config.h>


int main(int, char **)
{
	Nichts_store::Connection srv;

	Genode::config()->xml_node().for_each_sub_node("drv", [&] (Genode::Xml_node drv_node) {
		char path[Nichts_store::MAX_PATH_LEN];

		drv_node.value(path, sizeof(path));
		srv.realise(path);
	});
}