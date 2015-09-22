/*
 * \brief  Libc kernel for main and pthreads user contexts
 * \author Christian Helmuth
 * \author Emery Hemingway
 * \date   2016-01-22
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
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
#include <timer_session/connection.h>
#include <os/timer.h>

/* libc-internal includes */
#include <internal/call_func.h>
#include <base/internal/unmanaged_singleton.h>
#include "vfs_plugin.h"
#include "libc_init.h"
#include "task.h"


namespace Libc {
	class Kernel;
	class Pthreads;
	class Timer;
	class Timeout;
	class Timeout_handler;

	void (*original_call_component_construct)(Genode::Env &);
	void call_component_construct(Genode::Env &env);

	using Microseconds = Genode::Time_source::Microseconds;
}


/* function to notify libc about select() events */
extern void (*libc_select_notify)();


struct Libc::Timer
{
	::Timer::Connection _timer_connection;
	Genode::Timer       _timer;

	Timer(Genode::Env &env)
	:
		_timer_connection(env),
		_timer(_timer_connection, env.ep())
	{ }

	unsigned long curr_time() const
	{
		return _timer.curr_time().value/1000;
	}

	static Microseconds microseconds(unsigned long timeout_ms)
	{
		return Microseconds(1000*timeout_ms);
	}

	static unsigned long max_timeout()
	{
		return Genode::Timer::Microseconds::max().value/1000;
	}
};


struct Libc::Timeout_handler
{
	virtual void handle_timeout() = 0;
};


/*
 * TODO curr_time wrapping
 */
struct Libc::Timeout
{
	Libc::Timer                       &_timer;
	Timeout_handler                   &_handler;
	Genode::One_shot_timeout<Timeout>  _timeout;

	bool          _expired             = true;
	unsigned long _absolute_timeout_ms = 0;

	void _handle(Microseconds now)
	{
		_expired             = true;
		_absolute_timeout_ms = 0;
		_handler.handle_timeout();
	}

	Timeout(Timer &timer, Timeout_handler &handler)
	:
		_timer(timer),
		_handler(handler),
		_timeout(_timer._timer, *this, &Timeout::_handle)
	{ }

	void start(unsigned long timeout_ms)
	{
		unsigned long const now = _timer.curr_time();

		_expired             = false;
		_absolute_timeout_ms = now + timeout_ms;

		_timeout.start(_timer.microseconds(timeout_ms));
	}

	unsigned long duration_left() const
	{
		unsigned long const now = _timer.curr_time();

		return _expired ? 0 : _absolute_timeout_ms - now;
	}
};


struct Libc::Pthreads
{
	struct Pthread : Timeout_handler
	{
		Genode::Lock  lock { Genode::Lock::LOCKED };
		Pthread      *next { nullptr };

		Timeout _timeout;

		Pthread(Timer &timer, unsigned long timeout_ms)
		: _timeout(timer, *this)
		{
			if (timeout_ms > 0)
				_timeout.start(timeout_ms);
		}

		void handle_timeout()
		{
			lock.unlock();
		}
	};

	Genode::Lock  mutex;
	Pthread      *pthreads = nullptr;
	Timer        &timer;


	Pthreads(Timer &timer) : timer(timer) { }

	void resume_all()
	{
		Genode::Lock::Guard g(mutex);

		for (Pthread *p = pthreads; p; p = p->next)
			p->lock.unlock();
	}

	unsigned long suspend_myself(unsigned long timeout_ms)
	{
		Pthread myself { timer, timeout_ms };
		{
			Genode::Lock::Guard g(mutex);

			myself.next = pthreads;
			pthreads    = &myself;
		}
		myself.lock.lock();
		{
			Genode::Lock::Guard g(mutex);

			/* address of pointer to next pthread allows to change the head */
			for (Pthread **next = &pthreads; *next; next = &(*next)->next) {
				if (*next == &myself) {
					*next = myself.next;
					break;
				}
			}
		}

		return timeout_ms > 0 ? myself._timeout.duration_left() : 0;
	}
};


/* internal utility */
static void resumed_callback();
static void suspended_callback();


