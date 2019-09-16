#ifndef FAWKES_GOLOGPP_BLACKBOARD_H_
#define FAWKES_GOLOGPP_BLACKBOARD_H_

#include <aspect/blackboard.h>
#include <aspect/blocked_timing.h>
#include <aspect/configurable.h>
#include <aspect/logging.h>
#include <blackboard/interface_listener.h>
#include <blackboard/interface_observer.h>
#include <core/threading/thread.h>
#include <golog++/model/action.h>
#include <golog++/model/execution.h>

namespace gologpp {
class Type;
}

namespace fawkes_gpp {

///////////////////////////////////////////////////////////////////////////////
class ConfigError : public fawkes::Exception
{
public:
	ConfigError(const std::string &);
};

///////////////////////////////////////////////////////////////////////////////
class ExogManager : public fawkes::BlackBoardAspect,
                    public fawkes::ConfigurableAspect,
                    public fawkes::LoggingAspect,
                    public fawkes::Thread,
                    public fawkes::BlockedTimingAspect
{
public:
	ExogManager(gologpp::ExecutionContext &ctx);

	virtual void init() override;
	virtual void finalize() override;

	void exog_queue_push(gologpp::shared_ptr<gologpp::ExogEvent>);

	static const std::string cfg_prefix;

private:
	gologpp::shared_ptr<gologpp::ExogAction> find_mapped_exog(const std::string &mapped_name);
	gologpp::ExecutionContext &              golog_exec_ctx_;

	///////////////////////////////////////////////////////////////////
	class BlackboardEventHandler
	{
	public:
		BlackboardEventHandler(fawkes::BlackBoard *,
		                       gologpp::shared_ptr<gologpp::ExogAction>,
		                       ExogManager &exog_mgr);

		gologpp::shared_ptr<gologpp::ExogEvent> make_exog_event(fawkes::Interface *) const;

	protected:
		fawkes::BlackBoard *                              blackboard_;
		gologpp::shared_ptr<gologpp::ExogAction>          target_exog_;
		std::unordered_map<std::string, gologpp::arity_t> fields_order_;
		ExogManager &                                     exog_manager_;
	};

	///////////////////////////////////////////////////////////////////
	class InterfaceWatcher : public BlackboardEventHandler, public fawkes::BlackBoardInterfaceListener
	{
	public:
		InterfaceWatcher(fawkes::BlackBoard *,
		                 const std::string &id,
		                 gologpp::shared_ptr<gologpp::ExogAction>,
		                 ExogManager &exog_mgr);
		virtual ~InterfaceWatcher() override;

		virtual void bb_interface_data_changed(fawkes::Interface *) throw() override;

	private:
		fawkes::Interface *      iface_;
		std::vector<std::string> fields_ordered_;
	};

	//////////////////////////////////////////////////////////////////
	class PatternObserver : public BlackboardEventHandler, public fawkes::BlackBoardInterfaceObserver
	{
	public:
		PatternObserver(fawkes::BlackBoard *,
		                const std::string &pattern,
		                gologpp::shared_ptr<gologpp::ExogAction>,
		                ExogManager &exog_mgr);
		virtual ~PatternObserver() override;

		virtual void bb_interface_created(const char *type, const char *id) throw() override;

	private:
		std::string pattern_;
	};

	//////////////////////////////////////////////////////////////////
	std::unordered_map<std::string, gologpp::shared_ptr<gologpp::ExogAction>> mapped_exogs_;
	std::vector<InterfaceWatcher>                                             watchers_;
	std::vector<PatternObserver>                                              observers_;
};

} // namespace fawkes_gpp

#endif
