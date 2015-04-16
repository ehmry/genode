/*
 * \brief  Nichts_store realise test client
 * \author Emery Hemingway
 * \date   2015-03-13
 */


/* Genode includes */
#include <nichts_store_session/connection.h>
#include <base/allocator_avl.h>
#include <base/printf.h>
#include <os/config.h>


int main(int, char **)
{
	Genode::Allocator_avl    tx_alloc(Genode::env()->heap());
	Nichts_store::Connection store();

	Genode::config()->xml_node().for_each_sub_node("drv", [&] (Genode::Xml_node drv_node) {
		char path[Nichts_store::MAX_PATH_LEN];
		drv_node.value(path, sizeof(path));

		try {
			store.realise(path);
		}
		catch (Nichts_store::Invalid_derivation) {
			PERR("%s was invalid", path);
		}
		catch (Nichts_store::Build_timeout) {
			PERR("%s timed out", path);
		}
		catch (Nichts_store::Build_failure) {
			PERR("%s failed to build", path);
		}
	});
}