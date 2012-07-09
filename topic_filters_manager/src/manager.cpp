#include "topic_filters_manager/manager.hpp"
#include <topic_filters/SetState.h>
#include <topic_filters/SetDividerRate.h>

using namespace topic_filters_manager;

namespace topic_filters_manager
{
	template<> size_t proxy_index<topic_filters::SetState>() 
		{return 0;}
	template<> size_t proxy_index<topic_filters::SetDividerRate>() 
		{return 1;}

	template<> void filter::state<topic_filters::SetState>(
		const topic_filters::SetState& req)
	{
		if (req.request.state)
			state_ = "true";
		else
			state_ = "false";
	}

	template<> void filter::state<topic_filters::SetDividerRate>(
		const topic_filters::SetDividerRate& req)
	{
		state_ = req.request.divider;
	}

	struct switch_filter_handler: public filter_handler
	{
		bool valid(const std::string& ns)
		{
			return ros::service::exists(ns + "/switch_set_state", true) &&
				ros::service::exists(ns + "/set_divider_rate", true);
		}

		void add_proxies(ros::NodeHandle& n,
			const std::string& ns, manager::filter_map_t& map) const
		{
			std::vector<ros::ServiceClient> proxies(2);
			proxies[proxy_index<topic_filters::SetState>()] = 
				n.serviceClient<topic_filters::SetState>(
					ns + "/switch_set_state");
			proxies[proxy_index<topic_filters::SetDividerRate>()] = 
				n.serviceClient<topic_filters::SetDividerRate>(
					ns + "/set_divider_rate");

			manager::filter_t filter("switch_filter", proxies);
			map[ns] = filter;

		}

	};

    struct GenericDividerHandler: public filter_handler
    {
        bool valid(const std::string& ns)
        {
            ros::NodeHandle n("/" + ns);
            ROS_DEBUG("Looking for set_divider_rate in /%s...", ns.c_str());
            std::string service_name = n.resolveName("set_divider_rate");
            ros::service::waitForService(service_name, ros::Duration(5.0));
            return ros::service::exists(service_name, true);
        }

		void add_proxies(ros::NodeHandle&,
			const std::string& ns, manager::filter_map_t& map) const
        {
            std::vector<ros::ServiceClient> proxies(2);
            ros::NodeHandle np("/" + ns);
			proxies[proxy_index<topic_filters::SetDividerRate>()] = 
				np.serviceClient<topic_filters::SetDividerRate>(
					"set_divider_rate", true);

            manager::filter_t filter("GenericDivider", proxies);
            map[ns] = filter;
        }

    };


}

manager::manager()
{
	srv_register_filter_ =
		n_.advertiseService("register_filter", 
			&manager::register_filter_srv, this);
	srv_get_filters_ = 
		n_.advertiseService("get_filters",
			&manager::get_filters_srv, this);

	// Register default handlers.
	handlers_map_["switch_filter"] = new switch_filter_handler();
    handlers_map_["GenericDivider"] = new GenericDividerHandler();

}

manager::~manager()
{
	// Delete handlers.
	handlers_map_t::const_iterator i;
	for (i = handlers_map_.begin(); i != handlers_map_.end(); ++i)
		delete i->second;

}

bool manager::register_filter_srv(RegisterFilter::Request& req, 
	RegisterFilter::Response& res)
{
	register_filter(req.ns, req.type);
	return true;
}

void manager::register_filter(const std::string& ns, const std::string& type)
{
	// Check for an appropriate handler
	const filter_handler* handler = handlers_map_[type];
	if (!handler)
	{
		ROS_ERROR(
			"The %s filter type isn't supported by the topic filter manager.", 
			type.c_str());
		return;
	}

	// Check for the appropriate services in the specified namespace.
	if (!handlers_map_[type]->valid(ns))
	{
		ROS_ERROR("Cannot register filter with namespace %s", ns.c_str());
		return;
	}

	// Create service proxies and add them to the filter map.
	handler->add_proxies(n_, ns, filter_map_);

}

bool manager::get_filters_srv(GetFilters::Request& req,
	GetFilters::Response& res)
{
	res.filters.reserve(filter_map_.size());
	res.types.reserve(filter_map_.size());
	res.states.reserve(filter_map_.size());
	filter_map_t::const_iterator i;
	for (i = filter_map_.begin(); i != filter_map_.end(); ++i)
	{
		const filter_t& filter = i->second;
		res.filters.push_back(i->first);
		res.types.push_back(filter.filter_type());
		res.states.push_back(filter.state());
	}

	return true;

}

