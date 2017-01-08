/*
 * \brief  Component that manages routing of Block service
 * \author Emery Hemingway
 * \date   2017-01-07
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <timer_session/connection.h>
#include <os/timer.h>
#include <os/session_policy.h>
#include <report_session/report_session.h>
#include <base/attached_rom_dataspace.h>
#include <base/attached_ram_dataspace.h>
#include <base/session_state.h>
#include <base/heap.h>
#include <base/component.h>
#include <util/arg_string.h>

namespace Blk_router
{
	using namespace Genode;

	struct Main;

	class Report_session;
	struct Block_session;

	typedef unsigned long Milliseconds;
};


/**
 * Report session implementation
 */
class Blk_router::Report_session : public Genode::Rpc_object<Report::Session>,
                                   public Parent::Server
{
	private:

		Signal_transmitter                &_request_handler;
		Attached_ram_dataspace             _report_ds;
		Session_label               const  _label;
		Id_space<Parent::Server>::Element  _server_id;

		size_t _content_length = 0;

	public:

		Report_session(Genode::Env               &env,
		               Id_space<Parent::Server>  &server_space,
		               Parent::Server::Id  const  server_id,
		               Session_state::Args const &args,
		               size_t              const  buffer_size,
		               Signal_transmitter        &request_handler)
		:
			_request_handler(request_handler),
			_report_ds(env.ram(), env.rm(), buffer_size),
			_label(label_from_args(args.string()).prefix()),
			_server_id(*this, server_space, server_id)
		{ }

		Xml_node xml() const {
			return Xml_node(_report_ds.local_addr<char const>(), _content_length); }

		Session_label const &label() const {
			return _label; }

		/**********************
		 ** Report interface **
		 **********************/

		Dataspace_capability dataspace() override {
			return _report_ds.cap(); }

		void submit(size_t length) override
		{
			_content_length = min(length, _report_ds.size());

			_request_handler.submit();
			/* parse requests after returning to entrypoint */
		}

		void response_sigh(Signal_context_capability) override { };

		virtual size_t obtain_response() override {
			return 0; }
};


/**
 * Proxied block session
 */
struct Blk_router::Block_session : Parent::Server
{
	Parent::Client parent_client;

	Id_space<Parent::Client>::Element const client_id;
	Id_space<Parent::Server>::Element const server_id;

	Block_session(Id_space<Parent::Client> &client_space,
	              Id_space<Parent::Server> &server_space,
	              Parent::Server::Id        server_id)
	:
		client_id(parent_client, client_space),
		server_id(*this, server_space, server_id)
	{ }
};


struct Blk_router::Main
{
	Genode::Env &env;

	Attached_rom_dataspace config           { env, "config"           };
	Attached_rom_dataspace session_requests { env, "session_requests" };

	Timer::Connection timer { env };

	Heap heap { env.ram(), env.rm() };

	Sliced_heap report_heap { env.ram(), env.rm() };

	Id_space<Parent::Server> report_space;
	Id_space<Parent::Server> block_space;

	Milliseconds next_timeout = ~0UL;

	bool config_stale = false;

	void handle_config() {
		config_stale = true; }

	void handle_session_request(Parent::Server::Id id, Xml_node request);

	void handle_session_requests()
	{
		if (config_stale) {
			config.update();
			config_stale = false;
		}

		session_requests.update();

		Xml_node const requests = session_requests.xml();

		requests.for_each_sub_node([&] (Xml_node request) {
			Parent::Server::Id id {
				request.attribute_value("id", 0UL) };

			try { handle_session_request(id, request); }
            	    catch (Insufficient_ram_quota) {
			env.parent().session_response(id, Parent::INSUFFICIENT_RAM_QUOTA); }
            	    catch (Insufficient_cap_quota) {
			env.parent().session_response(id, Parent::INSUFFICIENT_CAP_QUOTA); }
            	    catch (Service_denied) {
			env.parent().session_response(id, Parent::SERVICE_DENIED); }
		});
	}

	Signal_handler<Main> config_handler {
		env.ep(), *this, &Main::handle_config };

	Signal_handler<Main> session_request_handler {
		env.ep(), *this, &Main::handle_session_requests };

	Signal_transmitter requests_transmitter { session_request_handler };

	Main(Genode::Env &env) : env(env)
	{
		config.sigh(config_handler);
		session_requests.sigh(session_request_handler);
		timer.sigh(session_request_handler);
	}

	Session_label lookup_partition(Session_label const &client_label);
};