/**
 * Libc "kernel"
 *
 * This class represents the "kernel" of the libc-based application
 * Blocking and deblocking happens here on libc functions like read() or
 * select(). This combines blocking of the VFS backend and other signal sources
 * (e.g., timers). The libc task runs on the component thread and allocates a
 * secondary stack for the application task. Context switching uses
 * setjmp/longjmp.
 *
 * TODO review doc
 */
struct Libc::Kernel
{
	private:

		Genode::Env &_env;

		Genode::Heap _heap { &_env.ram(), &_env.rm() };

		Vfs_plugin _vfs { _env, _heap };

		jmp_buf _kernel_context;
		jmp_buf _user_context;

		Genode::Thread &_myself { *Genode::Thread::myself() };

		void *_user_stack = {
			_myself.alloc_secondary_stack(_myself.name().string(),
			                              Component::stack_size()) };

		Genode::Reconstructible<Genode::Signal_handler<Kernel>> _resume_main_handler {
			_env.ep(), *this, &Kernel::_resume_main };

		Genode::Reconstructible<Genode::Signal_handler<Kernel>> _vfs_read_ready_handler {
			_env.ep(), *this, &Kernel::_vfs_read_ready };

		void (*_original_suspended_callback)() = nullptr;

		enum State { KERNEL, USER };

		State _state = KERNEL;

		Timer _timer { _env };

		struct Main_timeout : Timeout_handler
		{
			Genode::Signal_context_capability _signal_cap;
			Timeout                           _timeout;

			Main_timeout(Timer &timer)
			: _timeout(timer, *this)
			{ }

			void timeout(unsigned long timeout_ms, Signal_context_capability signal_cap)
			{
				_signal_cap = signal_cap;

				_timeout.start(timeout_ms);
			}

			void handle_timeout()
			{
				/*
				 * XXX I don't dare to call _resume_main() here as this switches
				 * immediately to the user stack, which would result in dead lock
				 * if the calling context holds any lock in the timeout
				 * implementation.
				 */

				Genode::Signal_transmitter(_signal_cap).submit();
			}
		};

		Main_timeout _main_timeout { _timer };

		Pthreads _pthreads { _timer };

		/**
		 * Trampoline to application (user) code
		 *
		 * This function is called by the main thread.
		 */
		static void _user_entry(Libc::Kernel *kernel)
		{
			original_call_component_construct(kernel->_env);

			/* returned from user - switch stack to libc and return to dispatch loop */
			kernel->_switch_to_kernel();
		}

		bool _main_context() const { return &_myself == Genode::Thread::myself(); }

		/**
		 * Utility to switch main context to kernel
		 *
		 * User context must be saved explicitly before this function is called
		 * to enable _switch_to_user() later.
		 */
		void _switch_to_kernel()
		{
			_state = KERNEL;
			_longjmp(_kernel_context, 1);
		}

		/**
		 * Utility to switch main context to user
		 *
		 * Kernel context must be saved explicitly before this function is called
		 * to enable _switch_to_kernel() later.
		 */
		void _switch_to_user()
		{
			_state = USER;
			_longjmp(_user_context, 1);
		}

		/* called from signal handler */
		void _resume_main()
		{
			if (!_main_context() || _state != KERNEL) {
				Genode::error(__PRETTY_FUNCTION__, " called from non-kernel context");
				return;
			}

			if (!_setjmp(_kernel_context))
				_switch_to_user();
		}

		unsigned long _suspend_main(unsigned long timeout_ms)
		{
			if (timeout_ms > 0)
				_main_timeout.timeout(timeout_ms, *_resume_main_handler);

			if (!_setjmp(_user_context))
				_switch_to_kernel();

			return timeout_ms > 0 ? _main_timeout._timeout.duration_left() : 0;
		}

		void _vfs_read_ready()
		{
			if (libc_select_notify)
				libc_select_notify();
			resume_all(); /* FIXME already in libc_select_notify() ? */
		}

	public:

		Kernel(Genode::Env &env) : _env(env)
		{
			_vfs.read_ready_sigh(*_vfs_read_ready_handler);
		}

