/*
 * \brief  User-level task based libc
 * \author Christian Helmuth
 * \date   2016-01-22
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/component.h>
#include <base/log.h>
#include <base/thread.h>
#include <base/rpc_server.h>
#include <base/rpc_client.h>
#include <base/heap.h>

/* libc-internal includes */
#include <internal/call_func.h>
#include <base/internal/unmanaged_singleton.h>
#include "vfs_plugin.h"
#include "libc_init.h"
#include "task.h"


namespace Libc {

	class Kernel;
	struct Pthread_tasks;

	void (*original_call_component_construct)(Genode::Env &);
	void call_component_construct(Genode::Env &env);
}


struct Task_resume
{
	GENODE_RPC(Rpc_resume, void, resume);
	GENODE_RPC_INTERFACE(Rpc_resume);
};


Genode::size_t Component::stack_size() { return 32*1024*sizeof(long); }


struct Libc::Pthread_tasks : Libc::Task
{
	Genode::Semaphore sem;


	/********************
	 ** Task interface **
	 ********************/

	void block() override {
		sem.down(); }

	void unblock() override
	{
		if (sem.cnt() < 0)
			sem.up();
	}
};


/**
 * Libc "Kernel"
 *
 * The libc task represents the"kernel" of the libc-based application.
 * Blocking and deblocking happens here on libc functions like read() or
 * select(). This combines blocking of the VFS backend and other signal sources
 * (e.g., timers). The libc task runs on the component thread and allocates a
 * secondary stack for the application task. Context switching uses
 * setjmp/longjmp.
 */
class Libc::Kernel : /*public Genode::Rpc_object<Task_resume, Libc::Task>,*/ public Task
{
	private:

		Genode::Env &_env;

		/* XXX: this heap is only used by the Vfs_plugin */
		Genode::Heap _heap { &_env.ram(), &_env.rm() };

		Vfs_plugin   _vfs  { _env, _heap };

		/**
		 * Application context and execution state
		 */
		bool    _app_runnable = true;
		jmp_buf _app_task;

		Pthread_tasks _pthread_tasks;

		Genode::Thread &_myself = *Genode::Thread::myself();

		void *_app_stack = {
			_myself.alloc_secondary_stack(_myself.name().string(),
			                              Component::stack_size()) };

		/**
		 * Libc context
		 */
		jmp_buf _libc_task;

		/**
		 * Trampoline to application code
		 */
		static void _app_entry(Kernel *);

		/* executed in the context of the main thread */
		static void _resumed_callback();

		/**
		 * Signal handler to switch the signal handling libc task to
		 * the application task, from another thread
		 */
		Genode::Signal_handler<Kernel> _unblock_handler {
			_env.ep(), *this, &Kernel::unblock };

		Genode::Signal_transmitter _unblock_transmitter { _unblock_handler };

		enum State { KERNEL, USER };

		State _state = USER;

	public:

		Kernel(Genode::Env &env) : _env(env) { }

		~Kernel() { Genode::error(__PRETTY_FUNCTION__, " should not be executed!"); }

		Genode::Env & env() { return _env; }

		Task &this_task()
		{
			if (Genode::Thread::myself() == (&_myself))
				return *this;
			else
				return _pthread_tasks;
		}

		void run()
		{
			/* save continuation of libc task (incl. current stack) */
			if (!_setjmp(_libc_task)) {
				/* _setjmp() returned directly -> switch to app stack and launch component */
				call_func(_app_stack, (void *)_app_entry, (void *)this);

				/* never reached */
			}

			/* _setjmp() returned after _longjmp() -> we're done */
		}

		/**
		 * Called from the app context (by fork)
		 */
		void schedule_suspend(void(*suspended_callback) ())
		{
			if (_setjmp(_app_task))
				return;

			_env.ep().schedule_suspend(suspended_callback, _resumed_callback);

			/* switch to libc task, which will return to entrypoint */
			_longjmp(_libc_task, 1);
		}

		/**
		 * Called from the context of the initial thread
		 */
		void resumed()
		{
			/*
			Genode::Capability<Task_resume> cap = _env.ep().manage(*this);
			cap.call<Task_resume::Rpc_resume>();
			_env.ep().dissolve(*this);
			*/
		}


		/********************
		 ** Task interface **
		 ********************/

		void block() override
		{
			if (_state == KERNEL) return;

			_state = KERNEL;
			if (!_setjmp(_app_task))
				_longjmp(_libc_task, 1);
		}

		void unblock() override
		{
			if (_state == USER) return;

			if (Genode::Thread::myself() == (&_myself)) {
				_state = USER;
				if (!_setjmp(_libc_task))
					_longjmp(_app_task, 1);
			} else {
				_unblock_transmitter.submit();
			}
		}
};


/******************************
 ** Libc task implementation **
 ******************************/

void Libc::Kernel::_app_entry(Kernel *kernel)
{
	original_call_component_construct(kernel->_env);

	/* returned from task - switch stack to libc and return to dispatch loop */
	_longjmp(kernel->_libc_task, 1);
}


/**
 * Libc kernel singleton
 *
 * The singleton is implemented with the unmanaged-singleton utility to ensure
 * it is never destructed like normal static global objects. Otherwise, the
 * task object may be destructed in a RPC to Rpc_resume, which would result in
 * a deadlock.
 */
static Libc::Kernel *kernel;


void Libc::Kernel::_resumed_callback() { kernel->resumed(); }


Libc::Task &Libc::this_task() { return kernel->this_task(); }


namespace Libc {

	void schedule_suspend(void (*suspended) ())
	{
		kernel->schedule_suspend(suspended);
	}

	Genode::Env & kernel_env() { return kernel->env(); }

}


/****************************
 ** Component-startup hook **
 ****************************/

/* XXX needs base-internal header? */
namespace Genode { extern void (*call_component_construct)(Genode::Env &); }

void Libc::call_component_construct(Genode::Env &env)
{
	/* pass Genode::Env to libc subsystems that depend on it */
	init_dl(env);

	kernel = unmanaged_singleton<Libc::Kernel>(env);
	kernel->run();
}


static void __attribute__((constructor)) libc_task_constructor(void)
{
	/* hook into component startup */
	Libc::original_call_component_construct = Genode::call_component_construct;
	Genode::call_component_construct        = &Libc::call_component_construct;
}