Genode::Session_label Blk_router::Main::lookup_partition(Genode::Session_label const &client_label)
{
	Session_policy const policy(client_label, config.xml());

	typedef String<37> Guid;
	typedef String<72> Name;
	typedef String<3>  Index;

	Report_session const *found = nullptr;
	Index part_index = 0;

	Milliseconds const time_elapsed = timer.elapsed_ms();

	policy.for_each_sub_node("partition", [&] (Xml_node const &want) {
		if (found) return;

		Milliseconds policy_timeout =
			want.attribute_value("timeout", (Milliseconds)0) * 1000;

		if (policy_timeout > time_elapsed) {

			/* skip this policy until the timeout has passed */
			if (policy_timeout < next_timeout) {
				next_timeout = policy_timeout;
				timer.trigger_once((next_timeout-time_elapsed)*1000);
			}
			return;
		}

		report_space.for_each<Report_session>([&] (Report_session const &report) {
			if (found) return;

			report.xml().for_each_sub_node("partition", [&] (Xml_node const &have) {

				if (want.has_attribute("unique") && have.has_attribute("unique")) {
					if (want.attribute_value("unique",Guid()) == have.attribute_value("unique",Guid())) {
						found = &report;
						part_index = have.attribute_value("index",Index("0"));
						return;
					}
				}

				if (want.has_attribute("name") && have.has_attribute("name")) {

					if (want.attribute_value("name",Name()) == have.attribute_value("name",Name())) {
						found = &report;
						part_index = have.attribute_value("index",Index("0"));
						return;
					}
				}

				if (want.has_attribute("type") && have.has_attribute("type")) {
					if (want.attribute_value("type",Guid()) == have.attribute_value("type",Guid())) {
						found = &report;
						part_index = have.attribute_value("index",Index("0"));
						return;
					}
				}

			});
		});
	});

	return (found)
		? prefixed_label(prefixed_label(found->label(), client_label),part_index)
		: Session_label();
}


void Blk_router::Main::handle_session_request(Parent::Server::Id server_id,
                                             Xml_node request)
{
	if (request.has_type("create")) {

		Session_state::Name name =
			request.attribute_value("service", Session_state::Name());

		if (!request.has_sub_node("args"))
			return;

		typedef Session_state::Args Args;
		Args const args = request.sub_node("args").decoded_content<Args>();

		if (name == "Report") {

			/* read report buffer size from session arguments */
			size_t const buffer_size =
				Arg_string::find_arg(args.string(), "buffer_size").aligned_size();
			size_t const ram_quota =
				Arg_string::find_arg(args.string(), "ram_quota").aligned_size();

			size_t const session_size =
				max(sizeof(Report_session), 4096U) + buffer_size;

			if (ram_quota < session_size) {
				error("insufficient resource donation for report: ",args);
				throw Insufficient_ram_quota();
			}

			Report_session *session = new (report_heap)
				Report_session(env, report_space, server_id,
				               args, buffer_size, requests_transmitter);

			env.parent().deliver_session_cap(server_id, env.ep().manage(*session));
		}

		else if (name == "Block") {
			Session_label client_label(label_from_args(args.string()));

			Session_label new_label = lookup_partition(client_label);
			if (new_label == "")
				return; /* try again later */

			char new_args[Session_state::Args::capacity()];
			strncpy(new_args, args.string(), sizeof(new_args));
			Arg_string::set_arg_string(new_args, sizeof(new_args), "label",
			                           new_label.string());

			Block_session *session = new (heap)
				Block_session(env.id_space(), block_space, server_id);

			/* TODO: asynchronous proxy requests */
			Affinity aff;
			Session_capability cap =
				env.session("Block", session->client_id.id(), new_args, aff);

			env.parent().deliver_session_cap(server_id, cap);
		}
	}

	else if (request.has_type("upgrade")) {
		block_space.apply<Block_session>(server_id, [&] (Block_session &session) {
			size_t ram_quota = request.attribute_value("ram_quota", 0UL);
			char buf[64];
			snprintf(buf, sizeof(buf), "ram_quota=%ld", ram_quota);

			env.upgrade(session.client_id.id(), buf);
		});
		env.parent().session_response(server_id, Parent::SESSION_OK);
	}

	else if (request.has_type("close")) {
		try {
			report_space.apply<Report_session>(server_id, [&] (Report_session &session) {
				destroy(report_heap, &session);
				env.parent().session_response(server_id, Parent::SESSION_CLOSED);
			});
		} catch (Genode::Id_space<Genode::Parent::Server>::Unknown_id) { }
		try {
			block_space.apply<Block_session>(server_id, [&] (Block_session &session) {
				env.close(session.client_id.id());
				destroy(heap, &session);
				env.parent().session_response(server_id, Parent::SESSION_CLOSED);
			});
			return;
		} catch (Genode::Id_space<Genode::Parent::Server>::Unknown_id) { }
	}
}


void Component::construct(Genode::Env &env)
{
	static Blk_router::Main inst(env);
	env.parent().announce("Report");
	env.parent().announce("Block");
}