		~Kernel() { Genode::error(__PRETTY_FUNCTION__, " should not be executed!"); }

		/**
		 * Setup kernel context and run libc application main context
		 *
		 * This function is called by the component thread at component
		 * construction time.
		 */
		void run()
		{
			if (!_main_context() || _state != KERNEL) {
				Genode::error(__PRETTY_FUNCTION__, " called from non-kernel context");
				return;
			}

			/* save continuation of libc kernel (incl. current stack) */
			if (!_setjmp(_kernel_context)) {
				/* _setjmp() returned directly -> switch to user stack and launch component */
				_state = USER;
				call_func(_user_stack, (void *)_user_entry, (void *)this);

				/* never reached */
			}

			/* _setjmp() returned after _longjmp() -> we're done */
		}

		/**
		 * Resume all contexts (main and pthreads)
		 */
		void resume_all()
		{
			Genode::Signal_transmitter(*_resume_main_handler).submit();

			_pthreads.resume_all();
		}

		/**
		 * Suspend this context (main or pthread)
		 */
		unsigned long suspend(unsigned long timeout_ms)
		{
			if (timeout_ms > _timer.max_timeout())
				Genode::warning("libc: limiting exceeding timeout of ",
				                timeout_ms, " ms to maximum of ",
				                _timer.max_timeout(), " ms");

			timeout_ms = min(timeout_ms, _timer.max_timeout());

			return _main_context() ? _suspend_main(timeout_ms)
			                       : _pthreads.suspend_myself(timeout_ms);
		}

		/**
		 * Called from the main context (by fork)
		 */
		void schedule_suspend(void(*original_suspended_callback) ())
		{
			if (_state != USER) {
				Genode::error(__PRETTY_FUNCTION__, " called from non-user context");
				return;
			}

			/*
			 * We hook into suspend-resume callback chain to destruct and
			 * reconstruct parts of the kernel from the context of the initial
			 * thread, i.e., without holding any object locks.
			 */
			_original_suspended_callback = original_suspended_callback;
			_env.ep().schedule_suspend(suspended_callback, resumed_callback);

			if (!_setjmp(_user_context))
				_switch_to_kernel();
		}

		/**
		 * Called from the context of the initial thread (on fork)
		 */
		void entrypoint_suspended()
		{
			_resume_main_handler.destruct();
			_vfs_read_ready_handler.destruct();

			_original_suspended_callback();
		}

		/**
		 * Called from the context of the initial thread (after fork)
		 */
		void entrypoint_resumed()
		{
			_resume_main_handler.construct(_env.ep(), *this, &Kernel::_resume_main);
			_vfs_read_ready_handler.construct(_env.ep(), *this, &Kernel::_vfs_read_ready);
			_vfs.read_ready_sigh(*_vfs_read_ready_handler);

			Genode::Signal_transmitter(*_resume_main_handler).submit();
		}
};


/**
 * Libc kernel singleton
 *
 * The singleton is implemented with the unmanaged-singleton utility
 * inLibc::call_component_construct() to ensure it is never destructed
 * like normal static global objects. Otherwise, the task object may be
 * destructed in a RPC to Rpc_resume, which would result in a deadlock.
 */
static Libc::Kernel *kernel;


/**
 * Main context execution was suspended (on fork)
 *
 * This function is executed in the context of the initial thread.
 */
static void suspended_callback() { kernel->entrypoint_suspended(); }


/**
 * Resume main context execution (after fork)
 *
 * This function is executed in the context of the initial thread.
 */
static void resumed_callback() { kernel->entrypoint_resumed(); }


/*******************
 ** Libc task API **
 *******************/

void Libc::resume_all() { kernel->resume_all(); }


unsigned long Libc::suspend(unsigned long timeout_ms)
{
	return kernel->suspend(timeout_ms);
}


void Libc::schedule_suspend(void (*suspended) ())
{
	kernel->schedule_suspend(suspended);
}


/****************************
 ** Component-startup hook **
 ****************************/

Genode::size_t Component::stack_size() { return 32*1024*sizeof(long); }

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
