#include <ffat/block.h>
#include <base/heap.h>
#include <libc/component.h>

extern int main (int argc, char* argv[]);

void Libc::Component::construct(Libc::Env &env)
{
	env.exec_static_constructors();

	Genode::Heap heap(env.ram(), env.rm());
	Ffat::block_init(env, heap);

	int r = main(0, 0);
	env.parent().exit(r);
}
